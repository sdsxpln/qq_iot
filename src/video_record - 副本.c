#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h> 
#include <time.h>
#include <dirent.h>
#include "ring_queue.h"
#include "msg_handle.h"
#include "common.h"
#include "monitor_dev.h"
#include "video_record.h"




#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"video_record:"



typedef struct record_video_handle
{

	pthread_mutex_t mutex_record;
	pthread_cond_t cond_record;
	ring_queue_t record_msg_queue;
	volatile unsigned int record_msg_num;
	unsigned int iframe_counts;
	char * record_file_name;

	int index_fd;
	int data_fd;

	unsigned int index_offset;
	unsigned int data_offset;
	
}record_video_handle;


static  record_video_handle * record_handle = NULL;



static char * make_new_record_file(void)
{

	int ret = -1;
	if(0 == mmc_get_status())
	{
		dbg_printf("the mmc is out ! \n");
		return(NULL);
	}
	unsigned int time_value = (unsigned int)time(NULL);
	char * new_file_name = calloc(1,sizeof(char)*32);
	if(NULL == new_file_name)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}
	snprintf(new_file_name,31,"%d",time_value);

	DIR *test_dir = NULL;
	if(NULL == (test_dir=opendir(RECORD_PATH)))
	{
		ret = mkdir(RECORD_PATH, 0777); 
		if(0 != ret)
		{
			dbg_printf("mkdir is fail ! \n");
			goto fail;
		}
	}
	else
	{
		closedir(test_dir);
		test_dir = NULL;
	}
	
	int fd = -1;
	char buff[64] = {0};
	memset(buff,'\0',64);
	snprintf(buff,64,"%s%s",RECORD_PATH,"/");
	strcat(buff,new_file_name);
	
	fd = open(buff,O_CREAT | O_TRUNC | O_RDWR,0777);
	if(fd < 0 )
	{
		dbg_printf("open file fail ! \n");
		goto fail;
	}
	else
	{
		close(fd);
		fd = -1;
	}

	return(new_file_name);
	
fail:

	if(NULL != new_file_name)
	{
		free(new_file_name);
		new_file_name = NULL;
	}

	return(NULL);

}


int  record_process_data(video_data_t * data)
{
	if(0 == mmc_get_status())
	{
		dbg_printf("the mmc is out ! \n");
		return(-1);
	}
	char buff[64] = {0};
	record_video_handle * handle = (record_video_handle*)record_handle;
	if(NULL == handle)
	{
		dbg_printf("handle is not init ! \n");
		return(-1);
	}
	if(NULL == handle->record_file_name || handle->iframe_counts >= IFRAME_COUNTS_MAX)
	{
		if(NULL != handle->record_file_name)
		{
			free(handle->record_file_name);
			handle->record_file_name = NULL;
		}
		handle->record_file_name = 	make_new_record_file();
		if(NULL == handle->record_file_name)return(-1);
		handle->iframe_counts = 0;

		if(handle->data_fd > 0)
		{
			close(handle->data_fd);
			handle->data_fd = -1;
		}
		if(handle->index_fd > 0)
		{
			close(handle->index_fd);
			handle->index_fd = -1;
		}
	}

	if(handle->data_fd < 0 )
	{
		memset(buff,'\0',64);
		snprintf(buff,64,"%s%s",RECORD_PATH,"/");
		strcat(buff,handle->record_file_name);
	
		handle->data_fd = open(buff,O_WRONLY);
		if(handle->data_fd < 0)return(-1);
		handle->data_offset = DATA_FILE_OFFSET;
	}

	if(handle->index_fd < 0 )
	{
		memset(buff,'\0',64);
		snprintf(buff,64,"%s%s",RECORD_PATH,"/");
		strcat(buff,handle->record_file_name);

		handle->index_fd = open(buff,O_WRONLY);
		if(handle->index_fd < 0)return(-1);
		handle->index_offset = INDEX_FILE_OFFSET;	
	}

	if(0 == data->nFrameType)
	{
		video_iframe_index_t index;
		memset(&index,'\0',sizeof(index));
		index.offset = handle->data_offset;
		index.time_stamp = data->nTimeStamps;
		memmove(index.magic,RECORD_MAGINC,strlen(RECORD_MAGINC));
		pwrite(handle->index_fd,(void*)&index,sizeof(index),handle->index_offset);
		handle->index_offset += sizeof(index);
		handle->iframe_counts += 1;
	}

	
	video_node_header_t head = {0};
	head.total_size = sizeof(video_data_t) - (VIDEO_DATA_MAX_SIZE-data->nEncDataLen);
	head.check_flag = data->nTotalIndex+data->nFrameIndex+data->nGopIndex+data->nFrameType;
	pwrite(handle->data_fd,(void*)&head,sizeof(head),handle->data_offset);
	handle->data_offset += sizeof(head);

	pwrite(handle->data_fd,(void*)data,head.total_size,handle->data_offset);
	handle->data_offset += head.total_size;
	dbg_printf("handle->data_offset===%ld \n",handle->data_offset);
	
	return(0);
}




int record_push_record_data(void * data )
{

	int ret = 1;
	int i = 0;
	if(0 == mmc_get_status())return(-1);

	record_video_handle * handle = (record_video_handle*)record_handle;
	if(NULL == handle || NULL== data)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	pthread_mutex_lock(&(handle->mutex_record));

	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&handle->record_msg_queue, data);
	    if (ret < 0)
	    {
	        usleep(i);
	        continue;
	    }
	    else
	    {
	        break;
	    }	
	}
    if (ret < 0)
    {
		pthread_mutex_unlock(&(handle->mutex_record));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &handle->record_msg_num;
    	fetch_and_add(task_num, 1);
    }
    pthread_mutex_unlock(&(handle->mutex_record));
	pthread_cond_signal(&(handle->cond_record));
	return(0);
}


static void * record_record_pthread(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	
	record_video_handle * handle = (record_video_handle*)arg;
	int ret = -1;
	int is_run = 1;
	video_data_t *	video = NULL;

	while(is_run)
	{

        pthread_mutex_lock(&(handle->mutex_record));
        while (0 == handle->record_msg_num)
        {
            pthread_cond_wait(&(handle->cond_record), &(handle->mutex_record));
        }
		ret = ring_queue_pop(&(handle->record_msg_queue), (void **)&video);
		pthread_mutex_unlock(&(handle->mutex_record));
		
		volatile unsigned int * num = &(handle->record_msg_num);
		fetch_and_sub(num, 1); 

		if(ret != 0 || NULL == video)continue;

		record_process_data(video);

		video->status = 0;


	}

	return(NULL);


}




int record_start_up(void)
{

	int ret = -1;
	if(NULL != record_handle)
	{
		dbg_printf("record_handle has been init ! \n");
		return(-1);
	}

	record_handle = calloc(1,sizeof(*record_handle));
	if(NULL == record_handle)
	{
		dbg_printf("calloc is fail !\n");
		return(-1);
	}

	record_handle->iframe_counts = 0;
	record_handle->record_file_name = NULL;
	record_handle->data_fd = -1;
	record_handle->index_fd = -1;
	
	ret = ring_queue_init(&record_handle->record_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
    pthread_mutex_init(&(record_handle->mutex_record), NULL);
    pthread_cond_init(&(record_handle->cond_record), NULL);
	record_handle->record_msg_num = 0;

	record_handle->data_offset = 0;
	record_handle->index_offset = 0;

	pthread_t record_pthid;
	ret = pthread_create(&record_pthid,NULL,record_record_pthread,record_handle);
	pthread_detach(record_pthid);
	

	return(0);
}


