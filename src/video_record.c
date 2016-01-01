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
#include "queue.h"
#include "common.h"
#include "TXIPCAM.h"
#include "monitor_dev.h"
#include "video_record.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"video_record:"


struct  record_file_node 
{
	unsigned long  file_name;
    tx_history_video_range rang;
    TAILQ_ENTRY(record_file_node) links;  
};


typedef struct record_video_handle
{

	pthread_mutex_t mutex_record;
	pthread_cond_t cond_record;
	ring_queue_t record_msg_queue;
	volatile unsigned int record_msg_num;
	unsigned int iframe_counts;
	char * record_file_name;

	FILE * index_fd;
	FILE * data_fd;

	pthread_mutex_t     record_file_mutex;
	unsigned int record_file_counts;
	TAILQ_HEAD(,record_file_node)record_file_queue;


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



static int is_digital(const char * file_name)
{
	int i = 0;
	int ret = 1;
	if(NULL == file_name)return (ret);
	for(i=0;i<strlen(file_name);++i)
	{
        if( file_name[i] >= '0' && file_name[i] <='9')
        {
			continue;
		}   
		else
		{
			ret = 0;
			break;
		}
	}
	
	return(ret);
}




void  fetch_history_video(unsigned int last_time, int max_count, int *count, void * range )
{
	record_video_handle * handle = record_handle;
	unsigned int last_time_zone = last_time + 8*3600;
	int count_video = 0;
	tx_history_video_range * range_list = (tx_history_video_range * )range;
	if(NULL == handle)
	{
		dbg_printf("handle has not been init ! \n");
		return;
	}
	
	struct  record_file_node  * record_node = NULL;
	pthread_mutex_lock(&handle->record_file_mutex);
    TAILQ_FOREACH(record_node,&handle->record_file_queue,links)
    {
     	if(record_node->file_name <= last_time_zone)
     	{

			range_list[count_video].start_time = record_node->rang.start_time - 8*3600;
			range_list[count_video].end_time = record_node->rang.end_time - 8*3600;
     		count_video += 1;
			if(count_video >= max_count-1)break;

		}
    }
	pthread_mutex_unlock(&handle->record_file_mutex);
	
	*count  = count_video-1;
	return;
	



}


static void * get_file_info(const char * file_name)
{
	if(NULL == file_name)return(NULL);
	FILE * precord = NULL;
	char buff[64] = {0};
	memset(buff,'\0',64);
	snprintf(buff,64,"%s%s",RECORD_PATH,"/");
	strcat(buff,file_name);
	
	precord = fopen(buff, "r");
	if(NULL == precord)
	{
		dbg_printf("fopen fail ! \n");
		return(NULL);
	}

	unsigned int read_counts = 0;
	unsigned long time_begin = 0;
	unsigned long time_end = 0;
	video_iframe_index_t * iframe = NULL;
	video_iframe_index_t * pre_iframe = NULL;
	char index_buff[DATA_FILE_OFFSET]={0};
	read_counts = fread(index_buff,1,DATA_FILE_OFFSET,precord);

	iframe = (video_iframe_index_t*)index_buff;
	if(0 != strcmp(iframe->magic,RECORD_MAGINC))
	{
		dbg_printf("this is not a right file ! \n");
		goto fail;
	}

//	time_begin = iframe->time_stamp/1000UL;
	time_begin = iframe->time_stamp;

	pre_iframe = iframe;
	iframe += 1;
	while(0 == strcmp(iframe->magic,RECORD_MAGINC))
	{
		pre_iframe = iframe;
		iframe += 1;
	}


//	time_end = pre_iframe->time_stamp/1000UL;
	time_end = pre_iframe->time_stamp;

	struct  record_file_node  * record_node = calloc(1,sizeof(struct  record_file_node));
	if(NULL == record_node)
	{
		dbg_printf("calloc is fail ! \n");
		goto fail;
	}
	
	tx_history_video_range * new_range = &record_node->rang;

	record_node->file_name = atol(file_name);
	new_range->start_time = time_begin;
	new_range->end_time = time_end;

	if(NULL != precord)
	{
		fclose(precord);
		precord = NULL;
	}

	return(record_node);

	
fail:

	if(NULL != precord)
	{
		fclose(precord);
		precord = NULL;
	}

	return(NULL);

}


static int record_scan_files(void)
{
	
	int ret = -1;
	DIR *record_dp = NULL;
	struct dirent * record_dirp = NULL;
	char * ptr = NULL;
	struct  record_file_node * record_node = NULL;
	if(NULL == record_handle)
	{
		dbg_printf("record_handle not been init ! \n");
		return(-1);
	}
	record_dp = opendir(RECORD_PATH);
	if(NULL == record_dp )
	{
		dbg_printf("open the dir fail \n");
		return (-1);
	}
	while((record_dirp =readdir(record_dp)) != NULL )
	{
		if(record_dirp->d_type != DT_REG )continue;
		ret = is_digital(record_dirp->d_name);
		if(0 == ret)continue;

		record_node = get_file_info(record_dirp->d_name);
		if(NULL == record_node)continue;
		TAILQ_INSERT_HEAD(&record_handle->record_file_queue,record_node,links);

		dbg_printf("file_name==%ld start_time==%ld  end_time==%ld \n",record_node->file_name,record_node->rang.start_time,record_node->rang.end_time);
		record_node = NULL;
	}
	
	closedir(record_dp);
	record_dp = NULL;

	return(0);
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

			struct  record_file_node * record_node = calloc(1,sizeof(*record_node));
			if(NULL == record_node)
			{
				dbg_printf("calloc is fail ! \n");
				return(-1);
			}
			tx_history_video_range * new_range = &record_node->rang;
			record_node->file_name = atol(handle->record_file_name);
			new_range->start_time = record_node->file_name;
			new_range->end_time = (unsigned int)time(NULL);
			TAILQ_INSERT_HEAD(&handle->record_file_queue,record_node,links);
	
			
			free(handle->record_file_name);
			handle->record_file_name = NULL;
		}
		handle->record_file_name = 	make_new_record_file();
		if(NULL == handle->record_file_name)return(-1);
		handle->iframe_counts = 0;


		if(NULL != handle->data_fd)
		{
			fclose(handle->data_fd);
			handle->data_fd = NULL;
		}
		if(NULL != handle->index_fd)
		{
			fclose(handle->index_fd);
			handle->index_fd = NULL;
		}
	}

	if(NULL == handle->data_fd)
	{
		memset(buff,'\0',64);
		snprintf(buff,64,"%s%s",RECORD_PATH,"/");
		strcat(buff,handle->record_file_name);
	
		handle->data_fd = fopen(buff,"w+");
		if(NULL == handle->data_fd)return(-1);
		fseek(handle->data_fd,DATA_FILE_OFFSET,SEEK_SET);

	}

	if(NULL == handle->index_fd)
	{
		memset(buff,'\0',64);
		snprintf(buff,64,"%s%s",RECORD_PATH,"/");
		strcat(buff,handle->record_file_name);

		handle->index_fd = fopen(buff,"w+");
		if(NULL == handle->index_fd)return(-1);
		fseek(handle->index_fd,INDEX_FILE_OFFSET,SEEK_SET);
	}



	if(0 == data->nFrameType)
	{
		video_iframe_index_t index;
		memset(&index,'\0',sizeof(index));
		index.offset = ftell(handle->index_fd);
		//index.time_stamp = data->nTimeStamps;
		index.time_stamp = (unsigned int)time(NULL);
		memmove(index.magic,RECORD_MAGINC,strlen(RECORD_MAGINC));
		fwrite((void*)&index,1,sizeof(index),handle->index_fd);
		handle->iframe_counts += 1;
	}

	
	video_node_header_t head = {0};
	head.total_size = sizeof(video_data_t) - (VIDEO_DATA_MAX_SIZE-data->nEncDataLen);
	head.check_flag = data->nTotalIndex+data->nFrameIndex+data->nGopIndex+data->nFrameType;
	fwrite((void*)&head,1,sizeof(head),handle->data_fd);

	fwrite((void*)data,1,head.total_size,handle->data_fd);

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
	record_handle->data_fd = NULL;
	record_handle->index_fd = NULL;
	
	ret = ring_queue_init(&record_handle->record_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
    pthread_mutex_init(&(record_handle->mutex_record), NULL);
    pthread_cond_init(&(record_handle->cond_record), NULL);
	record_handle->record_msg_num = 0;

	pthread_mutex_init(&(record_handle->record_file_mutex), NULL);
	TAILQ_INIT(&record_handle->record_file_queue);

	record_scan_files();
	
	pthread_t record_pthid;
	ret = pthread_create(&record_pthid,NULL,record_record_pthread,record_handle);
	pthread_detach(record_pthid);
	

	
	return(0);
}


