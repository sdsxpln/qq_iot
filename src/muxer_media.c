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
#include "monitor_dev.h"
#include "media_muxer_lib.h"
#include "demuxer_media.h"
#include "muxer_media.h"




#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"muxer_media:"


#define		SYNC_DATA_BUFF_SIZE		(64*1024)


typedef struct data_sync
{
	int fd;
	int need_seek;
	long seek_offset;
	long length;
	unsigned char *buff;
}data_sync_t;



typedef  struct record_node
{

	
	int need_seek;
	long offset_seek;
	long file_size;
	char * file_name;
	unsigned long start_time;
	unsigned long end_time;
	int need_exit;
	void *mux_handle;
	int handle_used;
	int file_fd;
	data_sync_t * data;

	pthread_mutex_t mutex_mux_data;
	pthread_cond_t cond_mux_data;
	volatile unsigned int msg_num_mux_data;

	pthread_mutex_t mutex_write_data;
	ring_queue_t  msg_queue_write_data;
	volatile unsigned int msg_num_write_data;

}record_node_t;




typedef  struct muxer_media
{

	pthread_mutex_t mutex_record;
	pthread_cond_t cond_record;
	ring_queue_t record_msg_queue;
	volatile unsigned int record_msg_num;
	record_node_t node;
	record_node_t *cur_node;

}muxer_media_t;



static muxer_media_t * muxer_media_handle = NULL;



static int mux_push_async_data(void * data);
static void muxer_media_fs_print(char* fmt, ...)
{


}


static long muxer_media_fs_read(long hFile, void* buf, long size)
{
  	return(size);
}


static long muxer_media_fs_isexist(long hFile)
{

    if(lseek64( hFile , 0, SEEK_CUR ) < 0) 
		return (0);
	else
		return(1);
}



static long muxer_media_fs_seek(long hFile, long offset, long whence)
{

	if(NULL == muxer_media_handle)
	{
		dbg_printf("please check the handle !\n");
		return(offset);
	}
	record_node_t * node = &muxer_media_handle->node;
	node->need_seek = 1;
	node->offset_seek = offset;
	return(offset);

}

static long muxer_media_fs_tell(long hFile)
{

	record_node_t * node = &muxer_media_handle->node;
	return(node->offset_seek);

}


