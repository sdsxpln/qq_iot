#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h> 
#include <time.h>
#include <dirent.h>
#include <sys/time.h>
#include "ring_queue.h"
#include "msg_handle.h"
#include "queue.h"
#include "common.h"
#include "TXIPCAM.h"
#include "monitor_dev.h"
#include "video_stream.h"
#include "fs_managed.h"
#include "muxer_media.h"
#include "media_demuxer_lib.h"
#include "demuxer_media.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"demuxer_media:"



typedef  struct demuxer_data
{
	char data_type;
	char iframe_type;
	unsigned int length;
	unsigned long time_sample;
	unsigned char data[VIDEO_DATA_MAX_SIZE];
}demuxer_data_t;

typedef enum 
{
	DEMUXER_DATA_VIDEO = 0x01,
	DEMUXER_DATA_VOICE,
}DEMUXER_DATA_TYPE_m;


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

	pthread_mutex_t mutex_replay_video;
	pthread_cond_t cond_replay_video;
	ring_queue_t replay_msg_queue_video;
	volatile unsigned int replay_msg_num_video;
	volatile unsigned int need_change_video;
	
	

}demuxer_media_t;



static demuxer_media_t * demuxer_media_handle = NULL;




unsigned long demuxer_get_time(void)
{


#if  0
	int ret = -1;
	struct timespec	ts;
	ret = clock_gettime(CLOCK_MONOTONIC, &ts);
	if(ret == -1)
	{
		dbg_printf("get time fail ! \n");
		return(0);
	}

	return(ts.tv_sec*1000+ts.tv_nsec / 1000000);
#else
		struct timeval tvs;
		unsigned long cur_time = 0;
        gettimeofday(&tvs, NULL);
        cur_time = (tvs.tv_sec)*1000 + (tvs.tv_usec) /1000;
		return(cur_time);

#endif

}



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


static  void demuxer_media_print(char* fmt, ...)
{


}


static long demuxer_media_read(long hFile, void* buf, long size)
{
	return read( hFile, buf, size);
}


static long demuxer_media_write(long hFile, void* buf, long size)
{
    return -1;
}



static long demuxer_media_seek(long hFile, long offset, long whence)
{
    return lseek(hFile, offset, whence);
}


static long demuxer_media_ftell(long hFile)
{
    return lseek(hFile, 0, SEEK_CUR);
}


static long demuxer_media_is_exit(long hFile)
{
    if (hFile <= 0) 
	{
        return 0;
    }
    if (lseek(hFile, 0, SEEK_CUR) < 0)
	{
        return 0;
    }
    return 1;
}


