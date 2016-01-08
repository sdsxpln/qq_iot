#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include "anyka_types.h"
#include "akuio.h"
#include "dev_camera.h"
#include "video_stream_lib.h"
#include "video_stream.h"
#include "ring_queue.h"
#include "TXAudioVideo.h"
#include "common.h"
#include <linux/videodev2.h>
#include "msg_handle.h"

#include "video_record.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"video_stream:"

#define	 	ENCMEM_BUFF_SIZE	(256*1024)






typedef struct video_encode_handle
{
	void * handle;
	void * pvirtual_addres;
	void * pmem_addres;
	T_VIDEOLIB_ENC_RC rc;
	
}video_encode_handle_t;


typedef struct video_stream_handle
{
	pthread_mutex_t mutex_capture;
	pthread_cond_t cond_capture;
	volatile unsigned int need_capture;

	
	pthread_mutex_t mutex_encode;
	pthread_cond_t cond_encode;
	ring_queue_t raw_msg_queue;
	volatile unsigned int raw_msg_num;


	pthread_mutex_t mutex_video_mode;
	volatile unsigned int video_mode;
	

	camera_dev_t * video_dev;
	video_encode_handle_t * encode_handle;
	

	

	int  bit_rate;
	char iframe_gap;
	char force_iframe;
	char frame_count;

}video_stream_handle_t;


#define  VIDEO_BUFFER_COUNTS	(64)

static video_stream_handle_t *  stream_handle = NULL;

static video_data_t video_data_buff[VIDEO_BUFFER_COUNTS]={0};

static T_pVOID enc_mutex_create(T_VOID)
{
	pthread_mutex_t *pMutex;
	pMutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(pMutex, NULL); 
	return pMutex;
}

static T_S32 enc_mutex_lock(T_pVOID pMutex, T_S32 nTimeOut)
{
	pthread_mutex_lock(pMutex);
	return 1;
}

static T_S32 enc_mutex_unlock(T_pVOID pMutex)
{
	pthread_mutex_unlock(pMutex);
	return 1;
}

static T_VOID enc_mutex_release(T_pVOID pMutex)
{
	int rc = pthread_mutex_destroy( pMutex ); 
    if ( rc  != 0 ) {                      
        pthread_mutex_unlock( pMutex );             
        pthread_mutex_destroy( pMutex );    
    } 
	free(pMutex);
}


T_BOOL ak_enc_delay(T_U32 ticks)
{
	akuio_wait_irq();
	return AK_TRUE;
}


int video_encode_init(void)
{
	T_VIDEOLIB_CB	init_cb_fun;
	int ret;

	memset(&init_cb_fun, 0, sizeof(T_VIDEOLIB_CB));

	init_cb_fun.m_FunPrintf			= (MEDIALIB_CALLBACK_FUN_PRINTF)printf;
	init_cb_fun.m_FunMalloc  		= (MEDIALIB_CALLBACK_FUN_MALLOC)malloc;
	init_cb_fun.m_FunFree    		= free;
	init_cb_fun.m_FunRtcDelay       = ak_enc_delay;
				
	init_cb_fun.m_FunDMAMalloc		= (MEDIALIB_CALLBACK_FUN_DMA_MALLOC)akuio_alloc_pmem;
  	init_cb_fun.m_FunDMAFree		= (MEDIALIB_CALLBACK_FUN_DMA_FREE)akuio_free_pmem;
  	init_cb_fun.m_FunVaddrToPaddr	= (MEDIALIB_CALLBACK_FUN_VADDR_TO_PADDR)akuio_vaddr2paddr;
  	init_cb_fun.m_FunMapAddr		= (MEDIALIB_CALLBACK_FUN_MAP_ADDR)akuio_map_regs;
  	init_cb_fun.m_FunUnmapAddr		= (MEDIALIB_CALLBACK_FUN_UNMAP_ADDR)akuio_unmap_regs;
  	init_cb_fun.m_FunRegBitsWrite	= (MEDIALIB_CALLBACK_FUN_REG_BITS_WRITE)akuio_sysreg_write;
	init_cb_fun.m_FunVideoHWLock	= (MEDIALIB_CALLBACK_FUN_VIDEO_HW_LOCK)akuio_lock_timewait;
	init_cb_fun.m_FunVideoHWUnlock	= (MEDIALIB_CALLBACK_FUN_VIDEO_HW_UNLOCK)akuio_unlock;

	init_cb_fun.m_FunMutexCreate	= enc_mutex_create;
	init_cb_fun.m_FunMutexLock		= enc_mutex_lock;
	init_cb_fun.m_FunMutexUnlock	= enc_mutex_unlock;
	init_cb_fun.m_FunMutexRelease	= enc_mutex_release;

	ret = VideoStream_Enc_Init(&init_cb_fun);
	
	return 0;
}