static long muxer_media_fs_write(long hFile, void* buf, long size)
{


	int ret = -1;

	if(NULL == muxer_media_handle)
	{
		dbg_printf("please check the handle !\n");
		return(size);
	}
	
	record_node_t * node = &muxer_media_handle->node;
	if(NULL == node->data)
	{
		node->data = calloc(1,sizeof(data_sync_t));	
		if(NULL == node->data)
		{
			dbg_printf("calloc is fail ! \n");
			node->offset_seek += size;
			return(size);
		}
		node->data->fd = hFile;
		node->data->need_seek = 0;
		node->data->seek_offset = 0;
		node->data->length = 0;
		node->data->buff = calloc(1,sizeof(unsigned char)*SYNC_DATA_BUFF_SIZE+1);
		if(NULL == node->data->buff)
		{
			dbg_printf("calloc is fail ! \n");
			if(NULL != node->data)
			{
				free(node->data);
				node->data==NULL;
			}
			node->offset_seek += size;
			return(size);
		}
		
	}

	

	if(1 == node->need_seek)
	{
		node->need_seek = 0;
		
		if((NULL != node->data) && (node->data->length > 0) )
		{
			ret = mux_push_async_data(node->data);
			if(0 == ret)
			{
				node->data = NULL;
			}
			else
			{
				dbg_printf("push fail !\n");
				if(NULL != node->data->buff)
				{
					free(node->data->buff);
					node->data->buff = NULL;
				}

				if(NULL != node->data)
				{
					free(node->data);
					node->data = NULL;
				}
			}	
		}

		
		data_sync_t * ofsset_data = NULL;
		ofsset_data = calloc(1,sizeof(data_sync_t));
		if(NULL == ofsset_data)
		{
			dbg_printf("calloc is fail ! \n");
			node->offset_seek += size;
			return(size);
		}
		
		ofsset_data->fd = hFile;
		ofsset_data->length = size;
		ofsset_data->buff = calloc(1,sizeof(unsigned char)*size+1);
		if(NULL == ofsset_data->buff)
		{
			dbg_printf("calloc is fail ! \n");
			if(NULL != ofsset_data)
			{
				free(ofsset_data);
				ofsset_data==NULL;
			}
			node->offset_seek += size;
			return(size);
		}
		
		memmove(ofsset_data->buff,buf,size);
		ofsset_data->need_seek = 1;
		ofsset_data->seek_offset= node->offset_seek;
		ret = mux_push_async_data(ofsset_data);
		if(0 != ret)
		{

			if(NULL != ofsset_data->buff)
			{
				free(ofsset_data->buff);
				ofsset_data->buff = NULL;
			}

			if(NULL != ofsset_data)
			{
				free(ofsset_data);
				ofsset_data = NULL;	
			}

		}

		ofsset_data = NULL;
		

	}
	else
	{
		if(node->data->length+size < SYNC_DATA_BUFF_SIZE)
		{
			memmove(node->data->buff+node->data->length,buf,size);
			node->data->length += size;
		}
		else
		{
			int size0 = SYNC_DATA_BUFF_SIZE-node->data->length;
			memmove(node->data->buff+node->data->length,buf,size0);
			node->data->length += size0;
			ret = mux_push_async_data(node->data);
			if(0==ret)
			{
				node->data = NULL;
			}
			else
			{
				dbg_printf("pus fail !\n");
				if(NULL != node->data->buff)
				{
					free(node->data->buff);
					node->data->buff = NULL;
				}

				if(NULL != node->data)
				{
					free(node->data);
					node->data = NULL;
				}
				
			}
			

			int size1 = size-size0;
			node->data = calloc(1,sizeof(data_sync_t));	
			if(NULL == node->data)
			{
				dbg_printf("calloc is fail !\n");
				node->offset_seek += size;
				return(size);
			}
			node->data->fd = hFile;
			node->data->length = 0;
			node->data->need_seek = 0;
			node->data->buff = calloc(1,sizeof(unsigned char)*SYNC_DATA_BUFF_SIZE+1);
			if(NULL == node->data->buff)
			{
				dbg_printf("calloc is fail!\n");
				if(NULL != node->data)
				{
					free(node->data);
					node->data = NULL;
				}
				node->offset_seek += size;
				return(size);
				
			}
			memmove(node->data->buff+node->data->length,buf+size0,size1);
			node->data->length += size1;

		}


	}

	node->offset_seek += size;
	return(size);


}



