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
#include "video_stream.h"
#include "fs_managed.h"
#include "muxer_media.h"
#include "demuxer_media.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"demuxer_media:"


typedef enum 
{
	RECORD_MP4_FILE = 0x01,
	RECORD_TMP_FILE,
}RECORD_FILE_TYPE;

struct  record_file_node 
{
	char *  file_name;
	unsigned long file_size;
    tx_history_video_range rang;
    TAILQ_ENTRY(record_file_node) links;  
	
};



typedef  struct demuxer_media
{


	pthread_mutex_t     record_file_mutex;
	volatile unsigned int record_file_counts;
	TAILQ_HEAD(record_queue,record_file_node)record_file_queue;
	struct  record_file_node cur_file_node;
	
	

}demuxer_media_t;



static demuxer_media_t * demuxer_media_handle = NULL;





void  demuxer_media_fetch_history(unsigned int last_time, int max_count, int *count, void * range )
{
	demuxer_media_t * handle = demuxer_media_handle;
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
    	tx_history_video_range * rang = (tx_history_video_range * )&record_node->rang;
		
     	if(rang->end_time <= last_time_zone)
     	{

			range_list[count_video].start_time = rang->start_time - 8*3600;
			range_list[count_video].end_time = rang->end_time - 8*3600;
     		count_video += 1;
			if(count_video >= max_count-1)break;

		}
    }
	pthread_mutex_unlock(&handle->record_file_mutex);
	*count = count_video;

	struct  record_file_node  * cur_node = NULL;
	cur_node = &handle->cur_file_node;
	tx_history_video_range * cur_range = &cur_node->rang;
	if(cur_range->end_time > 0)
	{
		range_list[count_video].start_time = cur_range->start_time - 8*3600;
		range_list[count_video].end_time = cur_range->end_time - 8*3600;
		*count  = count_video+1;
	}
	

}

static int demuxer_media_is_digital(const char * file_name)
{
	int i = 0;
	int ret = 1;
	if(NULL == file_name)return (0);
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

static unsigned long demuxer_media_get_file_size(const char * file_path)
{

	int ret = -1;
	if(NULL == file_path)
	{
		dbg_printf("the path is null,please check it ! \n");
		return(0);
	}
	struct stat file_stat;
	memset(&file_stat,0,sizeof(struct stat));
	ret = stat(file_path, &file_stat);
	if(ret < 0 )return(0);
	return(file_stat.st_size);
	
}


static void * demuxer_media_get_file_info(int type,const char * file_name)
{
	if(NULL == file_name)return(NULL);
	FILE * precord = NULL;
	char buff[128] = {0};
	memset(buff,'\0',128);
	snprintf(buff,128,"%s%s",RECORD_PATH,"/");
	strcat(buff,file_name);


	int ret = 1;
	int is_ok = 0;
	unsigned long start_time = 0;
	unsigned long end_time = 0;
	unsigned long file_size = 0;

	
	char buff_iparse0[32] = {0};
	char *ptr_iparse0 = NULL;
	char buff_iparse1[32] = {0};
	char *ptr_iparse1 = NULL;
	if(RECORD_MP4_FILE == type)
	{
		ptr_iparse0 = strchr(file_name,'_');
		if(NULL == ptr_iparse0)
		{
			dbg_printf("not find \n");
			return(NULL);
		}
		memmove(buff_iparse0,file_name,ptr_iparse0-file_name);
		ret = demuxer_media_is_digital(buff_iparse0);
		if(0==ret)
		{
			dbg_printf("is_digital fail !\n");
			return(NULL);
		}

		ptr_iparse1 = strstr(file_name,RECORD_MP4_FLAG);
		if(NULL == ptr_iparse1)
		{
			dbg_printf("not find mp4 flag !\n");
			return(NULL);
		}
		memmove(buff_iparse1,ptr_iparse0+1,(ptr_iparse1-1)-ptr_iparse0);
		ret = demuxer_media_is_digital(buff_iparse1);
		if(0==ret)
		{
			dbg_printf("is_digital fail !==%s\n",buff_iparse1);
			return(NULL);
		}

		start_time = atol(buff_iparse0);
		end_time = atol(buff_iparse1);
		file_size = demuxer_media_get_file_size(buff);
		is_ok = 1;
		

	}
	else if(RECORD_TMP_FILE == type)
	{


	}


	if(0 == is_ok)return(NULL);

	struct  record_file_node  * record_node = calloc(1,sizeof(struct  record_file_node));
	if(NULL == record_node)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}
	tx_history_video_range * new_range = &record_node->rang;
	record_node->file_size = file_size;
	asprintf(&record_node->file_name,"%s",buff);
	new_range->start_time = start_time;
	new_range->end_time = end_time;

	return(record_node);

	
}