static void  video_encode_unint( )
{
	VideoStream_Enc_Destroy();
}



static  void * video_new_encode(unsigned int bps)
{

	video_encode_handle_t * new_handle = calloc(1,sizeof(*new_handle));
	if(NULL == new_handle)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}

	new_handle->handle = NULL;
	new_handle->pmem_addres = NULL;
	new_handle->pvirtual_addres = NULL;
	new_handle->pmem_addres = akuio_alloc_pmem(ENCMEM_BUFF_SIZE);
	if(NULL == new_handle->pmem_addres)
	{
		dbg_printf("akuio_alloc_pmem fail ! \n");
		goto fail;
	}
	unsigned int addres = akuio_vaddr2paddr(new_handle->pmem_addres) & 7;
	new_handle->pvirtual_addres = ((unsigned char *)new_handle->pmem_addres) + ((8-addres)&7);

	T_VIDEOLIB_ENC_OPEN_INPUT open_input;
	memset(&open_input,0,sizeof(open_input));
	open_input.encFlag = VIDEO_DRV_H264;

	open_input.encH264Par.width = VIDEO_WIDTH_720P;		
	open_input.encH264Par.height = VIDEO_HEIGHT_720P;
	open_input.encH264Par.lumWidthSrc = VIDEO_WIDTH_720P;
	open_input.encH264Par.lumHeightSrc = VIDEO_HEIGHT_720P;
	open_input.encH264Par.horOffsetSrc = (open_input.encH264Par.lumWidthSrc-open_input.encH264Par.width)/2;
	open_input.encH264Par.verOffsetSrc = (open_input.encH264Par.lumHeightSrc-open_input.encH264Par.height)/2;
	
	open_input.encH264Par.rotation = ENC_ROTATE_0;		
	open_input.encH264Par.frameRateDenom = 1;
	open_input.encH264Par.frameRateNum = 25;	
	
	open_input.encH264Par.qpHdr = -1;		
  	open_input.encH264Par.streamType = 0;	
  	open_input.encH264Par.enableCabac = 1; 		
	open_input.encH264Par.transform8x8Mode = 0;
	
	open_input.encH264Par.qpMin = 10;        
	open_input.encH264Par.qpMax = 36;
	open_input.encH264Par.fixedIntraQp = 0;
    open_input.encH264Par.bitPerSecond = bps;
    open_input.encH264Par.gopLen = 50; 

	new_handle->rc.qpHdr = -1;
	new_handle->rc.qpMin = open_input.encH264Par.qpMin;
	new_handle->rc.qpMax = open_input.encH264Par.qpMax;
	new_handle->rc.fixedIntraQp = open_input.encH264Par.fixedIntraQp;
	new_handle->rc.bitPerSecond =  open_input.encH264Par.bitPerSecond;
	new_handle->rc.gopLen = open_input.encH264Par.gopLen;


	new_handle->handle = VideoStream_Enc_Open(&open_input);
	if(!new_handle->handle)
	{
		dbg_printf("VideoStream_Enc_Open fail ! \n");
		goto fail ;
	}
	return(new_handle);

fail:

	if(NULL != new_handle->pmem_addres)
	{
		akuio_free_pmem(new_handle->pmem_addres);
		new_handle->pmem_addres = NULL;

	}
	if(NULL != new_handle)
	{
		free(new_handle);
		new_handle = NULL;
	}

	return(NULL);
	
}

static int video_free_encode(video_encode_handle_t * pencode_handle)
{

	if(NULL == pencode_handle || NULL==pencode_handle->handle)return(-1);
	VideoStream_Enc_Close(pencode_handle->handle);
	akuio_free_pmem(pencode_handle->pmem_addres);
    free(pencode_handle);
	pencode_handle = NULL;
	return 0;
}


