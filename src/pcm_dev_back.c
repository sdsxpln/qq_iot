#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "common.h"
#include "pcm_dev.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"pcm_dev:"


#define PCM_IOC_MAGIC			'P'
#define AKPCM_IO(nr)			_IOC(_IOC_NONE, PCM_IOC_MAGIC, nr, 0)
#define AKPCM_IOR(nr)			_IOR(PCM_IOC_MAGIC, nr, int)
#define AKPCM_IORn(nr, size)	_IOR(PCM_IOC_MAGIC, nr, size)
#define AKPCM_IOW(nr)			_IOW(PCM_IOC_MAGIC, nr, int)
#define AKPCM_IOWn(nr, size)	_IOW(PCM_IOC_MAGIC, nr, size)

#define IOC_NR_PREPARE		(0xE0)
#define IOC_NR_RESUME		(0xE1)
#define IOC_NR_PAUSE		(0xE2)
#define IOC_NR_RSTBUF		(0xE3)
#define IOC_NR_RSTALL		(0xE4)
#define IOC_NR_GETELAPSE	(0xE5)
#define IOC_NR_GETTIMER		(0xE6)
#define IOC_NR_GETSTATUS	(0xE7)
#define IOC_PREPARE			AKPCM_IO(IOC_NR_PREPARE)
#define IOC_RESUME			AKPCM_IO(IOC_NR_RESUME)
#define IOC_PAUSE			AKPCM_IO(IOC_NR_PAUSE)
#define IOC_RSTBUF			AKPCM_IO(IOC_NR_RSTBUF)
#define IOC_RSTALL			AKPCM_IO(IOC_NR_RSTALL)
#define IOC_GETELAPSE		AKPCM_IORn(IOC_NR_GETELAPSE, unsigned long long)
#define IOC_GETTIMER		AKPCM_IORn(IOC_NR_GETTIMER, unsigned long)
#define IOC_GET_STATUS		AKPCM_IORn(IOC_NR_GETSTATUS, unsigned long)

#define IOC_NR_FEATS		(0xF0) 
#define IOC_NR_PARS			(0xF2) 
/*#define IOC_GET_FEATS		AKPCM_IORn(IOC_NR_FEATS, struct akpcm_features)*/
#define IOC_GET_PARS		AKPCM_IORn(IOC_NR_PARS, struct akpcm_pars)
#define IOC_SET_PARS		AKPCM_IOWn(IOC_NR_PARS, struct akpcm_pars)
#define IOC_NR_SOURCES		(0x10) 
#define IOC_GET_SOURCES		AKPCM_IOR(IOC_NR_SOURCES)
#define IOC_SET_SOURCES		AKPCM_IOW(IOC_NR_SOURCES) 
#define IOC_NR_GAIN			(0x30) 
#define IOC_GET_GAIN		AKPCM_IOR(IOC_NR_GAIN)
#define IOC_SET_GAIN		AKPCM_IOW(IOC_NR_GAIN) 
#define IOC_NR_DEV			(0x70)
#define IOC_GET_DEV			AKPCM_IOR(IOC_NR_DEV)
#define IOC_SET_DEV			AKPCM_IOW(IOC_NR_DEV)
#define IOC_NR_AGC			(0x80)
#define IOC_SET_NR_AGC		AKPCM_IOW(IOC_NR_AGC)
#define IOC_SPEAKER			(0x81)
#define IOC_SET_SPEAKER		AKPCM_IOW(IOC_SPEAKER)


#define AKPCM_PLAYDEV_HP	(1UL<<0)
#define AKPCM_PLAYDEV_LO	(1UL<<1)
#define AKPCM_PLAYDEV_AUTO	(0UL<<0)

#define AKPCM_CPTRDEV_MIC	(1UL<<0)
#define AKPCM_CPTRDEV_LI	(1UL<<1)
#define AKPCM_CPTRDEV_AUTO	(0UL<<0)
#define SOURCE_DAC           (0b001)
#define SOURCE_MIC           (0b100)

#define 	BIT_AEC				(1)
#define 	BIT_NR				(1<<1)
#define 	BIT_AGC				(1<<2)
#define 	BIT_NR_AGC			(BIT_NR | BIT_AGC)


#undef  	ADC_DEV
#define 	ADC_DEV 	"/dev/akpcm_cdev1"


#define 	ADC_MAX_READ_SIZE	(4096)
#define		ADC_SAMPLE_RATE		7900  
#define		ADC_CHANNELS		(1)
#define		ADC_SAMPLE_BITs		(16)