static int demuxer_media_scan_files(void)
{

	int ret = -1;
	demuxer_media_t * handle = demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("record_handle not been init ! \n");
		return(-1);
	}
	
	DIR *record_dp = NULL;
	struct dirent * record_dirp = NULL;
	char * ptr = NULL;
	struct  record_file_node * record_node = NULL;
	record_dp = opendir(RECORD_PATH);
	if(NULL == record_dp )
	{
		dbg_printf("open the dir fail \n");
		return (-1);
	}
	while((record_dirp =readdir(record_dp)) != NULL )
	{
		if(record_dirp->d_type != DT_REG )continue;
		
		if(NULL != strstr(record_dirp->d_name,RECORD_MP4_FLAG))
		{

			dbg_printf("the file name is %s \n",record_dirp->d_name);
			record_node = demuxer_media_get_file_info(RECORD_MP4_FILE,record_dirp->d_name);
		}
		else if(NULL != strstr(record_dirp->d_name,RECORD_TMP_FLAG))
		{
			record_node = demuxer_media_get_file_info(RECORD_TMP_FILE,record_dirp->d_name);
		}
		else
		{
			record_node = NULL;	
		}
		
		if(NULL == record_node)continue;

		pthread_mutex_lock(&handle->record_file_mutex);
		TAILQ_INSERT_HEAD(&handle->record_file_queue,record_node,links);
		handle->record_file_counts += 1;
		pthread_mutex_unlock(&handle->record_file_mutex);
		
		#if 1
		struct tm *now;
		now = localtime((time_t*)&record_node->rang.start_time);
		dbg_printf("start_time==%02d-%02d-%02d  %02d:%02d:%02d\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday,now->tm_hour, now->tm_min, now->tm_sec);

		struct tm *now1;
		now1 = localtime((time_t*)&record_node->rang.end_time);
		dbg_printf("end_time==%02d-%02d-%02d  %02d:%02d:%02d\n", now1->tm_year+1900, now1->tm_mon+1, now1->tm_mday,now1->tm_hour, now1->tm_min, now1->tm_sec);
		#endif

		record_node = NULL;
	}
	
	closedir(record_dp);
	record_dp = NULL;

	return(0);
}

int  demuxer_media_add_history(char* file_name, unsigned long start_time, unsigned long end_time)
{
	demuxer_media_t * handle = demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("check the handle!\n");
		return(-1);
	}
	struct  record_file_node  * new_node = calloc(1,sizeof(struct  record_file_node));
	if(NULL == new_node)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}
	tx_history_video_range * new_range = &new_node->rang;
	new_node->file_size = demuxer_media_get_file_size(file_name);
	asprintf(&new_node->file_name,"%s",file_name);
	new_range->start_time = start_time;
	new_range->end_time = end_time;
	pthread_mutex_lock(&handle->record_file_mutex);
	TAILQ_INSERT_HEAD(&handle->record_file_queue,new_node,links);
	handle->record_file_counts += 1;
	pthread_mutex_unlock(&handle->record_file_mutex);
	return(0);

}


int  demuxer_set_cur_start_time(unsigned long start_time)
{
	demuxer_media_t * handle = demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("check the handle!\n");
		return(-1);
	}
	struct  record_file_node  * node = &handle->cur_file_node;
	tx_history_video_range * rang = &node->rang;

	rang->start_time = start_time;
	
	return(0);

}


int  demuxer_set_cur_end_time(unsigned long end_time)
{
	demuxer_media_t * handle = demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("check the handle!\n");
		return(-1);
	}
	struct  record_file_node  * node = &handle->cur_file_node;
	tx_history_video_range * rang = &node->rang;

	rang->end_time = end_time;
	
	return(0);

}


int  demuxer_set_cur_file_name(char * file_name)
{
	demuxer_media_t * handle = demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("check the handle!\n");
		return(-1);
	}
	struct  record_file_node  * node = &handle->cur_file_node;
	if(NULL != node->file_name)
	{
		free(node->file_name);
		node->file_name = NULL;
	}
	asprintf(&node->file_name,"%s",file_name);

	return(0);

}

int  demuxer_init_cur_file(void)
{
	demuxer_media_t * handle = demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("check the handle!\n");
		return(-1);
	}
	struct  record_file_node  * node = &handle->cur_file_node;
	if(NULL != node->file_name)
	{
		free(node->file_name);
		node->file_name = NULL;
	}
	tx_history_video_range * rang = &node->rang;
	rang->end_time = 0;
	rang->start_time = 0;

	


	return(0);

}



int demuxer_media_handle_up(void)
{
	if(NULL != demuxer_media_handle)
	{
		dbg_printf("the handle has been init ! \n");
		return(-1);
	}

	demuxer_media_handle = calloc(1,sizeof(*demuxer_media_handle));
	if(NULL == demuxer_media_handle)
	{
		dbg_printf("calloc is fail!\n");
		return(-1);
	}

	pthread_mutex_init(&(demuxer_media_handle->record_file_mutex), NULL);
	TAILQ_INIT(&demuxer_media_handle->record_file_queue);
	demuxer_media_handle->record_file_counts = 0;

	bzero(&demuxer_media_handle->cur_file_node,sizeof(struct  record_file_node));
	demuxer_media_handle->cur_file_node.file_name = NULL;

	demuxer_media_scan_files();

	



	return(0);
}