static int video_encode_frame(video_encode_handle_t * pencode_handle, void *pinbuf2, void **poutbuf2, frame_type_m  frame_type)
{

	if(NULL == pencode_handle || NULL == pencode_handle->handle ||  NULL == pencode_handle->pvirtual_addres)
	{
		dbg_printf("please check the param ! \n");
		return(-1);
		
	}
	if(I_FRAME != frame_type && P_FRAME != frame_type)
	{
		dbg_printf("check the frame type ! \n");
		return(-2);

	}
	T_VIDEOLIB_ENC_IO_PAR video_enc_io_param2;
    video_enc_io_param2.QP = 0;		
	video_enc_io_param2.mode = frame_type;
	
	video_enc_io_param2.p_curr_data = pinbuf2;		
	video_enc_io_param2.p_vlc_data = pencode_handle->pvirtual_addres;			
	video_enc_io_param2.out_stream_size = ENCMEM_BUFF_SIZE;

	VideoStream_Enc_Encode(pencode_handle->handle, NULL, &video_enc_io_param2, NULL);
	*poutbuf2 = video_enc_io_param2.p_vlc_data;

	return video_enc_io_param2.out_stream_size;
}





int video_encode_reSetRc(int bit_rate)
{

	dbg_printf("bit_rate===%d \n",bit_rate);
	if(NULL == stream_handle || NULL==stream_handle->encode_handle)
	{
		dbg_printf("please check the param ! \n");
		return(-1);
	}
	video_encode_handle_t * pencode_handle = stream_handle->encode_handle;

	if(bit_rate > 400)bit_rate=400;
	if(bit_rate != stream_handle->bit_rate)
	{
		stream_handle->bit_rate = bit_rate;
		 pencode_handle->rc.bitPerSecond = bit_rate*1024;
		 VideoStream_Enc_setRC(pencode_handle->handle, &pencode_handle->rc);
	}
	return (0) ;
}



static int stream_push_raw_data(void * data )
{

	int ret = 1;
	int i = 0;
	video_stream_handle_t * handle = (video_stream_handle_t*)stream_handle;
	if(NULL == handle || NULL== data)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	pthread_mutex_lock(&(handle->mutex_encode));

	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&handle->raw_msg_queue, data);
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
		pthread_mutex_unlock(&(handle->mutex_encode));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &handle->raw_msg_num;
    	fetch_and_add(task_num, 1);
    }
    pthread_mutex_unlock(&(handle->mutex_encode));
	pthread_cond_signal(&(handle->cond_encode));
	return(0);
}



static int  video_fill_record_data(unsigned char *pcEncData, int nEncDataLen,int nFrameType, unsigned long nTimeStamps, int nGopIndex, int nFrameIndex, int nTotalIndex)
{
	int i = 0;
	int flag = 0;
	video_data_t * video = video_data_buff;
	if(nEncDataLen <= 0  ||  nEncDataLen > VIDEO_DATA_MAX_SIZE)
	{
		dbg_printf("check the param !nEncDataLen==%ld  nFrameType==%d\n",nEncDataLen,nFrameType);
		return(-1);
	}

	//dbg_printf("nFrameType==%d nEncDataLen==%dkb\n",nFrameType,nEncDataLen/1024);
	for(i=0;i<VIDEO_BUFFER_COUNTS;++i)
	{
		if(0 == video->status)
		{
			flag =1;
			video->status = 1;
			video->type = RECORD_VIDEO_DATA;
			video->nFrameType = nFrameType;
			video->nTimeStamps = nTimeStamps;
			video->nGopIndex = nGopIndex;
			video->nFrameIndex = nFrameIndex;
			video->nTotalIndex = nTotalIndex;
			video->nEncDataLen = nEncDataLen;
			memmove(video->data,pcEncData,video->nEncDataLen);
			record_push_record_data(video);
			break;
		}
		video += 1;
	}

	if(flag == 0)
	{
		dbg_printf("not find ! \n");
	}
	
	return(0);
}
        







static int video_capture_open(void)
{

	video_stream_handle_t * handle = (video_stream_handle_t*)stream_handle;
	camera_dev_t * dev = handle->video_dev;
	
	if(NULL == handle || NULL== dev)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	
	pthread_mutex_lock(&(handle->mutex_capture));
	volatile unsigned int *task_num = &handle->need_capture;
	compare_and_swap(task_num, 0,1);
	pthread_mutex_unlock(&(handle->mutex_capture));
	pthread_cond_signal(&(handle->cond_capture));

	return(0);
}