typedef struct adc_handle
{
	char adc_is_open;
	int adc_dev_fd;
	adc_user_t * cur_user;

}adc_handle_t;


static adc_handle_t * padc_handle = NULL;


static int adc_config_default(adc_handle_t * handle)
{
	int ret = -1;
	int value = 0;
	if(NULL == handle || handle->adc_dev_fd < 0 )
	{
		dbg_printf("dev not open ! \n");
		return(-1);
	}


	value = 0;
	ret = ioctl(handle->adc_dev_fd, IOC_PAUSE, (void *)(&value));
	if (ret < 0)
	{
		dbg_printf("IOC_PAUSE  fail ! \n");
		return -1;
	}

	value = AKPCM_CPTRDEV_MIC;
	ret = ioctl(handle->adc_dev_fd, IOC_SET_DEV, (void *)(&value));
	if(ret < 0 )
	{
		dbg_printf("IOC_SET_DEV  fail ! \n");
		return(-1);
	}
	value = SOURCE_MIC;
	ret = ioctl(handle->adc_dev_fd, IOC_SET_SOURCES, (void *)(&value));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_SOURCES  fail ! \n");
		return(-2);
	}


#if 0
	value = MIC_GAIN_MAX;
	ret = ioctl(handle->adc_dev_fd, IOC_SET_GAIN, (void *)(&value));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_GAIN  fail ! \n");
		return(-3);
	}


	value = BIT_NR_AGC;
	ret = ioctl(handle->adc_dev_fd, IOC_SET_NR_AGC, (void *)&value);
#endif
	struct akpcm_pars temp;
	temp.rate = ADC_SAMPLE_RATE;
	temp.channels = ADC_CHANNELS;
	temp.sample_bits = ADC_SAMPLE_BITs;
	temp.period_bytes = ADC_MAX_READ_SIZE;
	temp.periods = 8;
	temp.threshold = temp.period_bytes;
	
	ret = ioctl(handle->adc_dev_fd, IOC_SET_PARS, (void *)(&(temp)));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_PARS  fail ! \n");
		return(-4);
	}


	value = 0;
//	value |= BIT_NR_AGC;
	ret = ioctl(handle->adc_dev_fd, IOC_RESUME, (void *)&value);
	if (ret < 0) 
	{
		dbg_printf("IOC_RESUME  fail ! \n");
		return(-5);
	}


	usleep(1000);
	return(0);
}


static int adc_config_review(adc_user_t * puser)
{

	int ret = -1;
	int value = 0;
	if(NULL == puser || puser->adc_dev_fd <= 0)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	value = 0;
	ret = ioctl(puser->adc_dev_fd, IOC_PAUSE, (void *)(&value));
	if (ret < 0)
	{
		dbg_printf("IOC_PAUSE  fail ! \n");
		return (-2);
	}
	struct akpcm_pars temp;
	temp.rate = puser->sample_rate;
	temp.channels = puser->channels;
	temp.sample_bits = puser->bitsPerSample;
	temp.period_bytes = ADC_MAX_READ_SIZE;
	temp.periods = 8;
	temp.threshold = temp.period_bytes;
	
	ret = ioctl(puser->adc_dev_fd, IOC_SET_PARS, (void *)(&(temp)));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_PARS  fail ! \n");
		return(-3);
	}
	value = 0;
	ret = ioctl(puser->adc_dev_fd, IOC_RESUME, (void *)&value);
	if (ret < 0) 
	{
		dbg_printf("IOC_RESUME  fail ! \n");
		return(-4);
	}
	usleep(1000);
	return(0);
}



int adc_open_dev(void)
{

	int ret = -1;
	if(NULL != padc_handle)
	{
		dbg_printf("the dev has been inited ! \n");
		return(-1);
	}
	padc_handle = calloc(1,sizeof(*padc_handle));
	if(NULL == padc_handle)
	{
		dbg_printf("malloc fail ! \n");
		return(-2);
	}
	
	
	padc_handle->adc_dev_fd = open(ADC_DEV,O_RDONLY);
	if(padc_handle->adc_dev_fd < 0 )
	{
		dbg_printf("open dev fail ! \n");
		return(-2);
	}
	fcntl(padc_handle->adc_dev_fd, F_SETFD, FD_CLOEXEC);

	ret = adc_config_default(padc_handle);
	if(ret != 0 )
	{
		dbg_printf("adc_config_param  fail ! \n");
		close(padc_handle->adc_dev_fd);
		free(padc_handle);
		padc_handle = NULL;
		return(-3);

	}

	
	padc_handle->adc_is_open = 1;

	padc_handle->cur_user = NULL;
		
	return(0);
}


