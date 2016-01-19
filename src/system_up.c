#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "system_up.h"
#include "TXDeviceSDK.h"
#include "tencent_init.h"
#include "msg_handle.h"
#include "TXIPCAM.h"
#include "video_stream.h"
#include "voice_handle.h"
#include "talk_handle.h"
#include "monitor_dev.h"
#include "fs_managed.h"
#include "muxer_media.h"
#include "demuxer_media.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"system_up:"


system_config_t * system_config_info = NULL;


void system_free_config(void * arg)
{
	if(NULL == arg)
	{
		dbg_printf("the arg is null ! \n");
		return;
	}
	system_config_t * sys = (system_config_t*)arg;
	if(NULL != sys->dev_info)
		tencent_free_dev_info(sys->dev_info);
	
	if(NULL != sys->dev_notify)
		tencent_free_notify_info(sys->dev_notify);
	
	if(NULL != sys->dev_path)
		tencent_free_path_config(sys->dev_path);

	if(NULL != sys->dev_av)
		tencent_free_av(sys->dev_av);
		
	
	if(NULL != arg);
	{
		free(arg);
		arg = NULL;
	}

}


int system_tencent_init_up(void * arg)
{
	
	if(NULL == arg)
	{
		dbg_printf("this is null ! \n");
		return(-1);
	}
	int ret = -1;
	system_config_t * sys = (system_config_t*)arg;

	sys->dev_info 		= tencent_get_dev_info();
	sys->dev_notify 	= tencent_get_notify_info();
	sys->dev_path 		= tencent_get_path_config(); 
	if(NULL == sys->dev_info || NULL==sys->dev_notify || NULL == sys->dev_path)
	{
	
		goto fail;
	}
	tx_set_log_func(tencent_config_log);
	
	ret = tx_init_device((tx_device_info*)sys->dev_info,(tx_device_notify*)sys->dev_notify,(tx_init_path*)sys->dev_path);
	if(err_null != ret)
	{
		dbg_printf("tx_init_device is fail ! \n");
		goto fail;
	}

	sys->dev_av = tencent_get_av();
	sys->dev_histry = tencent_get_hisplay();
	sys->online_status = DEV_OFFLINE;


	return(0);

fail:

	dbg_printf("system_tencent_init is fail ! \n");
	system_free_config(sys);

	return(-1);

}



int system_up(void)
{

	int ret = -1;

	dbg_printf("this is a test for it !\n");

	if(system_config_info != NULL )
	{
		dbg_printf("system has been init ! \n");
		return(-1);
	}

	system_config_info = calloc(1,sizeof(*system_config_info));
	if(NULL == system_config_info)
	{
		dbg_printf("calloc is fail ! \n");
		return(-1);
	}

	ret = akuio_pmem_init();
	if(0 != ret)
	{
		dbg_printf("akuio_pmem_init \n");
		return(-1);
	}

	fs_handle_managed_up();
	monitor_handle_dev_up();
	video_handle_stream_up();
	msg_handle_center_up();
	voice_handle_center_up();
	talk_handle_center_up();

	ret = system_tencent_init_up(system_config_info);
	if(ret != 0 )
	{
		dbg_printf("system_tencent_init is fail ! \n");
	}
	else
	{
		dbg_printf("system_tencent_init is succed ! \n");
	}



	#if 1
	muxer_media_handle_up();
	demuxer_media_handle_up();
	#else
	record_handle_center_up();
	#endif
	sleep(2);
	video_record_video_start();
	//voice_record_voice_start();

	



}

