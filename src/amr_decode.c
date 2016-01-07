#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "amr_decode.h"
#include "sdcodec.h"
#include "common.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"amr_decode:"


static T_VOID ak_printf(T_pCSTR format, ...)
{

}

static T_BOOL ak_decode_lnx_delay(T_U32 ticks)
{
	return (usleep(ticks*1000) == 0);
}



void * amr_decode_open(void)
{
    T_AUDIO_DECODE_INPUT audio_input;
	T_AUDIO_DECODE_OUT out_info;
	void * new_handle = NULL;
    memset(&audio_input, 0, sizeof(T_AUDIO_DECODE_INPUT));
    audio_input.cb_fun.Free = (MEDIALIB_CALLBACK_FUN_FREE) free;
    audio_input.cb_fun.Malloc =  (MEDIALIB_CALLBACK_FUN_MALLOC) malloc;
    audio_input.cb_fun.printf = (MEDIALIB_CALLBACK_FUN_PRINTF) ak_printf;
    audio_input.cb_fun.delay = ak_decode_lnx_delay;
    audio_input.m_info.m_Type = _SD_MEDIA_TYPE_AMR;
	
    audio_input.m_info.m_InbufLen = 20*1024;
    audio_input.m_info.m_SampleRate = 8000;
    audio_input.m_info.m_Channels = 1;
    audio_input.m_info.m_BitsPerSample =16;
    audio_input.chip = AUDIOLIB_CHIP_AK39XXE;


    new_handle = _SD_Decode_Open(&audio_input, &out_info);
    if (new_handle == AK_NULL)
    {
		dbg_printf("_SD_Decode_Open is fail !\n");
		return(NULL);
    }

	/** 设置解码属性，当前设置为有数据即刻解码，无需等待够4K数据量 **/
    _SD_SetBufferMode(new_handle, _SD_BM_NORMAL);
	_SD_SetInbufMinLen(new_handle, 32);

    return new_handle;
}


int amr_decode_data(void * handle,unsigned char * data,int length,unsigned char * out_buff,int out_max_size)
{

	int free_size = 0;
    T_AUDIO_BUF_STATE bufstate;
    T_AUDIO_BUFFER_CONTROL bufctrl;
	bufstate  = _SD_Buffer_Check(handle,&bufctrl);
	

	if(_SD_BUFFER_WRITABLE == bufstate)
	{	
		free_size = bufctrl.free_len;
	}
	else if(_SD_BUFFER_WRITABLE_TWICE == bufstate)
	{	
		free_size = bufctrl.free_len + bufctrl.start_len;
	}
	else if(_SD_BUFFER_FULL	== bufstate  || _SD_BUFFER_ERROR==bufstate)
	{
		return(-1);
	}

	if(free_size < length)
	{
		dbg_printf("the free buff is small than need !\n");
		return(-1);
	}

	if(_SD_BUFFER_WRITABLE == bufstate)
	{	
		memmove((void *)bufctrl.pwrite, data, length);
		_SD_Buffer_Update(handle, length);

	}
	else if(_SD_BUFFER_WRITABLE_TWICE == bufstate)
	{	
		if(length <= bufctrl.free_len)
		{
			memmove((void *)bufctrl.pwrite, data, length);
			_SD_Buffer_Update(handle, length);
		}
		else 
		{
			memmove((void *)bufctrl.pwrite, data, bufctrl.free_len);
			_SD_Buffer_Update(handle, bufctrl.free_len);
			memmove((void *)bufctrl.pstart, data+bufctrl.free_len, length-bufctrl.free_len);
			_SD_Buffer_Update(handle, length-bufctrl.free_len);
			
		}
	}
 
	T_AUDIO_DECODE_OUT out_put;
	int out_length = 0;
	bzero(&out_put,sizeof(out_put));
	out_put.m_pBuffer = out_buff;
	out_put.m_ulSize = out_max_size;
	out_put.m_BitsPerSample = 16;
	out_put.m_Channels =1;
	out_put.m_SampleRate = 8000;
	int real_length = 0;
	do
	{
		out_length = _SD_Decode(handle, &out_put);
		real_length += out_length;
	}while(out_length > 0 );
	
	return(real_length);
}