int adc_close_dev(void)
{
	if(NULL == padc_handle)return(-1);
	if(padc_handle->adc_dev_fd > 0)
	{
		close(padc_handle->adc_dev_fd);
		padc_handle->adc_is_open = 0;
	}
	free(padc_handle);
	padc_handle = NULL;

	return(0);
}


void * adc_new_user(unsigned char channels,unsigned int bitsPerSample,unsigned int sample_rate)
{

	if(NULL ==padc_handle ||  1 != padc_handle->adc_is_open)
	{
		dbg_printf("please init the adc dev ! \n");
		return(NULL);

	}
	
	adc_user_t  * new_one = calloc(1,sizeof(*new_one));
	if(NULL == new_one)
	{
		dbg_printf("calloc fail ! \n");
		return(NULL);
	}

	new_one->channels = channels;
	new_one->bitsPerSample = bitsPerSample;
	new_one->sample_rate = sample_rate;
	new_one->adc_dev_fd = padc_handle->adc_dev_fd;

	return(new_one);
	
}


void adc_free_user(adc_user_t * user)
{
	if(NULL == user)return;
	free(user);
	user = NULL;
}


int adc_read_data(adc_user_t * puser,unsigned char *buff, unsigned int length)
{

	int read_length = 0;
	int total_length = 0;
	int need_length = 0;
	int ret = -1;
	if(NULL == puser || puser->adc_dev_fd <= 0  || NULL == buff  || 0 == length )
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	if(length > ADC_MAX_READ_SIZE)
	{
		dbg_printf("the length of read out of limit ! \n");
		return(-2);
	}

	need_length = length;
	while(total_length < length)
	{
		read_length = read(puser->adc_dev_fd,buff+total_length,need_length);
		total_length += read_length;
		need_length -= read_length;
	}
	
	return(total_length);

}




/*****************************************************************************************************************************/

/**										DAC																		*/
/*****************************************************************************************************************************/

#define 	DAC_MAX_WRITE_SIZE	(4096)
#define		DAC_SAMPLE_RATE		(8000)
#define		DAC_CHANNELS		(2)
#define		DAC_SAMPLE_BITs		(16)

#undef  	DAC_DEV
#define 	DAC_DEV 		"/dev/akpcm_cdev0"



typedef struct dac_handle
{
	char dac_is_open;
	int dac_dev_fd;
	dac_user_t * cur_user;

}dac_handle_t;


static dac_handle_t * pdac_handle = NULL;


static int dac_config_default(dac_handle_t * handle)
{
	int ret = -1;
	int value = 0;
	if(NULL == handle || handle->dac_dev_fd < 0 )
	{
		dbg_printf("dev not open ! \n");
		return(-1);
	}


	unsigned long dac_is_busy = 1;
	while(dac_is_busy)
	{
		ioctl(handle->dac_dev_fd, IOC_GET_STATUS, (void *)&dac_is_busy);
		usleep(1000);
	}

	value = AKPCM_PLAYDEV_HP;
	ret = ioctl(handle->dac_dev_fd, IOC_SET_DEV, (void *)(&value));
	if(ret < 0 )
	{
		dbg_printf("IOC_SET_DEV  fail ! \n");
		return(-1);
	}


	value = SOURCE_DAC;
	ret = ioctl(handle->dac_dev_fd, IOC_SET_SOURCES, (void *)(&value));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_SOURCES  fail ! \n");
		return(-2);
	}


	
	value = HEADPHONE_GAIN_MAX;
	ret = ioctl(handle->dac_dev_fd, IOC_SET_GAIN, (void *)(&value));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_GAIN  fail ! \n");
		return(-3);
	}

#if 0
	value = BIT_NR_AGC;
	ret = ioctl(handle->dac_dev_fd, IOC_SET_NR_AGC, (void *)&value);
#endif
	
	struct akpcm_pars temp;
	temp.rate = DAC_SAMPLE_RATE;
	temp.channels = DAC_CHANNELS;
	temp.sample_bits = DAC_SAMPLE_BITs;
	temp.period_bytes = DAC_MAX_WRITE_SIZE;
	temp.periods = 8;
	temp.threshold = temp.period_bytes;
	
	ret = ioctl(handle->dac_dev_fd, IOC_SET_PARS, (void *)(&(temp)));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_PARS  fail ! \n");
		return(-4);
	}
	
	usleep(1000);
	return(0);
}


