










#if 1
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



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"muxer_media:"


#define  ASYNC_DATA_BUFFER_NUM	(2)
#define	 ASYNC_DATA_BUFFER_SIZE	(128*1024)


typedef enum
{
	ASYNC_FREE,
	ASYNC_USED,
}async_buffer_status_m;

typedef  struct async_node
{
	int file_fd;
	int data_length;
	unsigned char buff_data[ASYNC_DATA_BUFFER_SIZE];
	async_buffer_status_m	buff_status;
}async_node_t;



typedef  struct muxer_media
{
	void * mux_media_handle;
	pthread_mutex_t mutex_record;
	pthread_cond_t cond_record;
	ring_queue_t record_msg_queue;
	volatile unsigned int record_msg_num;


	pthread_mutex_t mutex_async;
	pthread_cond_t cond_async;
	ring_queue_t  msg_queue_async;
	volatile unsigned int msg_num_async;

	int current_async_buff;
	async_node_t async_node_write[ASYNC_DATA_BUFFER_NUM];
	




	int index_file;
	int record_file;


}muxer_media_t;





static muxer_media_t * muxer_media_handle = NULL;



static  unsigned  int offset_anyka = 0;
static  int need_seek = 0;
static  unsigned int seek_offset = 0;
 
static int  is_i_frame = 0;



static int mux_push_async_data(void * data )
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

	pthread_mutex_lock(&(handle->mutex_async));
	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&handle->msg_queue_async, data);
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
		pthread_mutex_unlock(&(handle->mutex_async));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &handle->msg_num_async;
    	fetch_and_add(task_num, 1);
    }
    pthread_mutex_unlock(&(handle->mutex_async));
	pthread_cond_signal(&(handle->cond_async));

	return(0);
}



static T_VOID muxer_media_print(char* fmt, ...)
{


}

static T_S32 anyka_fs_read(T_S32 hFile, T_pVOID buf, T_S32 size)
{

	dbg_printf("i readsize====%ld  hfile==%ld\n",size,hFile);
  //  return read(hFile, buf, size);
  	return(0);
}


#if 0 /*back*/

static T_S32 anyka_fs_write(T_S32 hFile, T_pVOID buf, T_S32 size)
{

	dbg_printf("hFile===%d  size==%d\n",hFile,size);

	return(write(hFile,buf,size));
	
	int i = 0;
	int ret = 0;
	muxer_media_t * handle = (muxer_media_t*)muxer_media_handle;
	async_node_t * async_write = NULL;
	if(NULL == handle  || 0 == size)
	{
		dbg_printf("check the param ! \n");
		return(size);
	}

	dbg_printf("hFile===%d  size==%d\n",hFile,size);
	if(handle->current_async_buff < 0)
	{
		
		for(i=0;i<ASYNC_DATA_BUFFER_NUM;++i)
		{
			async_node_t * async = 	&handle->async_node_write[i];
			if(ASYNC_FREE == async->buff_status)
			{
				async->buff_status = ASYNC_USED;
				async->data_length = 0;
				handle->current_async_buff = i;
				break;
			}
		}

		if(handle->current_async_buff < 0)
		{
			dbg_printf("room1 is not enough!\n");
			return(size);
		}
	}


	async_write = &handle->async_node_write[handle->current_async_buff];
	if(async_write->data_length + size < ASYNC_DATA_BUFFER_SIZE)
	{
		memmove(async_write->buff_data+async_write->data_length,buf,size);
		async_write->data_length += size;
	}
	else
	{
		int size0 = ASYNC_DATA_BUFFER_SIZE-async_write->data_length;
		memmove(async_write->buff_data+async_write->data_length,buf,size0);
		
		async_write->buff_status = ASYNC_USED;
		async_write->data_length += size0;
		async_write->file_fd = hFile; /*!!!*/
		ret = mux_push_async_data(async_write);
		if(0 != ret)
		{
			async_write->buff_status = ASYNC_FREE;
			dbg_printf("mux_push_async_data is fail!\n");
		}
		else
		{
			dbg_printf("mux_push_async_data is ok!\n");
		}
		handle->current_async_buff = -1;


		int size1 = size-size0;
		async_node_t * async1 = NULL;
		for(i=0;i<ASYNC_DATA_BUFFER_NUM;++i)
		{
			async1 = 	&handle->async_node_write[i];
			if(ASYNC_FREE == async1->buff_status)
			{
				async1->buff_status = ASYNC_USED;
				async1->data_length = 0;
				handle->current_async_buff = i;
				break;
			}
		}

		if(handle->current_async_buff < 0)
		{
			dbg_printf("room2 is not enough!\n");
			return(size);
		}
		
		memmove(async1->buff_data+async1->data_length,buf+size0,size1);
		async1->data_length += size1;

	}

	

	return(size);

}

