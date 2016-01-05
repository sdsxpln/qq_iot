#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "amr_encode.h"
#include "common.h"
#include "sdcodec.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"amr_encode:"



static T_VOID ak_printf(T_pCSTR format, ...)
{

}
static T_pVOID ak_rec_cb_malloc( T_U32 size )
{
	return malloc( size );
}


static T_VOID ak_rec_cb_free( T_pVOID mem )
{
	return free( mem );
}


static T_BOOL ak_rec_cb_lnx_delay(T_U32 ticks)
{
	return (usleep(ticks*1000) == 0);
}


void *amr_enc_open(int nChannels, int nSampleRate,int nBitsPerSample)
{

	void * new_handle  = NULL;
	T_AUDIO_REC_INPUT 		inConfig;
	T_AUDIO_ENC_OUT_INFO	outConfig;
	bzero( &inConfig, sizeof( T_AUDIO_REC_INPUT ) );
	bzero( &outConfig, sizeof( T_AUDIO_ENC_OUT_INFO ));

	inConfig.cb_fun.Malloc = ak_rec_cb_malloc;
	inConfig.cb_fun.Free = ak_rec_cb_free;
	inConfig.cb_fun.printf = (MEDIALIB_CALLBACK_FUN_PRINTF)ak_printf;
	inConfig.cb_fun.delay = ak_rec_cb_lnx_delay;
	inConfig.enc_in_info.m_nChannel = nChannels;
	inConfig.enc_in_info.m_nSampleRate = nSampleRate;
	inConfig.enc_in_info.m_BitsPerSample = nBitsPerSample;
	inConfig.enc_in_info.m_Type = _SD_MEDIA_TYPE_AMR;
    inConfig.enc_in_info.m_private.m_amr_enc.mode = AMR_ENC_MR122;
    inConfig.chip = AUDIOLIB_CHIP_AK39XXE;
	new_handle = _SD_Encode_Open(&inConfig, &outConfig);
	if(NULL == new_handle)
	{
		dbg_printf("_SD_Encode_Open is fail !\n");
		return(NULL);
	}
	return (new_handle);
}



int  amr_enc_close(void *handle)
{

    if(NULL == handle)
    {
		dbg_printf("the amr encode handle is null !\n");
		return(-1);
	}
	_SD_Encode_Close(handle);
	
	return 0;
}


int amr_encode( void * encode_handle, unsigned char * pRawData, unsigned int nRawDataSize, unsigned char * pEncBuf, unsigned int nEncBufSize )
{

	T_AUDIO_ENC_BUF_STRC encBuf	= { NULL, NULL, 0, 0 };
	encBuf.buf_in	= pRawData;
	encBuf.len_in 	= nRawDataSize;
	encBuf.buf_out	= pEncBuf;
	encBuf.len_out	= nEncBufSize;
	return _SD_Encode(encode_handle, &encBuf);
}



