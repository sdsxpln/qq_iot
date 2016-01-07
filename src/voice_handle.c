#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "ring_queue.h"
#include "common.h"
#include "pcm_dev.h"
#include "amr_encode.h"
#include "amr_decode.h"
#include "TXAudioVideo.h"
#include "msg_handle.h"
#include "video_record.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"voice_handle:"



#define  PCM_RAW_BUFF_NUM	(100)

typedef struct pcm_node
{
	unsigned int status;
	unsigned int length;
	unsigned long time_sample;
	char pcm_data[ENCODE_PCM_SIZE];
}pcm_node_t;


typedef enum
{
	VOICE_SEND = (0x01<<1),
	VOICE_RECORD = (0x01<<2),
}voice_mode_m;


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
	
	

	void * da_handle;
	void * ad_handle;
	tx_audio_encode_param encode_param;
	void * pamr_decode_handle;
	void * pamr_encode_handle;

	pcm_node_t *pcm_raw_buff;
	int voice_mode;
	

}voice_system_t;



static voice_system_t * voice_handle  = NULL;



static int voice_capture_mic_start(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory || NULL == factory->ad_handle)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	volatile  unsigned int * flag = &(factory->mic_run);
	compare_and_swap(flag, 0, 1);
	pthread_cond_signal(&(factory->cond_mic));
	return(0);
}



static int voice_capture_mic_stop(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory || NULL == factory->ad_handle)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	volatile  unsigned int * flag = &(factory->mic_run);
	compare_and_swap(flag, 1, 0);

	return(0);
}



int voice_send_start(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	pthread_mutex_lock(&(factory->mutex_mic));
	factory->voice_mode |= VOICE_SEND;
	voice_capture_mic_start();
	pthread_mutex_unlock(&(factory->mutex_mic));


	return(0);
}


int voice_send_stop(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	pthread_mutex_lock(&(factory->mutex_mic));
	factory->voice_mode &= ~VOICE_SEND;
	if(0 == factory->voice_mode&VOICE_RECORD)
	{
		voice_capture_mic_stop();
	}
	pthread_mutex_unlock(&(factory->mutex_mic));
	
	return(0);
}


int voice_record_start(void)
{

	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	pthread_mutex_lock(&(factory->mutex_mic));
	factory->voice_mode |= VOICE_RECORD;
	voice_capture_mic_start();
	pthread_mutex_unlock(&(factory->mutex_mic));
	return(0);
}



int voice_record_stop(void)
{


	voice_system_t * factory = (voice_system_t*)voice_handle;
	if(NULL == factory)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	pthread_mutex_lock(&(factory->mutex_mic));
	factory->voice_mode &= ~VOICE_RECORD;
	if(0 == factory->voice_mode&VOICE_SEND)
	{
		voice_capture_mic_stop();
	}
	pthread_mutex_unlock(&(factory->mutex_mic));
	return(0);
}


