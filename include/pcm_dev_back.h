#ifndef _pcm_dev_h
#define _pcm_dev_h


#define HEADPHONE_GAIN_MIN    (0)
#define HEADPHONE_GAIN_MAX    (5)
#define LINEIN_GAIN_MIN       (0)
#define LINEIN_GAIN_MAX       (15)
#define MIC_GAIN_MIN          (0)
#define MIC_GAIN_MAX          (7)


struct akpcm_pars {

	unsigned int rate;
	unsigned int channels;
	unsigned int sample_bits;
	unsigned int period_bytes; 
	unsigned int periods; 

	unsigned int threshold;
	unsigned int reserved;
};


typedef  struct adc_user
{
	int adc_dev_fd;
	unsigned char channels;
	unsigned int bitsPerSample;
	unsigned int sample_rate;
}adc_user_t;


typedef  struct dac_user
{
	int dac_dev_fd;
	unsigned char channels;
	unsigned int bitsPerSample;
	unsigned int sample_rate;
	
}dac_user_t;



int adc_open_dev(void);
int adc_close_dev(void);
void * adc_new_user(unsigned char channels,unsigned int bitsPerSample,unsigned int sample_rate);
int adc_read_data(adc_user_t * puser,unsigned char *buff, unsigned int length);


int dac_open_dev(void);
int dac_close_dev(void);
void * dac_new_user(unsigned char channels,unsigned int bitsPerSample,unsigned int sample_rate);
void dac_free_user(dac_user_t * user);
int dac_config_review(dac_user_t * puser);
int dac_write_data(dac_user_t * puser,unsigned char *buff, unsigned int length);
int speaker_enable(int value);



#endif  /*_pcm_dev_h*/

