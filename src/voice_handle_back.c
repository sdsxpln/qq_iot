#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "ring_queue.h"
#include "common.h"
#include "pcm_dev.h"
#include "amr_loadlib.h"
#include "amr_encode.h"
#include "amr_decode.h"
#include "TXAudioVideo.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"voice_handle:"




typedef  enum
{
	PCM_FILE,
	PCM_DATA,
	AMR_FILE,
	AMR_DATA,
	MP3_FILE,
	MP3_DATA,
	
}voice_type_m;


typedef struct pcm_node
{
	unsigned int length;
	char pcm_data[PCM_PACKET_SIZE];
}pcm_node_t;




typedef  struct voice_node_head
{
	voice_type_m type;	
	unsigned int sample_rate;
	unsigned int data_length;
}voice_node_head_t;



typedef  union voice_node_data
{
	char file_name[128];
	char data_buff[128];

}voice_node_data_t;




typedef struct voice_system
{

	pthread_mutex_t mutex_voice;
	pthread_cond_t cond_voice;
	ring_queue_t voice_msg_queue;
    volatile unsigned int voice_msg_num;

	pthread_mutex_t mutex_mic;
	pthread_cond_t cond_mic;
    volatile unsigned int mic_run;

	pthread_mutex_t mutex_mic_data;
	pthread_cond_t cond_mic_data;
	ring_queue_t mic_data_msg_queue;
    volatile unsigned int mic_data_msg_num;
	
	

	void * da_user;
	void * ad_user;

	tx_audio_encode_param encode_param;
	amr_decode_handle_t * pamr_decode_handle;
	amr_encode_handle_t * pamr_encode_handle;
	

}voice_system_t;



static voice_system_t * voice_handle  = NULL;



int voice_capture_mic_start(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory || NULL == factory->ad_user )
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	volatile  unsigned int * flag = &(factory->mic_run);
	compare_and_swap(flag, 0, 1);
	pthread_cond_signal(&(factory->cond_mic));
	return(0);
}



int voice_capture_mic_stop(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory || NULL == factory->ad_user )
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	volatile  unsigned int * flag = &(factory->mic_run);
	compare_and_swap(flag, 1, 0);

	return(0);
}


static void * voice_pcmtoamr(void * data,unsigned int length,void * user)
{

	voice_system_t * factory = (voice_system_t*)user;
	#if 0
	int fd = -1;
	fd = open("/tmp/test.amr",O_WRONLY|O_APPEND);
	if(fd > 0 )
	{
		write(fd,data,length);

	}

	#endif
	
	tx_set_audio_data(&factory->encode_param,data,length);

	return(NULL);
}


static int voice_push_mic_data(void * data )
{

	int ret = 1;
	int i = 0;

	
	voice_system_t * handle = (voice_system_t*)voice_handle;
	if(NULL == handle || NULL== data)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	pthread_mutex_lock(&(handle->mutex_mic_data));

	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&handle->mic_data_msg_queue, data);
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
		pthread_mutex_unlock(&(handle->mutex_mic_data));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &handle->mic_data_msg_num;
    	fetch_and_add(task_num, 1);
    }
    pthread_mutex_unlock(&(handle->mutex_mic_data));
	pthread_cond_signal(&(handle->cond_mic_data));
	return(0);
}



static void *  voice_mic_capture_pthread(void * arg)
{
	voice_system_t * factory = (voice_system_t * )arg;
	if(NULL == factory)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	amr_encode_handle_t * encode_handle = (amr_encode_handle_t*)factory->pamr_encode_handle;
	
	adc_user_t * pad_user = (adc_user_t*)(factory->ad_user);
	if(NULL == pad_user)
	{
		dbg_printf("please init the ad modual ! \n");
		return(NULL);
	}

	int ret = -1;
	pcm_node_t  read_node;
	int is_run = 1;
	while(is_run)
	{
        pthread_mutex_lock(&(factory->mutex_mic));
        while (0 == factory->mic_run)
        {
            pthread_cond_wait(&(factory->cond_mic), &(factory->mutex_mic));
        }
		pthread_mutex_unlock(&(factory->mutex_mic));

		//read_node = calloc(1,sizeof(*read_node));
		//if(NULL == read_node)continue;
	
		read_node.length = adc_read_data(pad_user,read_node.pcm_data,PCM_PACKET_SIZE);
	//	dbg_printf("read_node.length===%d\n",read_node.length);
		amr_buff_pcmtoamr(encode_handle,voice_pcmtoamr,factory,read_node.pcm_data,read_node.length);
		
		//ret = voice_push_mic_data(read_node);
		if(0/*0 != ret*/)
		{
			dbg_printf("voice_push_mic_data is fail ! \n");
			//free(read_node);
			//read_node = NULL;
		}
		usleep(10000);
	}

	return(NULL);

}