static int  video_capture_close(void)
{


	video_stream_handle_t * handle = stream_handle;

	camera_dev_t * dev = handle->video_dev;

	if(NULL == handle || NULL == dev)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	pthread_mutex_lock(&(handle->mutex_capture));
	volatile unsigned int *task_num = &handle->need_capture;
	compare_and_swap(task_num, 1,0);
	pthread_mutex_unlock(&(handle->mutex_capture));

	
	dbg_printf("video_capture_stop\n");
	return(0);
}


int video_send_video_start(void)
{

	video_stream_handle_t * handle = (video_stream_handle_t*)stream_handle;
	if(NULL == handle)return(-1);

	pthread_mutex_lock(&(handle->mutex_video_mode));
	record_replay_send_stop();
	handle->video_mode |= SEND_VIDEO;
	video_capture_open();
	pthread_mutex_unlock(&(handle->mutex_video_mode));
	

	return(0);
}


int video_send_video_stop(void)
{
	
	video_stream_handle_t * handle = (video_stream_handle_t*)stream_handle;
	if(NULL == handle)return(-1);

	pthread_mutex_lock(&(handle->mutex_video_mode));
	handle->video_mode &= ~SEND_VIDEO;
	if(0 == (handle->video_mode & RECORD_VIDEO))
	{
		video_capture_close();	
	}
	record_replay_send_stop();
	pthread_mutex_unlock(&(handle->mutex_video_mode));

	

	return(0);
}


int video_record_video_start(void)
{
	
	video_stream_handle_t * handle = (video_stream_handle_t*)stream_handle;
	if(NULL == handle)return(-1);

	pthread_mutex_lock(&(handle->mutex_video_mode));
	handle->video_mode |= RECORD_VIDEO;
	video_capture_open();
	pthread_mutex_unlock(&(handle->mutex_video_mode));
	
	return(0);
}


int video_record_video_stop(void)
{
	
	video_stream_handle_t * handle = (video_stream_handle_t*)stream_handle;
	if(NULL == handle)return(-1);

	pthread_mutex_lock(&(handle->mutex_video_mode));
	handle->video_mode &= ~RECORD_VIDEO;
	if(0 == (handle->video_mode & SEND_VIDEO))
	{
		video_capture_close();	
	}
	pthread_mutex_unlock(&(handle->mutex_video_mode));
	return(0);
}




static void * video_capture_pthread(void * arg)
{

	int ret = -1;
	video_stream_handle_t * handle = (video_stream_handle_t*)arg;
	camera_dev_t * dev = handle->video_dev;
	
	if(NULL == handle || NULL== dev)
	{
		dbg_printf("check the param ! \n");
		return(NULL);
	}
	camera_start(dev);
	int is_run = 1;
	struct v4l2_buffer *frame_buf = NULL;
	while(is_run)
	{
		pthread_mutex_lock(&(handle->mutex_capture));
		while (0 == handle->need_capture)
		{
			pthread_cond_wait(&(handle->cond_capture), &(handle->mutex_capture));

		}
		pthread_mutex_unlock(&(handle->mutex_capture));
		
		frame_buf = camera_read_frame(dev);
		if(NULL == frame_buf)continue;
		ret = stream_push_raw_data(frame_buf);
		if(0 != ret)
		{
			camera_free_frame(dev,frame_buf);
		}
		
	}

	return(NULL);

}