static void * demuxer_media_open(char *file_name, unsigned int start)
{


	dbg_printf("the file name is %s\n",file_name);
	if(NULL ==file_name  || (access(file_name,F_OK) !=0))
	{
		dbg_printf("please check the pamram !\n");
		return(NULL);
	}
    void * hmedia = NULL;
	int ret = -1;
    T_MEDIALIB_DMX_INFO mediaInfo;
    T_MEDIALIB_DMX_OPEN_INPUT open_input;
    T_MEDIALIB_DMX_OPEN_OUTPUT open_output;
    int ifile = open(file_name, O_RDONLY);
    if(ifile < 0)
    {
    	return NULL;
    }
    memset(&open_input, 0, sizeof(T_MEDIALIB_DMX_OPEN_INPUT));
    memset(&open_output, 0, sizeof(T_MEDIALIB_DMX_OPEN_OUTPUT));
	open_input.m_MediaType = MEDIALIB_MEDIA_MP4;
    open_input.m_hMediaSource = (T_S32)ifile;
    open_input.m_CBFunc.m_FunPrintf = (MEDIALIB_CALLBACK_FUN_PRINTF)demuxer_media_print;
    open_input.m_CBFunc.m_FunRead = (MEDIALIB_CALLBACK_FUN_READ)demuxer_media_read;
    open_input.m_CBFunc.m_FunWrite = (MEDIALIB_CALLBACK_FUN_WRITE)demuxer_media_write;
    open_input.m_CBFunc.m_FunSeek = (MEDIALIB_CALLBACK_FUN_SEEK)demuxer_media_seek;
    open_input.m_CBFunc.m_FunTell = (MEDIALIB_CALLBACK_FUN_TELL)demuxer_media_ftell;
    open_input.m_CBFunc.m_FunMalloc = (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
    open_input.m_CBFunc.m_FunFree = (MEDIALIB_CALLBACK_FUN_FREE)free;
    open_input.m_CBFunc.m_FunFileHandleExist = demuxer_media_is_exit;

    hmedia = MediaLib_Dmx_Open(&open_input, &open_output);
    if (AK_NULL == hmedia)
    {
        close(ifile);
		dbg_printf("MediaLib_Dmx_Open is fail!==%d\n",open_output.m_State);
        return NULL;
    }
    ret = MediaLib_Dmx_Start(hmedia, start);
	if(ret < 0)
	{
		dbg_printf("start fail!\n");
		return(NULL);
	}

	
    return (void *)hmedia;
}




int demuxer_push_replay_info(unsigned int play_time, unsigned long long base_time)
{

	int ret = 1;
	int i = 0;
	
	video_replay_info_t * data = NULL;
	demuxer_media_t * handle = (demuxer_media_t*)demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	if(0 == mmc_get_status())return(-1);
	struct  record_file_node  * record_node = NULL;
	tx_history_video_range * range = NULL;
	
	play_time += 8*3600;
	dbg_printf("play_time====%ld\n",play_time);

	tx_history_video_range * cur_range = &handle->cur_file_node.rang;
	if(NULL  != cur_range)
	{
		if(cur_range->start_time <= play_time && cur_range->end_time >= play_time)
		{
			 data = calloc(1,sizeof(*data));
			 if(NULL == data )
			 {
				dbg_printf("calloc is fail!\n");
				return(-1);
			 }
			 asprintf(&data->file_name,"%s",handle->cur_file_node.file_name);
			
			 data->base_time = base_time;
			 data->play_time = play_time;
		}
	}

	if(NULL == data)
	{
		pthread_mutex_lock(&handle->record_file_mutex);
	    TAILQ_FOREACH(record_node,&handle->record_file_queue,links)
	    {
	    	range = (tx_history_video_range *)&record_node->rang;

			dbg_printf("start_time====%ld\n",range->start_time);
			dbg_printf("end_time====%ld\n",range->end_time);
			if(range->start_time <= play_time && range->end_time >= play_time)
			{
				 data = calloc(1,sizeof(*data));
				 if(NULL == data )break;
				 asprintf(&data->file_name,"%s",record_node->file_name);
				
				 data->base_time = base_time;
				 data->play_time = play_time;
				 break;
			}
	    }
		pthread_mutex_unlock(&handle->record_file_mutex);

	}


	if(NULL == data)
	{
		dbg_printf("not find  dest file ! \n");
		return(-1);
	}


	pthread_mutex_lock(&(handle->mutex_replay_video));
	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&handle->replay_msg_queue_video, data);
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
		pthread_mutex_unlock(&(handle->mutex_replay_video));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &handle->replay_msg_num_video;
    	fetch_and_add(task_num, 1);
		handle->need_change_video = 1;
    }
    pthread_mutex_unlock(&(handle->mutex_replay_video));
	pthread_cond_signal(&(handle->cond_replay_video));
	return(0);
}


int demuxer_replay_send_stop(void)
{
	demuxer_media_t * handle = (demuxer_media_t*)demuxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("the handle is null ! \n");
		return (-1);
	}
	pthread_mutex_lock(&(handle->mutex_replay_video));
	handle->need_change_video = 1;
	pthread_mutex_unlock(&(handle->mutex_replay_video));
	

	return(0);


}