#endif




typedef struct  data_sync
{
	int fd;
	int need_seek;
	unsigned int seek_offset;
	unsigned int length;
	unsigned char *buff;

}data_sync_t;



static T_S32 anyka_fs_write(T_S32 hFile, T_pVOID buf, T_S32 size)
{

	dbg_printf("hFile===%d  size==%d\n",hFile,size);
//	dbg_printf("hfile==%d size==%d \n",hFile,size);
//	return(write(hFile,buf,size));

#if 0
	offset_anyka+= size;
	return(size);
#endif
	if(size>50*1024)
	{
		dbg_printf("this is too big! size===%d\n",size);
		return(-1);
	}
	data_sync_t * data = calloc(1,sizeof(data_sync_t));
	if(NULL == data)
	{
		dbg_printf("calloc is fail ! \n");
		return(-1);
	}
	data->fd = hFile;
	data->length = size;
	data->buff = calloc(1,sizeof(unsigned char)*size);
	memmove(data->buff,buf,size);
	if(need_seek==1)
	{
		
		data->need_seek = 1;
		data->seek_offset= offset_anyka;
		need_seek = 0;
	}
	else
	{
		data->need_seek = -1;	
	}
	mux_push_async_data(data);
	

	
	offset_anyka+= size;
	
	return(size);

}


static T_S32 anyka_fs_seek(T_S32 hFile, T_S32 offset, T_S32 whence)
{

//	dbg_printf("hfile==%d offset==%d whence===%d\n",hFile,offset,whence);
	offset_anyka = offset;
	need_seek = 1;
	return(offset);

	

//    return lseek64(hFile, offset, whence);

}

static T_S32 anyka_fs_tell(T_S32 hFile)
{
    T_S32 ret;
//	dbg_printf("hfile==%d  offset_anyka===%d \n",hFile,offset_anyka);
	return(offset_anyka);

	
  //  ret =  lseek64( hFile , 0, SEEK_CUR );
	
  //  return ret;
}


