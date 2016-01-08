#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h> 
#include "common.h"
#include "video_record.h"
#include "ring_queue.h"
#include "fs_managed.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"fs_managed:"


typedef struct fs_managed_handle
{

	pthread_mutex_t mutex_fs_managed;
	pthread_cond_t cond_fs_managed;
	ring_queue_t fs_managed_msg_queue;
	volatile unsigned int fs_managed_msg_num;

}fs_managed_handle_t;


static  fs_managed_handle_t * fs_managed = NULL;




	

static int fs_managed_push_data(void * data )
{

	int ret = 1;
	int i = 0;
	fs_managed_handle_t * handle = (fs_managed_handle_t*)fs_managed;
	if(NULL == handle || NULL== data)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	pthread_mutex_lock(&(handle->mutex_fs_managed));

	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&handle->fs_managed_msg_queue, data);
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
		pthread_mutex_unlock(&(handle->mutex_fs_managed));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &handle->fs_managed_msg_num;
    	fetch_and_add(task_num, 1);
    }
    pthread_mutex_unlock(&(handle->mutex_fs_managed));
	pthread_cond_signal(&(handle->cond_fs_managed));
	
	return(0);
}



int fs_hangle_file(file_type_m type,fs_action_m cmd,char * file_path)
{

	int ret = -1;
	fs_header_t * new_file = calloc(1,sizeof(fs_header_t)+sizeof(fs_node_t)+1);
	if(NULL == new_file)
	{
		dbg_printf("calloc is fail ! \n");
		return(-1);
	}
	fs_node_t * node = (fs_node_t*)(new_file+1);

	new_file->type = type;
	new_file->cmd = cmd;
	if(NULL != file_path)
	{
		asprintf(&node->file_name,"%s",file_path);	
	}
	else
	{
		node->file_name = NULL;
	}

	ret = fs_managed_push_data(new_file);
	if(ret != 0)
	{
		if(NULL != node->file_name)
		{
			free(node->file_name);
			node->file_name = NULL;
		}
		free(new_file);
		new_file = NULL;
	}
	return(0);
}



static int fs_managed_record_file(void * arg)
{
	fs_header_t * header = (fs_header_t*)arg;	
	fs_node_t * node = (fs_node_t *)(header+1);
	if(FILE_NEW == header->cmd)
	{
		record_manage_files();		
	}
	else if(FILE_DEL == header->cmd)
	{
		if((NULL == node->file_name) || (0 != access(node->file_name,F_OK)))
		{
			dbg_printf("the file is not here ! \n");
			return(-1);
		}
		char buff[128]={0};
		dbg_printf("the file name is %s \n",node->file_name);
		snprintf(buff,128,"rm -rf %s",node->file_name);
		system(buff);
	}

	return(0);
}

static void * fs_managed_pthread(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	
	fs_managed_handle_t * handle = (fs_managed_handle_t*)arg;
	int ret = -1;
	int is_run = 1;
	fs_header_t *	header = NULL;

	while(is_run)
	{

        pthread_mutex_lock(&(handle->mutex_fs_managed));
        while (0 == handle->fs_managed_msg_num)
        {
            pthread_cond_wait(&(handle->cond_fs_managed), &(handle->mutex_fs_managed));
        }
		ret = ring_queue_pop(&(handle->fs_managed_msg_queue), (void **)&header);
		pthread_mutex_unlock(&(handle->mutex_fs_managed));
		volatile unsigned int * num = &(handle->fs_managed_msg_num);
		fetch_and_sub(num, 1); 
		if(ret != 0 || NULL == header)continue;

		switch(header->type)
		{

			case FILE_RECORD:
			{
				fs_managed_record_file(header);
				break;
			}
		
			default:
			{
				dbg_printf("unknow  type ! \n");
				break;
			
			}
		}
		
	
		fs_node_t * node = (fs_node_t *)(header+1);
		if(NULL != node->file_name)
		{
			free(node->file_name);
			node->file_name = NULL;
		}
		free(header);
		header = NULL;

	}

	return(NULL);

}



int fs_handle_managed_up(void)
{

	int ret = -1;
	if(NULL != fs_managed)
	{
		dbg_printf("record_handle has been init ! \n");
		return(-1);
	}

	fs_managed = calloc(1,sizeof(*fs_managed));
	if(NULL == fs_managed)
	{
		dbg_printf("calloc is fail !\n");
		return(-1);
	}

	ret = ring_queue_init(&fs_managed->fs_managed_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
    pthread_mutex_init(&(fs_managed->mutex_fs_managed), NULL);
    pthread_cond_init(&(fs_managed->cond_fs_managed), NULL);
	fs_managed->fs_managed_msg_num= 0;


	pthread_t fs_managed_pthid;
	ret = pthread_create(&fs_managed_pthid,NULL,fs_managed_pthread,fs_managed);
	pthread_detach(fs_managed_pthid);



	
	return(0);
}






