#ifndef _pcm_lib_h
#define _pcm_lib_h



void *pcm_ad_open(unsigned int samplerate,unsigned int samplebits);
void pcm_ad_filter_enable(void *handle, int enable);
void pcm_ad_clr_buffer(void *handle);
void pcm_set_smaple(void *handle, unsigned int samplerate,unsigned int samplebits);
void pcm_set_mic_vol(void *handle, int vol);
int pcm_read_ad(void *handle, unsigned char *pdata, int buf_size, unsigned long *ts);
int pcm_ad_close(void *handle);
int pcm_set_ad_agc_enable(int enable);
int pcm_get_ad_status(void);



void * pcm_da_open(unsigned int samplerate,unsigned int samplebits);
void pcm_da_filter_enable(void *handle, int enable);
void pcm_da_spk_enable(int enable);
int pcm_write_da(void *handle, unsigned char *pdata, int buf_size);
int pcm_da_close(void *handle);
void pcm_da_clr_buffer(void *handle);
void pcm_da_set_vol(void *handle, int vol);






#endif /*_pcm_lib_h*/


