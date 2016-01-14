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
#include "dev_camera.h"
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


	long index_file;
	long record_file;


}muxer_media_t;





static muxer_media_t * muxer_media_handle = NULL;



static T_VOID muxer_media_print(char* fmt, ...)
{


}

static T_S32 anyka_fs_read(T_S32 hFile, T_pVOID buf, T_S32 size)
{
    return read(hFile, buf, size);
}



static T_S32 anyka_fs_write(T_S32 hFile, T_pVOID buf, T_S32 size)
{
	return write(hFile, buf, size);
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




#if 1

void* mux_open2(void *mux_input, int record_fd, int index_fd)
{


    T_MEDIALIB_MUX_OPEN_INPUT mux_open_input;
    T_MEDIALIB_MUX_OPEN_OUTPUT mux_open_output;
    T_MEDIALIB_MUX_INFO MuxInfo;

    memset(&mux_open_input, 0, sizeof(T_MEDIALIB_MUX_OPEN_INPUT));
    mux_open_input.m_MediaRecType   = MEDIALIB_REC_3GP;//MEDIALIB_REC_AVI_NORMAL;
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
    mux_open_input.m_eVideoType     = MEDIALIB_VIDEO_H264;//MEDIALIB_VIDEO_H264;
    mux_open_input.m_nWidth         = 1280;//640;
    mux_open_input.m_nHeight        = 720;//480;
    mux_open_input.m_nFPS           = 30 + 0; //考虑到编码帧率可能有时会高于合成帧率，此处多增加几个空帧，避免出现录像时间不准的情况。
    mux_open_input.m_nKeyframeInterval  = 60-1;

    mux_open_input.m_eAudioType         = MEDIALIB_AUDIO_AMR;//MEDIALIB_AUDIO_PCM;
    mux_open_input.m_nSampleRate        = 8000;//8000;
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

   void *midea_handle = MediaLib_Mux_Open(&mux_open_input, &mux_open_output);


	 printf("mux_open_input.m_nFPS == %d  mux_open_input.m_nKeyframeInterval===%d\n",mux_open_input.m_nFPS,mux_open_input.m_nKeyframeInterval);
	 if (AK_NULL == midea_handle)
	 {
		printf("this is wrong !fuck\n");
	 }
	 else
	 {
		printf("this is ok!1dddd2345\n");
	 }

		

	return(NULL);
   
  
    
}
   
    


#endif

static void* mux_media_open(long record_fd, long index_fd)
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
    mux_open_input.m_nFPS           = 10/*FRAME_FPS_NUM + 1*/;
    mux_open_input.m_nKeyframeInterval  = 19/*FRAME_GAP_NUM-1*/;
    
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
	MediaLib_Mux_Handle(mux_handle);
	
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
	MediaLib_Mux_Handle(mux_handle);
	return ret;
}


int mux_push_record_data(void * data )
{

	int ret = 1;
	int i = 0;

//	if(0 == mmc_get_status())return(-1);

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


#if 1




	int index_file = open("/tmp/index1",O_RDWR | O_CREAT | O_TRUNC);

	int record_file = open("/tmp/hello1",O_RDWR | O_CREAT | O_TRUNC);
	mux_open2(NULL,record_file,index_file);
				
//	muxer_media_handle->mux_media_handle = mux_media_open(record_file,index_file);
//	if(NULL == muxer_media_handle->mux_media_handle)
//	{
//		dbg_printf("mux_media_open is fail!\n");
//		return(-1);

//	}


#endif

//	pthread_t muxer_pthid;
//	ret = pthread_create(&muxer_pthid,NULL,mux_record_pthread,muxer_media_handle);
//	pthread_detach(muxer_pthid);



	return(0);
}