static void *  voice_mic_process_pthread(void * arg)
{
	voice_system_t * factory = (voice_system_t * )arg;
	if(NULL == factory)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}

	amr_encode_handle_t * encode_handle = (amr_encode_handle_t*)factory->pamr_encode_handle;
	if(NULL == encode_handle)
	{
		dbg_printf("encode_handle has not be inited ! \n");
		return(NULL);
	}

	int ret = -1;
	int is_run = 1;
	pcm_node_t * read_node = NULL;
	while(is_run)
	{
        pthread_mutex_lock(&(factory->mutex_mic_data));
        while (0 == factory->mic_data_msg_num)
        {
            pthread_cond_wait(&(factory->cond_mic_data), &(factory->mutex_mic_data));
        }
		ret = ring_queue_pop(&(factory->mic_data_msg_queue), (void **)&read_node);
		pthread_mutex_unlock(&(factory->mutex_mic_data));
		
		volatile unsigned int *handle_num = &(factory->mic_data_msg_num);
		fetch_and_sub(handle_num, 1);  
		if(ret != 0 || NULL == read_node)continue;

		amr_buff_pcmtoamr(encode_handle,voice_pcmtoamr,factory,read_node->pcm_data,read_node->length);

		free(read_node);
		read_node = NULL;

	}

	return(NULL);

}



int voice_system_start_up(void)
{

	int ret = -1;
	if(NULL != voice_handle)
	{
		dbg_printf("the handle has been init !\n");
		return(-1);
	}

	voice_handle = calloc(1,sizeof(*voice_handle));
	if(NULL == voice_handle)
	{
		dbg_printf("calloc is fail !\n");
		return(-1);
	}

	ret = ring_queue_init(&voice_handle->voice_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");
		return(-1);
	}
    pthread_mutex_init(&(voice_handle->mutex_voice), NULL);
    pthread_cond_init(&(voice_handle->cond_voice), NULL);
	voice_handle->voice_msg_num = 0;


    pthread_mutex_init(&(voice_handle->mutex_mic), NULL);
    pthread_cond_init(&(voice_handle->cond_mic), NULL);
	voice_handle->mic_run = 0;
	

	ret = ring_queue_init(&voice_handle->mic_data_msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");
		return(-1);
	}
    pthread_mutex_init(&(voice_handle->mutex_mic_data), NULL);
    pthread_cond_init(&(voice_handle->cond_mic_data), NULL);
	voice_handle->mic_data_msg_num = 0;

	dac_open_dev();
	voice_handle->da_user = (dac_user_t*)dac_new_user(2,16,8000);
	if(NULL == voice_handle->da_user)
	{
		dbg_printf("dac_new_user is fail !\n");
	}
	

	adc_open_dev();
	voice_handle->ad_user = (adc_user_t*)adc_new_user(1,16,7990);
	if(NULL == voice_handle->ad_user)
	{
		dbg_printf("adc_new_user fail ! \n");
		return(-1);
	}

	ret = amr_decodelib_open();
	if(ret != 0)
	{
		dbg_printf("amr_decodelib_open  fail ! \n");
		return(-1);
	}
	voice_handle->pamr_decode_handle = amr_new_decode(1,16,8000,AMR_MR122_HEADER);
	if(NULL ==  voice_handle->pamr_decode_handle)
	{
		dbg_printf("amr_new_decode  fail ! \n");
		return(-1);
	}

	ret = amr_encodelib_open();
	if(ret != 0)
	{
		dbg_printf("amr_encodelib_open  fail ! \n");
		return(-1);
	}
	
	voice_handle->pamr_encode_handle = amr_new_encode(1,16,7990,AMR_MR122);
	if(NULL == voice_handle->pamr_encode_handle)
	{
		dbg_printf("pvoice_center->pamr_encode_handle fail ! \n");
		return(-1);
	}

	memset(&voice_handle->encode_param,0,sizeof(voice_handle->encode_param));
    voice_handle->encode_param.head_length = sizeof(tx_audio_encode_param);
    voice_handle->encode_param.audio_format = 1; 
    voice_handle->encode_param.encode_param = 7; 
    voice_handle->encode_param.frame_per_pkg = FRAME_NUMS_PER_PACKET;
    voice_handle->encode_param.sampling_info = GET_SIMPLING_INFO(1, 8000, 16);

	pthread_t mic_capture_pthred;
	pthread_create(&mic_capture_pthred, NULL, voice_mic_capture_pthread, voice_handle);
	pthread_detach(mic_capture_pthred);

	pthread_t mic_process_pthred;
	pthread_create(&mic_process_pthred, NULL, voice_mic_process_pthread, voice_handle);
	pthread_detach(mic_process_pthred);
	

	return(0);
}