static void* mux_media_handle_open(int file_fd)
{

    T_MEDIALIB_MUX_OPEN_INPUT mux_open_input;
    T_MEDIALIB_MUX_OPEN_OUTPUT mux_open_output;
    T_MEDIALIB_MUX_INFO MuxInfo;
	void * new_handle = NULL;
    memset(&mux_open_input, 0, sizeof(T_MEDIALIB_MUX_OPEN_INPUT));
    mux_open_input.m_MediaRecType   = MEDIALIB_REC_3GP;
    mux_open_input.m_hMediaDest     = (T_S32)file_fd;
    mux_open_input.m_bCaptureAudio  = 1;
    mux_open_input.m_bNeedSYN       = AK_TRUE; 
    mux_open_input.m_bLocalMode     = AK_TRUE; 
    mux_open_input.m_bIdxInMem      = AK_TRUE; 
    mux_open_input.m_ulIndexMemSize = 100*1024;  
    mux_open_input.m_hIndexFile     = (T_S32)(-1);

    //for syn
    mux_open_input.m_ulVFifoSize    = 600*1024; //video fifo size
    mux_open_input.m_ulAFifoSize    = 200*1024; //audio fifo size
    mux_open_input.m_ulTimeScale    = 1000;     //time scale

    // set video open info
    mux_open_input.m_eVideoType     = MEDIALIB_VIDEO_H264;
    mux_open_input.m_nWidth         = 1280;
    mux_open_input.m_nHeight        = 720;
    mux_open_input.m_nFPS           = FRAME_FPS_NUM;
    mux_open_input.m_nKeyframeInterval  = FRAME_GAP_NUM-1;
    
    // set audio open info
    mux_open_input.m_eAudioType         = MEDIALIB_AUDIO_AMR;
    mux_open_input.m_nSampleRate        = 8000;
    mux_open_input.m_nChannels          = 1;
    mux_open_input.m_wBitsPerSample     = 16;
	mux_open_input.m_ulAudioBitrate = 12200;
	mux_open_input.m_ulSamplesPerPack = 160;
	
    mux_open_input.m_CBFunc.m_FunPrintf= (MEDIALIB_CALLBACK_FUN_PRINTF)muxer_media_fs_print;
    mux_open_input.m_CBFunc.m_FunMalloc= (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
    mux_open_input.m_CBFunc.m_FunFree = (MEDIALIB_CALLBACK_FUN_FREE)free;
    mux_open_input.m_CBFunc.m_FunRead= (MEDIALIB_CALLBACK_FUN_READ)muxer_media_fs_read;
    mux_open_input.m_CBFunc.m_FunFileHandleExist = muxer_media_fs_isexist;


    mux_open_input.m_CBFunc.m_FunSeek= (MEDIALIB_CALLBACK_FUN_SEEK)muxer_media_fs_seek;
    mux_open_input.m_CBFunc.m_FunTell = (MEDIALIB_CALLBACK_FUN_TELL)muxer_media_fs_tell;
    mux_open_input.m_CBFunc.m_FunWrite = (MEDIALIB_CALLBACK_FUN_WRITE)muxer_media_fs_write;

    new_handle = MediaLib_Mux_Open(&mux_open_input, &mux_open_output);
	if(AK_NULL == new_handle)
	{
		dbg_printf("MediaLib_Mux_Open is fail! === %d \n",mux_open_output.m_State);
		return(NULL);
	}
 
    if(AK_FALSE == MediaLib_Mux_GetInfo(new_handle, &MuxInfo))
    {
		MediaLib_Mux_Close(new_handle);
		dbg_printf("MediaLib_Mux_GetInfo is fail!\n");
		return(NULL);
    }

    if(AK_FALSE == MediaLib_Mux_Start(new_handle))
    {
		MediaLib_Mux_Close(new_handle);
		dbg_printf("MediaLib_Mux_Start\n");
		return(NULL);
    }

	return(new_handle);

}


static int mux_media_add_audio(void *mux_handle, void *pbuf, unsigned long size, unsigned long timestamp)
{

	int ret = -1;
    T_MEDIALIB_MUX_PARAM mux_param;
    mux_param.m_pStreamBuf = pbuf;
    mux_param.m_ulStreamLen = size;
    mux_param.m_ulTimeStamp = timestamp;
    mux_param.m_bIsKeyframe = 1;
	ret = MediaLib_Mux_AddAudioData(mux_handle,&mux_param);
	if(0 == ret)
	{
		dbg_printf("MediaLib_Mux_AddAudioData is fail!\n");
		return(-1);
	}

    return 0;
}


static int mux_media_add_video(void *mux_handle, void *pbuf, unsigned long size, unsigned long timestamp, int nIsIFrame)
{
    int ret=0;
 
	T_MEDIALIB_MUX_PARAM mux_param;
	mux_param.m_pStreamBuf = pbuf;
	mux_param.m_ulStreamLen = size;
	mux_param.m_ulTimeStamp = timestamp;
	mux_param.m_bIsKeyframe = nIsIFrame;
	ret = MediaLib_Mux_AddVideoData(mux_handle,&mux_param);
	if(0 == ret)
	{
		dbg_printf("MediaLib_Mux_AddVideoData add fail!\n");
		return(-1);
	}
	return ret;
}



static char * mux_media_new_file(record_node_t * dest_node)
{

	int ret = -1;
	if(0 == mmc_get_status())
	{
		dbg_printf("the mmc is out ! \n");
		return(NULL);
	}

	if(NULL == dest_node)
	{
		dbg_printf("please check the param! \n");
		return(NULL);
	}
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

	
	
	unsigned int time_value = (unsigned int)time(NULL);
	dest_node->start_time = time_value;
	char * new_file_name = calloc(1,sizeof(char)*64);
	if(NULL == new_file_name)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}
	snprintf(new_file_name,63,"%s%s%d%s",RECORD_PATH,"/",time_value,RECORD_TMP_FLAG);

	return(new_file_name);
	
fail:

	if(NULL != new_file_name)
	{
		free(new_file_name);
		new_file_name = NULL;
	}

	return(NULL);

}

