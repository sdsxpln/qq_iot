#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "TXDeviceSDK.h"
#include "TXIPCAM.h"

#include "tencent_init.h"
#include "common.h"
#include "msg_handle.h"
#include "talk_handle.h"
#include "video_record.h"



#undef  TENCENT_CFG_FILE
#define	 TENCENT_CFG_FILE	"/etc/jffs2/tencent.conf"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"tencent_init:"


#define  OS_PLATFORM		"Linux"
			
#define	 DEV_NAME			"ip_camera"
#define  VERSION_ID			(201512)



static void qq_login_complete(int error_code)
{

	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+sizeof(login_complete_t)+1);
	if(NULL == msg)return;
	
	login_complete_t * login = (login_complete_t*)(msg+1);

	msg->cmd = LOGIN_COMPLETE_CMD;
	login->error_code = error_code;
	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}
}

static void qq_online_status(int old_status,  int new_status)
{
	dbg_printf("old_status==%d  new_status==%d \n",old_status,new_status);
	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+sizeof(online_status_t)+1);
	if(NULL == msg)return;
	
	online_status_t * status = (online_status_t*)(msg+1);

	msg->cmd = ONLINE_STATUS_CMD;
	status->new_status = new_status;
	status->old_status = old_status;
	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}

}

void qq_wlan_upload(int error_code, tx_binder_info * pBinderList, int nCount)
{
	dbg_printf("qq_wlan_upload-------------------\n");
	

}


static void qq_binder_list_change(int error_code, tx_binder_info * pBinderList, int nCount)
{

	dbg_printf("error_code===%d \n",error_code);

}


static int qq_set_definition(int definition, char *cur_definition, int cur_definition_length)
{

	dbg_printf("definition====%d \n",definition);
	return(0);
}

static int qq_control_rotate(int rotate_direction, int rotate_degree)
{

	dbg_printf("rotate_degree===%d \n",rotate_degree);
	return(0);
}



static bool qq_start_camera(void)
{

	dbg_printf("qq_start_camera\n");
	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+1);
	if(NULL == msg)return;
	msg->cmd = START_VIDEO_CMD;

	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}

	return(true);
}


static bool qq_stop_camera(void)
{


	dbg_printf("qq_stop_camera\n");
	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+1);
	if(NULL == msg)return;
	msg->cmd = STOP_VIDEO_CMD;

	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}
	return(true);
}

static bool qq_set_bitrate(int bit_rate)
{

	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+sizeof(set_bitrate_t)+1);
	if(NULL == msg)return;
	
	set_bitrate_t * bitrate = (set_bitrate_t*)(msg+1);

	msg->cmd = SET_BITRATE_CMD;
	bitrate->bit_rate = bit_rate;
	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}
	
	
	return(true);
}

static bool qq_start_mic()
{

	dbg_printf("qq_start_mic\n");

	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+1);
	if(NULL == msg)return(false);
	msg->cmd = START_MIC_CMD;
	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}
	return(true);
}



static bool qq_stop_mic()
{

	dbg_printf("qq_stop_mic\n");

	int ret = -1;
	msg_header_t * msg = calloc(1,sizeof(msg_header_t)+1);
	if(NULL == msg)return(false);
	msg->cmd = STOP_MIC_CMD;
	ret = msg_push(msg);
	if(0 != ret)
	{
		free(msg);
		msg = NULL;
	}

	return(true);
}


static void qq_recv_audiodata(tx_audio_encode_param *param, unsigned char *pcEncData, int nEncDataLen)
{

	int ret = -1;
	if(param->audio_format !=1 || param->encode_param != 7 || nEncDataLen <= 0)
	{
		dbg_printf("the format is wrong !\n");
		return;
	}

	if(nEncDataLen > TALK_NET_DATA_SIZE)
	{
		dbg_printf("out of size !\n");
		return;
	}

	talk_net_data_t * talk_data = calloc(1,sizeof(*talk_data));
	if(NULL == talk_data)
	{
		dbg_printf("calloc is fail !\n");
		return;
	}

	talk_data->pack_num = param->frame_per_pkg;
	talk_data->length = nEncDataLen;
	memmove(talk_data->data,pcEncData,nEncDataLen);

	ret = talk_push_net_data(talk_data);
	if(0 != ret)
	{
		free(talk_data);
		talk_data = NULL;
	}
	

}