static void * video_encode_pthread(void * arg)
{


	video_stream_handle_t * handle = (video_stream_handle_t*)arg;
	camera_dev_t * dev = handle->video_dev;

	if(NULL == handle || NULL== dev)
	{
		dbg_printf("check the param ! \n");
		return(NULL);
	}

	int ret = -1;
	int i = 0;
	int size = 0;
	int is_run = 1;
	unsigned long timeStamp =  0;
	struct v4l2_buffer *frame_buf = NULL;
	frame_type_m i_frame = UNKNOW_FRAME;
	void * out_buff = NULL;

	char force_iframe_flag = 0;
	unsigned long nGopIndex = 0;
	unsigned long nFrameIndex = 0;
	unsigned long nTotalIndex = 0;

	while(is_run)
	{
        pthread_mutex_lock(&(handle->mutex_encode));
        while (0 == handle->raw_msg_num)
        {
            pthread_cond_wait(&(handle->cond_encode), &(handle->mutex_encode));
        }
		ret = ring_queue_pop(&(handle->raw_msg_queue), (void **)&frame_buf);
		pthread_mutex_unlock(&(handle->mutex_encode));
		
		volatile unsigned int *handle_num = &(handle->raw_msg_num);
		fetch_and_sub(handle_num, 1);  

		if(ret != 0 || NULL == frame_buf)continue;

		if(handle->force_iframe)
		{
			i_frame = I_FRAME;	
			handle->force_iframe = 0;
			force_iframe_flag = 1;
		}
		else
		{
			i_frame = (0 == handle->frame_count % handle->iframe_gap) ? (I_FRAME) : (P_FRAME);
			handle->frame_count = (handle->frame_count + 1)%handle->iframe_gap;
		}

		if(1 == force_iframe_flag)
		{

		}
		else if(I_FRAME == i_frame)
		{
			nGopIndex = (nGopIndex+1)%65535;	
			nFrameIndex = 0;
		}
		else
		{
			nFrameIndex ++;
		}
		nTotalIndex = (nTotalIndex+1)%65535;

		timeStamp = frame_buf->timestamp.tv_sec * 1000ULL + frame_buf->timestamp.tv_usec / 1000ULL;
		size = video_encode_frame(handle->encode_handle,(void*)frame_buf->m.userptr,&out_buff,i_frame);

		if(handle->video_mode & SEND_VIDEO)
		{
			tx_set_video_data(out_buff,size,i_frame,timeStamp,nGopIndex,nFrameIndex,nTotalIndex,30);
		}

		if(handle->video_mode & RECORD_VIDEO)
		{
			video_fill_record_data(out_buff,size,i_frame,timeStamp,nGopIndex,nFrameIndex,nTotalIndex);
		}
		

		camera_free_frame(dev,frame_buf);

	}

	return(NULL);

}


static  int video_stream_init(void)
{

	int ret = -1;
	if(NULL != stream_handle)
	{
		dbg_printf("stream_handle has been init ! \n");
		return(-1);

	}
	stream_handle = (video_stream_handle_t*)calloc(1,sizeof(*stream_handle));
	if(NULL == stream_handle)
	{
		dbg_printf("calloc is fail ! \n");
		return(-1);
	}

	
	pthread_mutex_init(&(stream_handle->mutex_capture), NULL);
	pthread_cond_init(&(stream_handle->cond_capture), NULL);
	stream_handle->need_capture = 0;
	
	
	ret = ring_queue_init(&stream_handle->raw_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");

	}
    pthread_mutex_init(&(stream_handle->mutex_encode), NULL);
    pthread_cond_init(&(stream_handle->cond_encode), NULL);
	stream_handle->raw_msg_num = 0;


	pthread_mutex_init(&(stream_handle->mutex_video_mode), NULL);
	

	
	stream_handle->video_dev = (camera_dev_t*)camera_new_dev(CAMERA_DEVICE);
	if(NULL == stream_handle->video_dev)
	{
		dbg_printf("camera_new_dev is fail ! \n");
		goto fail;
	}

	stream_handle->bit_rate = 200;
	stream_handle->encode_handle = video_new_encode(stream_handle->bit_rate*1024);
	if(NULL == stream_handle->encode_handle)
	{
		dbg_printf("video_new_encode is fail !\n");
		goto fail;
	}
	stream_handle->frame_count = 0;
	stream_handle->force_iframe = 0;
	stream_handle->iframe_gap = 50;
	stream_handle->video_mode = 0;
	
	return(0);

fail:

	if(NULL != stream_handle->video_dev)
	{
		camera_free_dev(stream_handle->video_dev);	
	}

	if(NULL != stream_handle->encode_handle)
	{
		video_free_encode(stream_handle->encode_handle);	
	}

	if(NULL != stream_handle)
	{
		free(stream_handle);
		stream_handle = NULL;
	}

	return(-1);
}



int video_handle_stream_up(void)
{




	
	int ret = -1;
	ret = video_encode_init();
	if(0 != ret)
	{
		dbg_printf("video_encode_init is fail ! \n");
		return(-1);
	}

	

	ret = video_stream_init();
	if(0 != ret )
	{
		dbg_printf("video_stream_init is fail ! \n");
		return(-1);
	}

	memset(video_data_buff,0,sizeof(video_data_buff));

	

	pthread_t raw_video_pthid;
	ret = pthread_create(&raw_video_pthid,NULL,video_capture_pthread,stream_handle);
	pthread_detach(raw_video_pthid);


	pthread_t encode_video_pthid;
	ret = pthread_create(&encode_video_pthid,NULL,video_encode_pthread,stream_handle);
	pthread_detach(encode_video_pthid);


	return(0);
}

