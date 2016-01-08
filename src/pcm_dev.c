#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "common.h"
#include "pcm_lib.h"
#include "pcm_dev.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"pcm_dev:"



void *pcm_ad_open(unsigned int samplerate,unsigned int samplebits)
	
{
    void *handle;
    pcm_pars_t da_info;
    handle = pcm_open(PCM_DEV_AD);

    da_info.samplebits = samplebits;
    da_info.samplerate = samplerate;
    pcm_ioctl(handle, AD_SET_PARS, (void *)&da_info);    
    return handle;
}


void pcm_ad_filter_enable(void *handle, int enable)
{
    pcm_ioctl(handle, AD_SET_SDF, (void *)&enable);
}


void pcm_ad_clr_buffer(void *handle)
{
    pcm_ioctl(handle, SET_RSTBUF, (void *)~0);
}


void pcm_set_smaple(void *handle, unsigned int samplerate,unsigned int samplebits)
{
    pcm_pars_t ad_info;
 
    ad_info.samplebits = samplebits;
    ad_info.samplerate = samplerate;
    pcm_ioctl(handle, AD_SET_PARS, (void *)&ad_info);
}



void pcm_set_mic_vol(void *handle, int vol)
{
    pcm_ioctl(handle, AD_SET_GAIN, (void *)&vol);
}



int pcm_read_ad(void *handle, unsigned char *pdata, int buf_size, unsigned long *ts)
{
	int ret;
    
    ret = pcm_read(handle, pdata, buf_size);
	
	pcm_get_timer(handle, ts);

	return ret;
}


int pcm_ad_close(void *handle)
{
	return pcm_close(handle);
}


int pcm_set_ad_agc_enable(int enable)
{
	return pcm_set_nr_agc_enable(PCM_DEV_AD, enable);
}


int pcm_get_ad_status(void)
{
	return pcm_get_status(PCM_DEV_AD);
}




void * pcm_da_open(unsigned int samplerate,unsigned int samplebits)
{

    void *handle;
    pcm_pars_t da_info;
    handle = pcm_open(PCM_DEV_DA);
	if(NULL == handle)
	{
		dbg_printf("pcm_open is fail !\n");
		return(NULL);
	}

    da_info.samplebits = samplebits;
    da_info.samplerate = samplerate;
    pcm_ioctl(handle, DA_SET_PARS, (void *)&da_info);

    return (handle);
	
}


void pcm_da_filter_enable(void *handle, int enable)
{
    pcm_ioctl(handle, DA_SET_SDF, (void *)&enable);
}


void pcm_da_spk_enable(int enable)
{
    pcm_set_speaker_enable(enable);	
}


int pcm_write_da(void *handle, unsigned char *pdata, int buf_size)
{
    return pcm_write(handle, pdata, buf_size);
}


int pcm_da_close(void *handle)
{
	return pcm_close(handle);
}



void pcm_da_clr_buffer(void *handle)
{
    pcm_ioctl(handle, SET_RSTBUF, (void *)~0);
}


void pcm_da_set_vol(void *handle, int vol)
{
    pcm_ioctl(handle, DA_SET_GAIN, (void *)&vol);
}