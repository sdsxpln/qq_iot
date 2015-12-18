#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"

#include "msg_handle.h"
#include "ring_queue.h"
#include "system_up.h"
#include "TXAudioVideo.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"handle_msg:"


typedef struct msg_handle
{
	pthread_mutex_t msg_mutex;
	pthread_cond_t msg_cond;
	ring_queue_t msg_queue;
	volatile unsigned int msg_num;
	char is_run;
	
}msg_handle_t;


static msg_handle_t * msg_center = NULL;


int  msg_push(void * data )
{

	int ret = 1;
	int i = 0;

	msg_handle_t * msg = (msg_handle_t*)msg_center;
	if(NULL == msg)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	pthread_mutex_lock(&(msg->msg_mutex));

	for (i = 0; i < 1000; i++)
	{
	    ret = ring_queue_push(&msg->msg_queue, data);
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
		pthread_mutex_unlock(&(msg->msg_mutex));
		return (-2);
    }
    else
    {
		volatile unsigned int *task_num = &msg->msg_num;
    	fetch_and_add(task_num, 1);
    }

    pthread_mutex_unlock(&(msg->msg_mutex));
	pthread_cond_signal(&(msg->msg_cond));
	return(0);
}

static int msg_handle_init(void)
{
	int ret = -1;
	if(NULL != msg_center)
	{
		dbg_printf("msg_center has been init ! \n");
		return(-1);
	}

	msg_center = calloc(1,sizeof(*msg_center));
	if(NULL == msg_center)
	{
		dbg_printf("calloc is fail ! \n");
		return(-1);
	}
	ret = ring_queue_init(&msg_center->msg_queue, 1024);
	if(ret < 0 )
	{
		dbg_printf("ring_queue_init  fail \n");
		free(msg_center);
		msg_center = NULL;
		return(-1);
	}
    pthread_mutex_init(&(msg_center->msg_mutex), NULL);
    pthread_cond_init(&(msg_center->msg_cond), NULL);
	msg_center->msg_num = 0;
	
	return(0);
}


static int msg_login_complete(void *arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(-1);
	}
	login_complete_t * login = (login_complete_t*)arg;
	if(login->error_code)
	{
		dbg_printf("msg_login_complete is fail ! \n");
	}
	else
	{
		dbg_printf("msg_login_complete is succed ! \n");

	}
	
	return(0);
}


static int msg_online_status(void * arg)
{

	int ret = -1;
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(-1);
	}
	
	if(NULL==system_config_info || NULL == system_config_info->dev_av)
	{
		dbg_printf("please check the param ! \n");
		return(-1);
	}
	online_status_t * status = (online_status_t * )arg;
	dbg_printf("online==%d  %d \n",status->new_status,status->old_status);

	if(status->new_status != system_config_info->online_status )
	{
		system_config_info->online_status = status->new_status;
		if(DEV_ONLINE == status->new_status)
		{
			ret = tx_start_av_service((tx_av_callback*)system_config_info->dev_av);
		}
		else if(DEV_OFFLINE == status->new_status)
		{
			ret= tx_stop_av_service();	

		}
		dbg_printf("ret ====== %d \n",ret);
		

	}


	return(0);
}



static void * msg_handle_pthread(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}

	int ret = -1;
	cmd_type_t type = 0;
	msg_handle_t * msg = (msg_handle_t*)arg;
	msg_header_t * packet = NULL;
	
	msg->is_run = 1;
	while(msg->is_run)
	{

        pthread_mutex_lock(&(msg->msg_mutex));
        while (0 == msg->msg_num)
        {
            pthread_cond_wait(&(msg->msg_cond), &(msg->msg_mutex));
        }
		ret = ring_queue_pop(&(msg->msg_queue), (void **)&packet);
		pthread_mutex_unlock(&(msg->msg_mutex));
		
		volatile unsigned int * num = &(msg->msg_num);
		fetch_and_sub(num, 1); 

		if(ret != 0 || NULL == packet)continue;

		switch(packet->cmd)
		{

			case ONLINE_STATUS_CMD:
			{
				msg_online_status(packet+1);
				break;
			}
			case LOGIN_COMPLETE_CMD:
			{

				msg_login_complete(packet+1);
				break;
			}


			default:
			{
				dbg_printf("unknow cmd type ! \n");
				break;

			}
			
		}

		
		


		free(packet);
		packet = NULL;

	}



}




int msg_handle_start_up(void)
{
	int ret = -1;
	ret = msg_handle_init();
	if(ret != 0)
	{
		dbg_printf("msg_handle_init is fail ! \n");
	}

	pthread_t msg_pthid;
	ret = pthread_create(&msg_pthid,NULL,msg_handle_pthread,msg_center);
	pthread_detach(msg_pthid);

	return(0);
}