static T_S32 anyka_fs_isexist(T_S32 hFile)
{

//	dbg_printf("hfile==%d  \n",hFile);
    if(lseek64( hFile , 0, SEEK_CUR ) < 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


static void* mux_media_open(int record_fd, int index_fd)
{

	if(record_fd<0 || index_fd<0)
	{
		dbg_printf("please check the param!\n");
		return(NULL);
	}
	void * new_handle = NULL;
    T_MEDIALIB_MUX_OPEN_INPUT mux_open_input;
    T_MEDIALIB_MUX_OPEN_OUTPUT mux_open_output;
    T_MEDIALIB_MUX_INFO MuxInfo;
    memset(&mux_open_input, 0, sizeof(T_MEDIALIB_MUX_OPEN_INPUT));
    mux_open_input.m_MediaRecType   = MEDIALIB_REC_3GP;
    mux_open_input.m_hMediaDest     = (T_S32)record_fd;
    mux_open_input.m_bCaptureAudio  = 1;
    mux_open_input.m_bNeedSYN       = AK_TRUE;  /*AK_TRUE*/
    mux_open_input.m_bLocalMode     = AK_TRUE;  /*AK_TRUE*/
    mux_open_input.m_bIdxInMem      = AK_TRUE;  /*AK_FALSE*/
    mux_open_input.m_ulIndexMemSize = 100*1024;  
    mux_open_input.m_hIndexFile     = (T_S32)index_fd;

    //for syn
    mux_open_input.m_ulVFifoSize    = 1024*1024; //video fifo size
    mux_open_input.m_ulAFifoSize    = 1024*1024; //audio fifo size
    mux_open_input.m_ulTimeScale    = 1000;     //time scale

    // set video open info
    mux_open_input.m_eVideoType     = MEDIALIB_VIDEO_H264;
    mux_open_input.m_nWidth         = 1280;
    mux_open_input.m_nHeight        = 720;
    mux_open_input.m_nFPS           = FRAME_FPS_NUM + 1;
    mux_open_input.m_nKeyframeInterval  = FRAME_GAP_NUM-1;
    
    // set audio open info

    mux_open_input.m_eAudioType         = MEDIALIB_AUDIO_AMR;
    mux_open_input.m_nSampleRate        = 8000;
    mux_open_input.m_nChannels          = 1;
    mux_open_input.m_wBitsPerSample     = 16;
	mux_open_input.m_ulAudioBitrate = 12200;
	mux_open_input.m_ulSamplesPerPack = 160;
    mux_open_input.m_CBFunc.m_FunPrintf= (MEDIALIB_CALLBACK_FUN_PRINTF)muxer_media_print;
    mux_open_input.m_CBFunc.m_FunMalloc= (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
    mux_open_input.m_CBFunc.m_FunFree = (MEDIALIB_CALLBACK_FUN_FREE)free;
    mux_open_input.m_CBFunc.m_FunRead= (MEDIALIB_CALLBACK_FUN_READ)anyka_fs_read;
    mux_open_input.m_CBFunc.m_FunSeek= (MEDIALIB_CALLBACK_FUN_SEEK)anyka_fs_seek;
    mux_open_input.m_CBFunc.m_FunTell = (MEDIALIB_CALLBACK_FUN_TELL)anyka_fs_tell;
    mux_open_input.m_CBFunc.m_FunWrite = (MEDIALIB_CALLBACK_FUN_WRITE)anyka_fs_write;
    mux_open_input.m_CBFunc.m_FunFileHandleExist = anyka_fs_isexist;

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
	int status = 0;
	status = MediaLib_Mux_Handle(mux_handle);
//	dbg_printf("status=====%d\n",status);
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
	int status = 0;
	status = MediaLib_Mux_Handle(mux_handle);
//	dbg_printf("status=====%d\n",status);
	return ret;
}


int mux_push_record_data(void * data )
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





static int  mux_media_process_data(void * data)
{
	if(0 == mmc_get_status())
	{
		dbg_printf("the mmc is out ! \n");
		return(-1);
	}
	char buff[64] = {0};
	
	muxer_media_t * handle = (muxer_media_t*)muxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("handle is not init ! \n");
		return(-1);
	}

	record_data_type_t record_type = *(record_data_type_t*)data;
	if(RECORD_VIDEO_DATA == record_type)
	{

		video_data_t * video = (video_data_t*)data;


		if(is_i_frame==0 && (1 == video->nFrameType) )
		{
			video->status = 0;
			return(-1);
		}

		is_i_frame +=1;
#if 1
		if(is_i_frame > 180)
		{

			if(NULL != handle->mux_media_handle)
			{	
			//	fsync(handle->index_file);
			//	fsync(handle->record_file);
			//	close(handle->index_file);
			//	close(handle->record_file);
			
				int status = 0;
				do
				{

					status = MediaLib_Mux_Handle(handle->mux_media_handle);
				}while(status ==  2);
				MediaLib_Mux_Stop(handle->mux_media_handle);
		
		    	MediaLib_Mux_Close(handle->mux_media_handle);
				handle->mux_media_handle = NULL;
				return(-1);

			}
			else
			{
				video->status = 0;
				return(-1);
			}
		}
#endif

		mux_media_add_video(handle->mux_media_handle,video->data,video->nEncDataLen,video->nTimeStamps,1-video->nFrameType);
		video->status = 0;
	}
	else
	{
		voice_data_t * voice = (voice_data_t*)data;	

		if(is_i_frame < 180)
		mux_media_add_audio(handle->mux_media_handle,voice->data,voice->data_length,voice->time_sample);
		
		if(NULL != voice)
		{
			free(voice);
			voice = NULL;
		}

	}

#if 0
	pthread_mutex_lock(&(handle->mutex_async));
	volatile unsigned int *task_num = &handle->msg_num_async;
	compare_and_swap(task_num,0, 1);
    pthread_mutex_unlock(&(handle->mutex_async));
	pthread_cond_signal(&(handle->cond_async));
#endif


	return(0);
}


	int count = 0;
/*两个线程进行交互*/
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

	int status = 0;
	int size = 0;
	while(is_run)
	{

        pthread_mutex_lock(&(handle->mutex_record));
        while (0 == handle->record_msg_num)
        {
            pthread_cond_wait(&(handle->cond_record), &(handle->mutex_record));
        }
		ret = ring_queue_pop(&(handle->record_msg_queue), (void **)&record_data);
		pthread_mutex_unlock(&(handle->mutex_record));
		volatile unsigned int * num = &(handle->record_msg_num);
		fetch_and_sub(num, 1); 
		if(ret != 0 || NULL == record_data)continue;

		//mux_media_process_data(record_data);
		if(RECORD_VIDEO_DATA == *(record_data_type_t*)record_data)
		{
			video_data_t * video = (video_data_t*)record_data;
			
			if(count==0 && 0 != video->nFrameType)
			{
				
			}
			else if(count == 301)
			{

				count +=1;
				int status = 0;
				dbg_printf("i will stop it========================================================================= !\n");
				MediaLib_Mux_Stop(handle->mux_media_handle);
		    	MediaLib_Mux_Close(handle->mux_media_handle);
		

			}
			else
			{
				count += 1;

				if(count < 300)
				{
					mux_media_add_video(handle->mux_media_handle,video->data,video->nEncDataLen,video->nTimeStamps,1-video->nFrameType);
					size += video->nEncDataLen;
				}

			}
			
			video->status = 0;
			
			
			
		}
		else
		{
			voice_data_t * voice = (voice_data_t*)record_data;	

				if(count < 300)
				{
					mux_media_add_audio(handle->mux_media_handle,voice->data,voice->data_length,voice->time_sample);
					size += voice->data_length;
				}




			if(NULL != voice)
			{
				free(voice);
				voice = NULL;
			}

		}


#if 0
		if(size < 1024*100)continue;

		dbg_printf("i write it ! -----------------------------------------------------------------------------\n");
		size = 0;


	if(count < 100)
	{
	pthread_mutex_lock(&(handle->mutex_async));
	volatile unsigned int *task_num = &handle->msg_num_async;
	compare_and_swap(task_num,0, 1);
    pthread_mutex_unlock(&(handle->mutex_async));
	pthread_cond_signal(&(handle->cond_async));

	}

#endif

	

		




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
	int ret = -1;
	int is_run = 1;
	int write_length = 0;
	data_sync_t *	async_data = NULL;
	int count_test = 0;
	int status = 0;
	while(is_run)
	{

        pthread_mutex_lock(&(handle->mutex_async));
        while (0 == handle->msg_num_async)
        {
            pthread_cond_wait(&(handle->cond_async), &(handle->mutex_async));
        }
		ret = ring_queue_pop(&(handle->msg_queue_async), (void **)&async_data);
		pthread_mutex_unlock(&(handle->mutex_async));
		volatile unsigned int * num = &(handle->msg_num_async);
		fetch_and_sub(num, 1); 

		if(ret != 0 || NULL == async_data)continue;

		if(async_data->need_seek==1)
		{
			lseek64(async_data->fd, async_data->seek_offset, 0)	;
		}
		write_length = write(async_data->fd,async_data->buff,async_data->length);
		dbg_printf("write_length====%d\n",write_length);

		free(async_data->buff);
		free(async_data);
		
	
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


    pthread_mutex_init(&(muxer_media_handle->mutex_async), NULL);
    pthread_cond_init(&(muxer_media_handle->cond_async), NULL);
	ret = ring_queue_init(&muxer_media_handle->msg_queue_async, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
	muxer_media_handle->msg_num_async = 0;

	
	for(i=0;i<ASYNC_DATA_BUFFER_NUM;++i)
	{
		async_node_t *async = &muxer_media_handle->async_node_write[i];
		async->buff_status = ASYNC_FREE;
		async->data_length = 0;
	}
	muxer_media_handle->current_async_buff = -1;


	muxer_media_handle->index_file = open("/tmp/index2",O_RDWR | O_CREAT | O_TRUNC);
	muxer_media_handle->record_file = open("/mnt/hello2.mp4",O_RDWR | O_CREAT | O_TRUNC);			
	muxer_media_handle->mux_media_handle = mux_media_open(muxer_media_handle->record_file,muxer_media_handle->index_file);
	if(AK_NULL == muxer_media_handle->mux_media_handle)
	{
		dbg_printf("mux_media_open is fail!\n");
		return(-1);

	}
	else
	{
		dbg_printf("this is right56 !\n");
	
	}

	pthread_t muxer_pthid;
	ret = pthread_create(&muxer_pthid,NULL,mux_record_pthread,muxer_media_handle);
	pthread_detach(muxer_pthid);

	pthread_t async_pthid;
	ret = pthread_create(&async_pthid,NULL,mux_async_pthread,muxer_media_handle);
	pthread_detach(async_pthid);


	return(0);
}







#else




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



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"muxer_media:"


typedef  struct muxer_media
{
	void * mux_media_handle;
	pthread_mutex_t mutex_record;
	pthread_cond_t cond_record;
	ring_queue_t record_msg_queue;
	volatile unsigned int record_msg_num;


	int index_file;
	int record_file;


}muxer_media_t;





static muxer_media_t * muxer_media_handle = NULL;


static int  is_i_frame = 0;

static T_VOID muxer_media_print(char* fmt, ...)
{


}

static T_S32 anyka_fs_read(T_S32 hFile, T_pVOID buf, T_S32 size)
{
    return read(hFile, buf, size);
}


unsigned char h264_buff[1024*1024];
int h264_lenght = 0;
T_S32 file_fd_is = 0;
static T_S32 anyka_fs_write(T_S32 hFile, T_pVOID buf, T_S32 size)
{
	dbg_printf("szie===%d hfile===%d\n",size,hFile);

#if 0
	if(file_fd_is != hFile)
	{

		dbg_printf("not ok!hFile===%d\n",hFile);
		write(hFile, buf, size);
	}
	else
	{

		if(h264_lenght > 1024*64)
		{
			write(hFile, h264_buff, h264_lenght);
			h264_lenght = 0;
		}

		
		if(size < 20)
		{

			if(h264_lenght>0)
			{
				write(hFile, h264_buff, h264_lenght);
				h264_lenght = 0;

			}
			write(hFile, buf, size);
		}
		else
		{
			memmove(h264_buff+h264_lenght,buf,size);
			h264_lenght += size;

		}	
	}
	

	return(size);

#else
 return write(hFile, buf, size);
#endif
}


static T_S32 anyka_fs_seek(T_S32 hFile, T_S32 offset, T_S32 whence)
{
    return lseek64(hFile, offset, whence);

}

static T_S32 anyka_fs_tell(T_S32 hFile)
{
    T_S32 ret;
    ret =  lseek64( hFile , 0, SEEK_CUR );
    return ret;
}


static T_S32 anyka_fs_isexist(T_S32 hFile)
{
    if(lseek64( hFile , 0, SEEK_CUR ) < 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


static void* mux_media_open(int record_fd, int index_fd)
{

	if(record_fd<0 || index_fd<0)
	{
		dbg_printf("please check the param!\n");
		return(NULL);
	}
	void * new_handle = NULL;
    T_MEDIALIB_MUX_OPEN_INPUT mux_open_input;
    T_MEDIALIB_MUX_OPEN_OUTPUT mux_open_output;
    T_MEDIALIB_MUX_INFO MuxInfo;
    memset(&mux_open_input, 0, sizeof(T_MEDIALIB_MUX_OPEN_INPUT));
    mux_open_input.m_MediaRecType   = MEDIALIB_REC_3GP;
    mux_open_input.m_hMediaDest     = (T_S32)record_fd;
    mux_open_input.m_bCaptureAudio  = 1;
    mux_open_input.m_bNeedSYN       = AK_TRUE;
    mux_open_input.m_bLocalMode     = AK_TRUE;
    mux_open_input.m_bIdxInMem      = AK_FALSE;
    mux_open_input.m_ulIndexMemSize = 0;
    mux_open_input.m_hIndexFile     = (T_S32)index_fd;

    //for syn
    mux_open_input.m_ulVFifoSize    = 200*1024; //video fifo size
    mux_open_input.m_ulAFifoSize    = 100*1024; //audio fifo size
    mux_open_input.m_ulTimeScale    = 1000;     //time scale

    // set video open info
    mux_open_input.m_eVideoType     = MEDIALIB_VIDEO_H264;
    mux_open_input.m_nWidth         = 1280;
    mux_open_input.m_nHeight        = 720;
    mux_open_input.m_nFPS           = FRAME_FPS_NUM + 1;
    mux_open_input.m_nKeyframeInterval  = FRAME_GAP_NUM-1;
    
    // set audio open info

    mux_open_input.m_eAudioType         = MEDIALIB_AUDIO_AMR;
    mux_open_input.m_nSampleRate        = 8000;
    mux_open_input.m_nChannels          = 1;
    mux_open_input.m_wBitsPerSample     = 16;
	mux_open_input.m_ulAudioBitrate = 12200;
	mux_open_input.m_ulSamplesPerPack = 160;
    mux_open_input.m_CBFunc.m_FunPrintf= (MEDIALIB_CALLBACK_FUN_PRINTF)muxer_media_print;
    mux_open_input.m_CBFunc.m_FunMalloc= (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
    mux_open_input.m_CBFunc.m_FunFree = (MEDIALIB_CALLBACK_FUN_FREE)free;
    mux_open_input.m_CBFunc.m_FunRead= (MEDIALIB_CALLBACK_FUN_READ)anyka_fs_read;
    mux_open_input.m_CBFunc.m_FunSeek= (MEDIALIB_CALLBACK_FUN_SEEK)anyka_fs_seek;
    mux_open_input.m_CBFunc.m_FunTell = (MEDIALIB_CALLBACK_FUN_TELL)anyka_fs_tell;
    mux_open_input.m_CBFunc.m_FunWrite = (MEDIALIB_CALLBACK_FUN_WRITE)anyka_fs_write;
    mux_open_input.m_CBFunc.m_FunFileHandleExist = anyka_fs_isexist;

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
	ret=MediaLib_Mux_Handle(mux_handle);
	
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
	ret = MediaLib_Mux_Handle(mux_handle);
	return ret;
}


int mux_push_record_data(void * data )
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



static int  mux_media_process_data(void * data)
{
	if(0 == mmc_get_status())
	{
		dbg_printf("the mmc is out ! \n");
		return(-1);
	}
	char buff[64] = {0};
	
	muxer_media_t * handle = (muxer_media_t*)muxer_media_handle;
	if(NULL == handle)
	{
		dbg_printf("handle is not init ! \n");
		return(-1);
	}

	record_data_type_t record_type = *(record_data_type_t*)data;
	if(RECORD_VIDEO_DATA == record_type)
	{

		video_data_t * video = (video_data_t*)data;
		if(is_i_frame==0 && (1 == video->nFrameType) )
		{
			video->status = 0;
			return(-1);
		}

		is_i_frame +=1;
		if(is_i_frame > 180)
		{

			if(NULL != handle->mux_media_handle)
			{	
			//	fsync(handle->index_file);
			//	fsync(handle->record_file);
			//	close(handle->index_file);
			//	close(handle->record_file);
				int status = 0;
				do
				{
					status=MediaLib_Mux_Handle(handle->mux_media_handle);
					dbg_printf("status===%d\n",status);
				}while(2==status);
		   		MediaLib_Mux_Stop(handle->mux_media_handle);
		    	MediaLib_Mux_Close(handle->mux_media_handle);
				handle->mux_media_handle = NULL;
				return(-1);

			}
			else
			{
				video->status = 0;
				return(-1);
			}
				


		}
		
		
		mux_media_add_video(handle->mux_media_handle,video->data,video->nEncDataLen,video->nTimeStamps,1-video->nFrameType);
		video->status = 0;


	
		
	}
	else
	{
		voice_data_t * voice = (voice_data_t*)data;	
		mux_media_add_audio(handle->mux_media_handle,voice->data,voice->data_length,voice->time_sample);
		if(NULL != voice)
		{
			free(voice);
			voice = NULL;
		}

	}



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

	while(is_run)
	{

        pthread_mutex_lock(&(handle->mutex_record));
        while (0 == handle->record_msg_num)
        {
            pthread_cond_wait(&(handle->cond_record), &(handle->mutex_record));
        }
		ret = ring_queue_pop(&(handle->record_msg_queue), (void **)&record_data);
		pthread_mutex_unlock(&(handle->mutex_record));
		volatile unsigned int * num = &(handle->record_msg_num);
		fetch_and_sub(num, 1); 
		if(ret != 0 || NULL == record_data)continue;

		
		mux_media_process_data(record_data);

	}

	return(NULL);

}



int muxer_media_handle_up(void)
{

	int ret = -1;
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







	muxer_media_handle->index_file = open("/mnt/index2",O_RDWR | O_CREAT | O_TRUNC);
	muxer_media_handle->record_file = open("/mnt/hello2.mp4",O_RDWR | O_CREAT | O_TRUNC);		
	file_fd_is = muxer_media_handle->record_file;
	muxer_media_handle->mux_media_handle = mux_media_open(muxer_media_handle->record_file,muxer_media_handle->index_file);
	if(AK_NULL == muxer_media_handle->mux_media_handle)
	{
		dbg_printf("mux_media_open is fail!\n");
		return(-1);

	}
	else
	{
		dbg_printf("this is right56 !\n");
	
	}

	pthread_t muxer_pthid;
	ret = pthread_create(&muxer_pthid,NULL,mux_record_pthread,muxer_media_handle);
	pthread_detach(muxer_pthid);
	return(0);
}

























#endif