int dac_config_review(dac_user_t * puser)
{

	int ret = -1;
	int value = 0;
	if(NULL == puser || puser->dac_dev_fd <= 0)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}


	int dac_is_busy = 1;
	while(dac_is_busy)
	{
		ioctl(puser->dac_dev_fd, IOC_GET_STATUS, (void *)&dac_is_busy);
		usleep(1000);
	}


	struct akpcm_pars temp;
	temp.rate = puser->sample_rate;
	temp.channels = puser->channels;
	temp.sample_bits = puser->bitsPerSample;
	temp.period_bytes = ADC_MAX_READ_SIZE;
	temp.periods = 8;
	temp.threshold = temp.period_bytes;
	
	ret = ioctl(puser->dac_dev_fd, IOC_SET_PARS, (void *)(&(temp)));
	if (ret < 0) 
	{
		dbg_printf("IOC_SET_PARS  fail ! \n");
		return(-3);
	}

	usleep(1000);
	return(0);
}



int dac_open_dev(void)
{

	int ret = -1;
	if(NULL != pdac_handle)
	{
		dbg_printf("the dev has been inited ! \n");
		return(-1);
	}
	pdac_handle = calloc(1,sizeof(*pdac_handle));
	if(NULL == pdac_handle)
	{
		dbg_printf("malloc fail ! \n");
		return(-2);
	}
	
	
	pdac_handle->dac_dev_fd = open(DAC_DEV,O_WRONLY);
	if(pdac_handle->dac_dev_fd < 0 )
	{
		dbg_printf("open dev fail ! \n");
		return(-2);
	}


	fcntl(pdac_handle->dac_dev_fd, F_SETFD, FD_CLOEXEC);


	ret = dac_config_default(pdac_handle);
	if(ret != 0 )
	{
		dbg_printf("adc_config_param  fail ! \n");
		close(pdac_handle->dac_dev_fd);
		free(pdac_handle);
		pdac_handle = NULL;
		return(-3);

	}



	pdac_handle->dac_is_open = 1;

	pdac_handle->cur_user = NULL;
		
	return(0);
}


int dac_close_dev(void)
{
	if(NULL == pdac_handle)return(-1);
	if(pdac_handle->dac_dev_fd > 0)
	{
		close(pdac_handle->dac_dev_fd);
		pdac_handle->dac_is_open = 0;
	}
	free(pdac_handle);
	pdac_handle = NULL;

	return(0);
}


void * dac_new_user(unsigned char channels,unsigned int bitsPerSample,unsigned int sample_rate)
{

	if(NULL ==pdac_handle ||  1 != pdac_handle->dac_is_open)
	{
		dbg_printf("please init the dac dev ! \n");
		return(NULL);

	}
	
	dac_user_t  * new_one = calloc(1,sizeof(*new_one));
	if(NULL == new_one)
	{
		dbg_printf("calloc fail ! \n");
		return(NULL);
	}

	new_one->channels = channels;
	new_one->bitsPerSample = bitsPerSample;
	new_one->sample_rate = sample_rate;
	new_one->dac_dev_fd = pdac_handle->dac_dev_fd;

	return(new_one);
	
}


void dac_free_user(dac_user_t * user)
{
	if(NULL == user)return;
	free(user);
	user = NULL;
}


int dac_write_data(dac_user_t * puser,unsigned char *buff, unsigned int length)
{

	int read_length = 0;
	int ret = -1;
	if(NULL == puser || puser->dac_dev_fd <= 0  || NULL == buff  || 0 == length )
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	if(length > DAC_MAX_WRITE_SIZE )
	{
		dbg_printf("the length of write out of limit ! \n");
		return(-2);
	}


#if 0
	if(puser != pdac_handle->cur_user)
	{
		ret = dac_config_review(puser);
		if(ret != 0)
		{
			dbg_printf("adc_config_review fail ! \n");
			return(-3);
		}
		pdac_handle->cur_user = puser;

	}
#endif

	read_length = write(puser->dac_dev_fd,buff,length);
	
	return(read_length);

}



int speaker_enable(int value)
{
	int ret = -1;
	ret = ioctl(pdac_handle->dac_dev_fd, IOC_SET_SPEAKER, (void *)&value);
	if (ret < 0)
	{
		dbg_printf("dac ioctl set speaker failed.\n");
		return (-1);
	}

	return(0);
}