#include <time.h>
static void  qq_fetch_history_video(unsigned int last_time, int max_count, int *count, tx_history_video_range * range_list)
{

	struct tm *now;
	now = localtime((time_t*)&last_time);
	dbg_printf("%02d-%02d-%02d  %02d:%02d:%02d\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday,now->tm_hour+8, now->tm_min, now->tm_sec);


//	replay_fetch_history_video(last_time,max_count,count,range_list);
	record_fetch_history(last_time,max_count,count,range_list);
	dbg_printf("count====%d\n",*count);

}


static  void qq_play_history_video(unsigned int play_time, unsigned long long base_time)
{

	struct tm *now;
	now = localtime((time_t*)&play_time);
	dbg_printf("play_time_%02d-%02d-%02d  %02d:%02d:%02d\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday,now->tm_hour+8, now->tm_min, now->tm_sec);
	
	if(0 == play_time)
	{
		record_replay_send_stop();
		video_send_video_start();
		voice_send_net_start();
	}
	else
	{
		record_push_replay_video_data(play_time,base_time);
	}
	


}


	


int tencent_free_dev_info(void * dev_info)
{
	if(NULL == dev_info)
	{
		dbg_printf("this  is null ! \n");
		return(-1);
	}

	tx_device_info * dev = (tx_device_info * )dev_info;

	if(NULL != dev->os_platform)
	{
		free(dev->os_platform);
		dev->os_platform  = NULL;

	}

	if(NULL != dev->device_name)
	{

		free(dev->device_name);
		dev->device_name = NULL;
	}
	
	if(NULL != dev->device_serial_number)
	{
		free(dev->device_serial_number);
		dev->device_serial_number = NULL;
	}

	if(NULL != dev->device_license)
	{
		free(dev->device_license);
		dev->device_license = NULL;
	}

	if(NULL != dev->server_pub_key)
	{

		free(dev->server_pub_key);
		dev->server_pub_key = NULL;
	}

	if(NULL != dev_info)
	{
		free(dev_info);
		dev_info = NULL;
	}

	return(0);


}



void * tencent_get_dev_info(void)
{

	int i = 0;
	char data_buff[4096];
	char * pdata = data_buff;
	unsigned int length = 0;
	FILE * id_file = NULL;
	tx_device_info  * dev_info = calloc(1,sizeof(*dev_info));
	if(NULL == dev_info)
	{
		dbg_printf("calloc is fail ! \n");
		goto fail;
	}

	memset(dev_info,'\0',sizeof(*dev_info));
	memset(&data_buff,'\0',sizeof(data_buff));
	
	id_file = fopen(TENCENT_CFG_FILE,"r");
	if(NULL == id_file)
	{
		dbg_printf("open  the file fail ! \n");
		goto fail;
	}
	length = fread(data_buff,1,4096,id_file);
	fclose(id_file);
	id_file = NULL;
	
	if(length <= 0 )
	{
		dbg_printf("read fail  ! \n");
		goto fail;
	}

	length = pdata[0] + (int)pdata[1] * 256;
	if(16 != length)
	{
		dbg_printf("the length is fail ! \n");
		goto fail;
	}
	pdata += 2;
	dev_info->device_serial_number = calloc(1,sizeof(char)*length + 1);
	if(NULL == dev_info->device_serial_number)
	{
		dbg_printf("calloc is fail ! \n");
		goto fail;

	}
	memmove(dev_info->device_serial_number,pdata,length);

	dbg_printf("device_serial_number===%s \n",dev_info->device_serial_number);



	pdata += length;
	char id_buff[1024];
	memset(&id_buff,'\0',sizeof(id_buff));
	length = pdata[0] + (int)pdata[1] * 256;
	pdata += 2;
	memmove(id_buff,pdata,length);
	dev_info->product_id = atoi(id_buff);


	pdata += length;
	length = pdata[0] + (int)pdata[1] * 256;
	pdata += 2;
	dev_info->server_pub_key = calloc(1,sizeof(char)*length + 1);
	if(NULL == dev_info->server_pub_key)
	{
		dbg_printf("calloc is fail ! \n");
		goto fail;

	}
	memmove(dev_info->server_pub_key,pdata,length);


	pdata += length;
	length = pdata[0] + (int)pdata[1] * 256;
	pdata += 2;
	dev_info->device_license= calloc(1,sizeof(char)*length*2 + 1);
	if(NULL == dev_info->device_license)
	{
		dbg_printf("calloc is fail ! \n");
		goto fail;

	}
	char licence_buff[1024];
	memset(licence_buff,'\0',sizeof(licence_buff));
	memmove(licence_buff,pdata,length);
	for(i=0;i<length;++i)
	{
		sprintf(dev_info->device_license+ i * 2, "%02x", licence_buff[i]);
	}

	asprintf(&(dev_info->os_platform),"%s","linux3.4.35");
	asprintf(&(dev_info->device_name),"%s",DEV_NAME);
	dev_info->network_type = network_type_wifi;
	dev_info->product_version = VERSION_ID;
	
	return(dev_info);

fail:

	tencent_free_dev_info(dev_info);
	dev_info = NULL;
	return(NULL);


}




void * tencent_get_notify_info(void)
{
	tx_device_notify * new_notify = calloc(1,sizeof(*new_notify));
	if(NULL == new_notify)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}

	new_notify->on_online_status = qq_online_status;
	new_notify->on_login_complete = qq_login_complete;
	new_notify->on_binder_list_change = qq_binder_list_change;
	new_notify->on_wlan_upload_register_info_success = qq_wlan_upload;

	return(new_notify);
}



int  tencent_free_notify_info(void * notify)
{
	
	if(NULL == notify)
	{
		dbg_printf("this is null ! \n");
		return(-1);
	}
	
	free(notify);
	notify = NULL;

}






void  tencent_free_path_config(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("this is null ! \n");
		return;
	}

	tx_init_path * path = (tx_init_path *)arg;
	if(NULL != path->app_path)
	{
		free(path->app_path);
		path->app_path = NULL;
	}

	if(NULL != path->system_path)
	{
		free(path->system_path);
		path->system_path = NULL;

	}

	if(NULL != path->temp_path)
	{
		free(path->temp_path);
		path->temp_path = NULL;
	}


	if(NULL != arg)
	{
		free(arg);
		arg = NULL;
	}

	
}