int voice_net_send(unsigned char *enc_data, int data_len)
{

	voice_system_t * factory = (voice_system_t * )voice_handle;
	if(NULL == factory)
	{
		dbg_printf("please init the handle !\n");
		return(-1);
	}
	tx_set_audio_data(&factory->encode_param,enc_data,data_len);
	
	return(0);
}
static void * voice_get_free_rawbuff(void)
{
	int i = 0;
	voice_system_t * factory = (voice_system_t * )voice_handle;
	if(NULL == factory)
	{
		dbg_printf("please init the handle !\n");
		return(NULL);
	}
	pcm_node_t * node = factory->pcm_raw_buff;
	if(NULL == node)
	{
		dbg_printf("check the buff!\n");
		return(NULL);
	}
	for(i=0;i<PCM_RAW_BUFF_NUM;++i)
	{
		if(0 == node->status)return(node);
		node+= 1;
	}
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
	void * ad_handle = factory->ad_handle;
	if(NULL==ad_handle)
	{
		dbg_printf("please check the param !\n");
		return(NULL);
	}
	
	int ret = -1;
	int is_run = 1;
	int read_length = 0;
	int enc_length = 0;
	unsigned long nTimeStamp=0;

    pcm_set_ad_agc_enable(1);/*!*/
    pcm_ad_filter_enable(ad_handle, 1);
	pcm_set_mic_vol(ad_handle,5);
	while(is_run)
	{
        pthread_mutex_lock(&(factory->mutex_mic));
        while (0 == factory->mic_run)
        {
            pthread_cond_wait(&(factory->cond_mic), &(factory->mutex_mic));
        }
		pthread_mutex_unlock(&(factory->mutex_mic));

	
		pcm_node_t * node = (pcm_node_t*)voice_get_free_rawbuff();
		if(NULL == node)
		{
			sleep(1);
			dbg_printf("do not find the node !\n");
			continue;
		}
		node->length= pcm_read_ad(ad_handle,node->pcm_data,ENCODE_PCM_SIZE,(unsigned long *)&node->time_sample);
		if(node->length <= 0 )
		{
			free(node);
			node = NULL;
			continue;
		}

		
		ret = voice_push_mic_data(node);
		if(ret != 0)
		{
			free(node);
			node = NULL;
			continue;

		}

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
	int ret = -1;
	int is_run = 1;
	int enc_length = 0;
	pcm_node_t * read_node = NULL;
	void * encode_handle = factory->pamr_encode_handle;
	if(NULL == encode_handle)
	{
		dbg_printf("encode_handle is null \n");
		return(NULL);
	}

	char enc_out_buff[ENCODE_AMR_SIZE];
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

		enc_length=amr_encode(encode_handle,read_node->pcm_data,read_node->length,enc_out_buff,ENCODE_AMR_SIZE);

		if(factory->voice_mode&VOICE_SEND)
		{
			voice_net_send(enc_out_buff,enc_length);
		}

		if(factory->voice_mode&VOICE_RECORD)
		{
			voice_data_t * voice_record = calloc(1,sizeof(*voice_record)+1);
			if(NULL != voice_record)
			{
				voice_record->type = RECORD_VOICE_DATA;
				voice_record->data_length = enc_length;
				voice_record->time_sample = read_node->time_sample;
				memmove(voice_record->data,enc_out_buff,voice_record->data_length);
				ret = record_push_record_data(voice_record);
				if(ret != 0)
				{
					free(voice_record);
					voice_record = NULL;
				}
				
			}			

		}
		
		read_node->status = 0;
		
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

	voice_handle->ad_handle = pcm_ad_open(8000,16);
	if(NULL == voice_handle->ad_handle)
	{
		dbg_printf("dac_new_user is fail !\n");
	}

	voice_handle->pamr_encode_handle = amr_enc_open(1,8000,16);
	if(NULL ==  voice_handle->pamr_encode_handle)
	{
		dbg_printf("pamr_encode_handle  fail ! \n");
		return(-1);
	}

	memset(&voice_handle->encode_param,0,sizeof(voice_handle->encode_param));
    voice_handle->encode_param.head_length = sizeof(tx_audio_encode_param);
    voice_handle->encode_param.audio_format = 1; 
    voice_handle->encode_param.encode_param = 7; 
    voice_handle->encode_param.frame_per_pkg = FRAME_NUM_PER_PACKET;
    voice_handle->encode_param.sampling_info = GET_SIMPLING_INFO(1, 8000, 16);

	voice_handle->pcm_raw_buff = calloc(1,sizeof(pcm_node_t)*PCM_RAW_BUFF_NUM);
	if(NULL == voice_handle->pcm_raw_buff)
	{
		dbg_printf("calloc is fail !\n");
		return(-1);
	}
	voice_handle->voice_mode = 0;
	
	pthread_t mic_capture_pthred;
	pthread_create(&mic_capture_pthred, NULL, voice_mic_capture_pthread, voice_handle);
	pthread_detach(mic_capture_pthred);

	pthread_t mic_process_pthred;
	pthread_create(&mic_process_pthred, NULL, voice_mic_process_pthread, voice_handle);
	pthread_detach(mic_process_pthred);
	

	return(0);
}