static int demuxer_get_data(void * demuxer_handle,demuxer_data_t * data)
{
	if(NULL == demuxer_handle || NULL == data)
	{
		dbg_printf("please check the param!\n");
		return(-1);
	}

	int ret = -1;
	unsigned int size = 0;
	
	unsigned long time_sample = 0;
	char data_type = 0;
	char frame_type = 0;
	T_MEDIALIB_DMX_BLKINFO dmxBlockInfo;
	bzero(&dmxBlockInfo,sizeof(dmxBlockInfo));
	ret = MediaLib_Dmx_GetNextBlockInfo(demuxer_handle,&dmxBlockInfo);
	if(AK_FALSE == ret)
	{
		return(-1);
	}
	size=dmxBlockInfo.m_ulBlkLen;

	//dbg_printf("dmxBlockInfo.m_eBlkType===%d\n",dmxBlockInfo.m_eBlkType);	
	switch (dmxBlockInfo.m_eBlkType)
	{
    	case T_eMEDIALIB_BLKTYPE_VIDEO:
       	data_type = DEMUXER_DATA_VIDEO;
        unsigned char * streamBuf = data->data;
    	unsigned char tmpBuf[64] = {0};
    	unsigned char* framehead = AK_NULL;
    	unsigned char* p = AK_NULL;
    	unsigned char* q = AK_NULL;
    	unsigned long  m = 0;
    	unsigned long n = 0;
    	MediaLib_Dmx_GetVideoFrame(demuxer_handle, (unsigned char*)data->data, (unsigned int *)&size);
        time_sample = *(unsigned int *)(data->data + 4);
		if ((streamBuf[12]&0x1F) == 7)
		{
			framehead = tmpBuf;
			memcpy(tmpBuf, (streamBuf+8), 64);

			tmpBuf[0] &= 0x00;
			tmpBuf[1] &= 0x00;
			tmpBuf[2] &= 0x00;
			m = tmpBuf[3];
			tmpBuf[3] = (tmpBuf[3]&0x00)|0x01;
			
			p = tmpBuf+m+4;
			p[0] &= 0x00;
			p[1] &= 0x00;
			p[2] &= 0x00;
			n = p[3];
			p[3] = (p[3]&0x00)|0x01;

			q = p+n+4;
			q[0] &= 0x00;
			q[1] &= 0x00;
			q[2] &= 0x00;
			q[3] = (q[3]&0x00)|0x01;

			memcpy((streamBuf+8), framehead, 64);
            frame_type = 0;

		}
		else
		{
			streamBuf[8] &= 0x00;
			streamBuf[9] &= 0x00;
			streamBuf[10] &= 0x00;
			streamBuf[11] = ((streamBuf[11]&0x00) | 0x01);
            frame_type = 1;
		}
        break;
        
    case T_eMEDIALIB_BLKTYPE_AUDIO:
        data_type = DEMUXER_DATA_VOICE;
		MediaLib_Dmx_GetAudioPts (demuxer_handle , &time_sample);
		MediaLib_Dmx_GetAudioData(demuxer_handle, (unsigned char*)data->data, size);

        break;
        
    default:
       // dbg_printf("unknow type\n");
        break;
	}

	data->data_type = data_type;
	data->iframe_type = frame_type;
	data->length =size;
	data->time_sample = time_sample;
	return(0);



}
static void * demuxer_replay_pthread(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	
	demuxer_media_t * handle = (demuxer_media_t*)arg;
	int ret = -1;
	int is_run = 1;
	FILE * cur_file;
	int read_counts = 0;
	video_node_header_t data_head = {0};
	video_data_t	data_send = {0};
	voice_data_t	voice_send = {0};
	int time_offset = 0;
	int real_time_offset = 0;
	unsigned int pre_time_stamps = 0;
	unsigned int cur_time_stamps = 0;
	unsigned long time_slip0 = 0;
	unsigned long time_slip1 = 0;
	demuxer_data_t demuxer_out_data;
	video_replay_info_t *	replay_now_video = NULL;
	void * demuxer_handle = NULL;
	unsigned long nGopIndex = 0;
	unsigned long nFrameIndex = 0;
	unsigned long nTotalIndex = 0;
	while(is_run)
	{
        pthread_mutex_lock(&(handle->mutex_replay_video));
        while (0 == handle->replay_msg_num_video)
        {
            pthread_cond_wait(&(handle->cond_replay_video), &(handle->mutex_replay_video));
        }
		ret = ring_queue_pop(&(handle->replay_msg_queue_video), (void **)&replay_now_video);
		pthread_mutex_unlock(&(handle->mutex_replay_video));


		if(ret != 0 || NULL==replay_now_video)continue;

		volatile unsigned int * num = &(handle->replay_msg_num_video);
		fetch_and_sub(num, 1); 

		demuxer_handle = demuxer_media_open(replay_now_video->file_name,0/*replay_now_video->play_time*1000*/);
		if(NULL == demuxer_handle)
		{
			dbg_printf("demuxer_media_open is fail!\n");
			if(NULL != replay_now_video->file_name)
			{
				free(replay_now_video->file_name);
				replay_now_video->file_name = NULL;
			}

			free(replay_now_video);
			replay_now_video = NULL;
			continue;
		}
		else
		{
			dbg_printf("open ok!\n");

		}

		do
		{
		
			ret = demuxer_get_data(demuxer_handle,&demuxer_out_data);
		}while(DEMUXER_DATA_VIDEO != demuxer_out_data.data_type || I_FRAME != demuxer_out_data.iframe_type);


		video_send_video_stop();
		voice_send_net_stop();
		if(DEMUXER_DATA_VIDEO == demuxer_out_data.data_type)
		{

			pre_time_stamps = demuxer_out_data.time_sample;
			cur_time_stamps = replay_now_video->play_time*1000 - replay_now_video->base_time-8*3600*1000;
			if(I_FRAME == demuxer_out_data.iframe_type)
			{
				nGopIndex = (nGopIndex+1)%65535;	
				nFrameIndex = 0;
			}
			else
			{
				nFrameIndex ++;
			}
			nTotalIndex = (nTotalIndex+1)%65535;
			tx_set_video_data(demuxer_out_data.data,demuxer_out_data.length,demuxer_out_data.iframe_type,cur_time_stamps,nGopIndex,nFrameIndex,nTotalIndex,30);
		}
	
		handle->need_change_video = 0;
		while((0 == handle->need_change_video) && (0==ret))
		{
	
			ret = demuxer_get_data(demuxer_handle,&demuxer_out_data);
			if(ret != 0)continue;

			if(DEMUXER_DATA_VIDEO == demuxer_out_data.data_type)
			{

				time_offset = demuxer_out_data.time_sample-pre_time_stamps;
				pre_time_stamps = demuxer_out_data.time_sample;

				if(time_offset > 0)
				{
					usleep(time_offset*1000);  /*yong gettime of day 替代，因为发送音频会消耗时间*/
				}

				
				if(I_FRAME == demuxer_out_data.iframe_type)
				{
					nGopIndex = (nGopIndex+1)%65535;	
					nFrameIndex = 0;
				}
				else
				{
					nFrameIndex ++;
				}
				nTotalIndex = (nTotalIndex+1)%65535;
				cur_time_stamps += time_offset;
				tx_set_video_data(demuxer_out_data.data,demuxer_out_data.length,demuxer_out_data.iframe_type,cur_time_stamps,nGopIndex,nFrameIndex,nTotalIndex,30);
			}
			else if(DEMUXER_DATA_VOICE == 0)
			{
				voice_fill_net_data(demuxer_out_data.data,demuxer_out_data.length);
			}

		}

		
		if(NULL != demuxer_handle)
		{
			MediaLib_Dmx_Stop(demuxer_handle);
			MediaLib_Dmx_Close(demuxer_handle);
			demuxer_handle = NULL;
		}

		if(NULL != replay_now_video->file_name)
		{
			free(replay_now_video->file_name);
			replay_now_video->file_name = NULL;
		}

		free(replay_now_video);
		replay_now_video = NULL;

	}

	return(NULL);

}








int demuxer_media_handle_up(void)
{


	int ret = -1;
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



	ret = ring_queue_init(&demuxer_media_handle->replay_msg_queue_video, 128);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
    pthread_mutex_init(&(demuxer_media_handle->mutex_replay_video), NULL);
    pthread_cond_init(&(demuxer_media_handle->cond_replay_video), NULL);
	demuxer_media_handle->replay_msg_num_video = 0;
	demuxer_media_handle->need_change_video = 0;


	demuxer_media_scan_files();

	pthread_t replay_pthid;
	ret = pthread_create(&replay_pthid,NULL,demuxer_replay_pthread,demuxer_media_handle);
	pthread_detach(replay_pthid);

	
	return(0);
}