void * tencent_get_path_config(void)
{

	tx_init_path * new_path = calloc(1,sizeof(*new_path));
	if(NULL == new_path)
	{
		dbg_printf("this is null ! \n");
		goto fail;

	}

	asprintf(&(new_path->system_path),"%s","/etc/jffs2");
    new_path->system_path_capicity  = 1024*50;

	asprintf(&(new_path->app_path),"%s","/tmp");
    new_path->app_path_capicity  	= 1024*1024;

	
	asprintf(&(new_path->temp_path),"%s","/tmp");
    new_path->temp_path_capicity  	= 1024*1024;

	return(new_path);
	
fail:

	tencent_free_path_config(new_path);

	return(NULL);

}



void tencent_config_log(int level, const char* module, int line, const char* message)
{

//	if(0==level || 1==level)
//	printf("line:%d module:%s message:%s \n",line,module,message);
}



void * tencent_get_ipcamera_notify(void)
{
	tx_ipcamera_notify * new_notify = calloc(1,sizeof(*new_notify));
	if(NULL == new_notify)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}


	new_notify->on_control_rotate = qq_control_rotate;
	new_notify->on_set_definition = qq_set_definition;

	return(new_notify);

}


void tencent_free_ipcamera_notify(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("this is null ! \n");
		return;
	}

	free(arg);
	arg = NULL;
}



void * tencent_get_av(void)
{
	tx_av_callback * new_av = calloc(1,sizeof(*new_av));
	if(NULL == new_av)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}

	new_av->on_recv_audiodata = qq_recv_audiodata;
	new_av->on_set_bitrate = qq_set_bitrate;
	new_av->on_start_camera = qq_start_camera;
	new_av->on_stop_camera = qq_stop_camera;
	new_av->on_start_mic = qq_start_mic;
	new_av->on_stop_mic = qq_stop_mic;

	return(new_av);
	
}


void  tencent_free_av(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("this is null ! \n");
		return;
	}
	free(arg);
	arg = NULL;
}


void * tencent_get_hisplay(void)
{
	tx_history_video_notify * new_his = calloc(1,sizeof(tx_history_video_notify));
	if(NULL == new_his)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}
	new_his->on_fetch_history_video = qq_fetch_history_video;
	new_his->on_play_history_video = qq_play_history_video;

	return(new_his);

}


void  tencent_free_hisplay(void * arg)
{
	if(NULL != arg)
	{
		free(arg);
		arg = NULL;
	}

}



