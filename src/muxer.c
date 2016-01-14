#include "common.h"
#include "muxer.h"
#include "system_up.h"



typedef struct _anyka_muxer_handle
{
    long index_fd;
    long record_fd;
    T_VOID *midea_handle;
    pthread_mutex_t     muxer_lock;
}anyka_muxer_handle, *Panyka_muxer_handle;






/**
 * NAME        mux_open
 * @BRIEF	open mux lib
                  
 * @PARAM	T_MUX_INPUT *mux_input,  set mux lib attribute
 			int record_fd, save file descriptor
 			int index_fd, tmp file descriptor
 * @RETURN	void * 
 * @RETVAL	mux handle
 */



static T_VOID muxer_media_print1(char* fmt, ...)
{


}

static T_S32 anyka_fs_read1(T_S32 hFile, T_pVOID buf, T_S32 size)
{
    return read(hFile, buf, size);
}



static T_S32 anyka_fs_write1(T_S32 hFile, T_pVOID buf, T_S32 size)
{
	return write(hFile, buf, size);
}


static T_S32 anyka_fs_seek1(T_S32 hFile, T_S32 offset, T_S32 whence)
{
    return lseek64(hFile, offset, whence);

}

static T_S32 anyka_fs_tell1(T_S32 hFile)
{
    T_S32 ret;
    ret =  lseek64( hFile , 0, SEEK_CUR );
    return ret;
}


static T_S32 anyka_fs_isexist1(T_S32 hFile)
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

void* mux_open1(T_MUX_INPUT *mux_input, int record_fd, int index_fd)
{

    Panyka_muxer_handle panyka_muxer;
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

    mux_open_input.m_CBFunc.m_FunPrintf= (MEDIALIB_CALLBACK_FUN_PRINTF)muxer_media_print1;
    mux_open_input.m_CBFunc.m_FunMalloc= (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
    mux_open_input.m_CBFunc.m_FunFree = (MEDIALIB_CALLBACK_FUN_FREE)free;
    mux_open_input.m_CBFunc.m_FunRead= (MEDIALIB_CALLBACK_FUN_READ)anyka_fs_read1;
    mux_open_input.m_CBFunc.m_FunSeek= (MEDIALIB_CALLBACK_FUN_SEEK)anyka_fs_seek1;
    mux_open_input.m_CBFunc.m_FunTell = (MEDIALIB_CALLBACK_FUN_TELL)anyka_fs_tell1;
    mux_open_input.m_CBFunc.m_FunWrite = (MEDIALIB_CALLBACK_FUN_WRITE)anyka_fs_write1;
    mux_open_input.m_CBFunc.m_FunFileHandleExist = anyka_fs_isexist1;

   void *midea_handle = MediaLib_Mux_Open(&mux_open_input, &mux_open_output);


	 printf("mux_open_input.m_nFPS == %d  mux_open_input.m_nKeyframeInterval===%d\n",mux_open_input.m_nFPS,mux_open_input.m_nKeyframeInterval);
	 if (AK_NULL == midea_handle)
	 {
		printf("this is wrong !fuck\n");
	 }
	 else
	 {
		printf("this is ok!12345\n");
	 }

		

	return(NULL);
   
  
    
}

/**
 * NAME        mux_addAudio
 * @BRIEF	add audio to mux 
                  
 * @PARAM	void *handle,  handle when open 
 			void *pbuf, 	pointer to data
 			unsigned long size, data size
 			unsigned long timestamp, current time stamps
 * @RETURN	int
 * @RETVAL	if return 0 success, otherwise failed
 */

int mux_addAudio(void *handle, void *pbuf, unsigned long size, unsigned long timestamp)
{



    return 0;
}



/**
 * NAME        mux_addAudio
 * @BRIEF	add video to mux 
                  
 * @PARAM	void *handle,  handle when open 
 			void *pbuf, 	pointer to data
 			unsigned long size, data size
 			unsigned long timestamp, current time stamps
 			int nIsIFrame, the number of I frame
 * @RETURN	int
 * @RETVAL	if return 0 success, otherwise failed
 */

int mux_addVideo(void *handle, void *pbuf, unsigned long size, unsigned long timestamp, int nIsIFrame)
{


}

/**
 * NAME        mux_addAudio
 * @BRIEF	close mux 
                  
 * @PARAM	void *handle,  handle when open 

 * @RETURN	int
 * @RETVAL	if return 0 success, otherwise failed
 */

int mux_close(void *handle)
{



}


int main(void)
{
	int index_file_test = open("/tmp/index",O_RDWR | O_CREAT | O_TRUNC);

	int record_file_test = open("/tmp/hello",O_RDWR | O_CREAT | O_TRUNC);

    system_up();
//	mux_open1(NULL, record_file_test, index_file_test);

    while(1)sleep(10);
        
	

}