static int mux_push_async_data(void * data)
{

	int ret = 1;
	int i = 0;
	if(0 == mmc_get_status())
	{
		dbg_printf("the mmc is out !\n");
		return(-1);
	}
	muxer_media_t * handle = (muxer_media_t*)muxer_media_handle;
	if(NULL == handle || NULL== data)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	record_node_t * node = &muxer_media_handle->node;
	pthread_mutex_lock(&(node->mutex_write_data));
	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&(node->msg_queue_write_data), data);
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
		pthread_mutex_unlock(&(node->mutex_write_data));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &node->msg_num_write_data;
    	fetch_and_add(task_num, 1);
    }
    pthread_mutex_unlock(&(node->mutex_write_data));

	return(0);
}



int mux_push_record_data(void * data)
{

	int ret = 1;
	int i = 0;
	if(0 == mmc_get_status())return(-1);

	muxer_media_t * handle = (muxer_media_t*)muxer_media_handle;
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



static void * mux_record_pthread(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	muxer_media_t * handle = (muxer_media_t*)arg;
	
	int ret = -1;
	int is_run = 1;
	void *	record_data = NULL;
	video_data_t * video = NULL;
	voice_data_t * voice = NULL;
	record_node_t * node = &handle->node;
	static int is_first = 0;
	int count = 0;
	while(is_run)
	{

        pthread_mutex_lock(&(handle->mutex_record));
        while (0 == handle->record_msg_num)
        {
            pthread_cond_wait(&(handle->cond_record), &(handle->mutex_record));
        }
		ret = ring_queue_pop(&(handle->record_msg_queue), (void **)&record_data);
		pthread_mutex_unlock(&(handle->mutex_record));
		if(ret != 0 || NULL == record_data)continue;
		volatile unsigned int * num = &(handle->record_msg_num);
		fetch_and_sub(num, 1); 

		if(NULL == handle->cur_node )
		{
			if(0 == node->handle_used)
			{
	
				
				if(NULL != node->file_name)
				{
					free(node->file_name);
					node->file_name = NULL;
				}
				node->file_name = mux_media_new_file(node);
				if(NULL == node->file_name)
				{
					dbg_printf("mux_media_new_file is fail!\n");
					goto out;
				}
					

				
				node->file_fd = open(node->file_name,O_RDWR | O_CREAT | O_TRUNC);
				node->mux_handle =mux_media_handle_open(node->file_fd);
				if(NULL == node->mux_handle)
				{
					dbg_printf("open fail ! \n");
					if(NULL != node->file_name)
					{
						free(node->file_name);
						node->file_name = NULL;
					}
					goto out;
					
				}

				demuxer_init_cur_file();
				demuxer_set_cur_file_name(node->file_name);
				demuxer_set_cur_start_time(node->start_time);
				dbg_printf("the file name is %s \n",node->file_name);
				node->need_seek = 0;
				node->offset_seek = 0;
				node->file_size = 0;
				node->need_exit = 0;
				node->handle_used = 1;
				node->data = NULL;
				handle->cur_node = node;
				is_first = 0;
				
			}

		}
		
out:
		if(RECORD_VIDEO_DATA == *(record_data_type_t*)record_data)
		{
			video = (video_data_t*)record_data;
			
			if(NULL != handle->cur_node)
			{

				if(is_first ==0 && 1 == video->nFrameType)
				{
					video->status = 0;
					continue;
				}
				is_first = 1;
				mux_media_add_video(handle->cur_node->mux_handle,video->data,video->nEncDataLen,video->nTimeStamps,1-video->nFrameType);

			}
			
			
			video->status = 0;	
		}
		else
		{
			voice = (voice_data_t*)record_data;	

			if(NULL != handle->cur_node)
			{
				if(is_first == 1 )
				{
					mux_media_add_audio(handle->cur_node->mux_handle,voice->data,voice->data_length,voice->time_sample);
				}
			}
			
			
			if(NULL != voice)
			{
				free(voice);
				voice = NULL;
			}
		}


		if(NULL != handle->cur_node)
		{
			pthread_mutex_lock(&(handle->cur_node->mutex_mux_data));
			volatile unsigned int *task_num = &handle->cur_node->msg_num_mux_data;
			fetch_and_add(task_num, 1);
			pthread_mutex_unlock(&(handle->cur_node->mutex_mux_data));
			pthread_cond_signal(&(handle->cur_node->cond_mux_data));

			if(handle->cur_node->file_size > 100*1024*1024)
			{
				handle->cur_node->need_exit = 1;
				handle->cur_node = NULL;
				count += 1;
				
			}
		}

	}

	return(NULL);

}


static void * mux_async_pthread(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	
	muxer_media_t * handle = (muxer_media_t*)arg;
	record_node_t * node = &handle->node;
	int ret = -1;
	int is_run = 1;
	int write_length = 0;
	data_sync_t *	async_data = NULL;
	while(is_run)
	{

        pthread_mutex_lock(&(node->mutex_mux_data));
        while ((0 == node->msg_num_mux_data) && (0 == node->need_exit))
        {
            pthread_cond_wait(&(node->cond_mux_data), &(node->mutex_mux_data));
        }
		pthread_mutex_unlock(&(node->mutex_mux_data));
		volatile unsigned int * task_num = &(node->msg_num_mux_data);
		fetch_and_sub(task_num, 1);

		if(NULL != node->mux_handle)
		{
			MediaLib_Mux_Handle(node->mux_handle);
		}
		
			
		if(0 == node->need_exit)
		{

			ret = -1;
	        pthread_mutex_lock(&(node->mutex_write_data));
	        if(node->msg_num_write_data > 0 )
	        {
				ret = ring_queue_pop(&(node->msg_queue_write_data), (void **)&async_data);
	        }
			pthread_mutex_unlock(&(node->mutex_write_data));
			if(ret != 0)continue;

			volatile unsigned int * num = &(node->msg_num_write_data);
			fetch_and_sub(num, 1); 
			if(NULL == async_data)continue;
			
			if(async_data->need_seek==1)
			{
				lseek64(async_data->fd, async_data->seek_offset, 0)	;
			}
			write_length = write(async_data->fd,async_data->buff,async_data->length);
		//	dbg_printf("write_length===%d\n",write_length);
			node->file_size += write_length;
			if(NULL != async_data->buff)
			{
				free(async_data->buff);
				async_data->buff = NULL;
			}
			if(NULL != async_data)
			{
				free(async_data);
				async_data = NULL;
			}
			node->end_time = (unsigned int)time(NULL);
			demuxer_set_cur_end_time(node->end_time);

		}
		else
		{

			MediaLib_Mux_Handle(node->mux_handle);
			MediaLib_Mux_Stop(node->mux_handle);
			MediaLib_Mux_Close(node->mux_handle);
			while(1)
			{
				ret = -1;
		        pthread_mutex_lock(&(node->mutex_write_data));
		        if(node->msg_num_write_data > 0 )
		        {
					ret = ring_queue_pop(&(node->msg_queue_write_data), (void **)&async_data);
		        }
				pthread_mutex_unlock(&(node->mutex_write_data));
				if(ret != 0 && NULL==async_data)break;
				volatile unsigned int * num = &(node->msg_num_write_data);
				fetch_and_sub(num, 1); 

				if(NULL == async_data)continue;
				
				if(async_data->need_seek==1)
				{
					lseek64(async_data->fd, async_data->seek_offset, 0)	;
				}
				write_length = write(async_data->fd,async_data->buff,async_data->length);
				//dbg_printf("write_length===%d\n",write_length);
				node->file_size += write_length;
				if(NULL != async_data->buff)
				{
					free(async_data->buff);
					async_data->buff = NULL;
				}
				if(NULL != async_data)
				{
					free(async_data);
					async_data = NULL;
				}


			}


			dbg_printf("node->msg_num_write_data===%d\n",node->msg_num_write_data);
			node->mux_handle = NULL;
			node->msg_num_mux_data = 0;
		
			node->need_seek = 0;
			node->offset_seek = 0;
			node->need_exit = 0;


			if(NULL != node->data)
			{
				if(NULL != node->data->buff)
				{
					free(node->data->buff);
					node->data->buff = NULL;
				}
				free(node->data);
				node->data = NULL;
			}

			if(node->file_fd > 0)
			{
				close(node->file_fd);
				node->file_fd = -1;
			}
			
			node->end_time = (unsigned int)time(NULL);

			char * p = strstr(node->file_name,RECORD_TMP_FLAG);
			if(NULL != p)
			{
				char new_name[128] = {0};
				char file_name[32] = {0};
				memmove(file_name,node->file_name,p-node->file_name);
				snprintf(new_name,127,"%s%s%d%s",file_name,"_",node->end_time,RECORD_MP4_FLAG);
				ret = rename(node->file_name,new_name);
				if(0 != ret)
				{
					dbg_printf("rename is fail ! \n");
				}
				else
				{
					demuxer_media_add_history(new_name,node->start_time,node->end_time);
					dbg_printf("the new name  is %s\n",new_name);
				}
			}

			node->handle_used = 0;

			dbg_printf("i will exit it 0 !\n");
		}

	}

	return(NULL);

}




int muxer_media_handle_up(void)
{

	int ret = -1;
	int i = 0;
	if(NULL != muxer_media_handle)
	{
		dbg_printf("handle has been init !\n");
		return(-1);
	}

	muxer_media_handle = calloc(1,sizeof(*muxer_media_handle));
	if(NULL == muxer_media_handle)
	{
		dbg_printf("calloc is fail!\n");
		return(-1);
	}

	ret = ring_queue_init(&muxer_media_handle->record_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
    pthread_mutex_init(&(muxer_media_handle->mutex_record), NULL);
    pthread_cond_init(&(muxer_media_handle->cond_record), NULL);
	muxer_media_handle->record_msg_num = 0;



	record_node_t * node = &muxer_media_handle->node;
	bzero(node,sizeof(*node));

	node->file_name = NULL;
	node->file_size = 0;
	node->data = NULL;
	node->need_exit = 0;
	node->mux_handle = NULL;
	node->handle_used = 0;
	
	pthread_mutex_init(&(node->mutex_mux_data), NULL);
	pthread_cond_init(&(node->cond_mux_data), NULL);
	node->msg_num_mux_data = 0;
	
	pthread_mutex_init(&(node->mutex_write_data), NULL);
	ret = ring_queue_init(&node->msg_queue_write_data, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");
	}
	node->msg_num_write_data = 0;




	muxer_media_handle->cur_node = NULL;

	pthread_t async_pthid;
	ret = pthread_create(&async_pthid,NULL,mux_async_pthread,muxer_media_handle);
	pthread_detach(async_pthid);
	pthread_t record_pthid;
	ret = pthread_create(&record_pthid,NULL,mux_record_pthread,muxer_media_handle);
	pthread_detach(record_pthid);

	return(0);
	
}






