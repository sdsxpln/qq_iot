#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include "amr_loadlib.h"
#include "amr_decode.h"
#include "common.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"amr_decode:"


#undef  AMR_FILE_MAGIC
#define AMR_FILE_MAGIC	"#!AMR\n"


static volatile int amr_decode_lib = 0;


int amr_decodelib_open(void)
{
	int ret = -1;
	if(0  != amr_decode_lib)
	{
		dbg_printf("the lib has been init ! \n");
		return(-1);
	}

	ret = loadlib_amrdec_init();
	if(0 == ret)
	{
		amr_decode_lib = 1;	
	}
	return(ret);
}


int amr_decodelib_close(void)
{
	loadlib_amrdec_deinit();
	amr_decode_lib = 0;

	return(0);
}



void * amr_new_decode(unsigned char channels,unsigned int bitsPerSample,unsigned int sample_rate,enum arm_header_mode  mode)
{

	if(0 == amr_decode_lib)
	{
		dbg_printf("please init the amr_encode_lib ! \n");
		return(NULL);
	}
	amr_decode_handle_t * new_handle = calloc(1,sizeof(amr_decode_handle_t));
	if(NULL == new_handle)
	{
		dbg_printf("calloc is fail ! \n");
		return(NULL);
	}

	new_handle->channels = channels;
	new_handle->bitsPerSample = bitsPerSample;
	new_handle->sample_rate = sample_rate;
	new_handle->mode = mode; 

	new_handle->handle = Decoder_Interface_init();
	if(NULL == new_handle->handle)
	{
		dbg_printf("Decoder_Interface_init fail ! \n");
		return(NULL);
	}

	return(new_handle);
	
}


void amr_free_dehandle(amr_decode_handle_t * decode_handle)
{
	if(NULL != decode_handle)
	{
		if(NULL != decode_handle->handle)
		{
			Decoder_Interface_exit(decode_handle->handle);
			decode_handle->handle = NULL;
		}
		free(decode_handle);
		decode_handle = NULL;
	}
}


static int arm_get_frame_size(enum arm_header_mode mode)
{
	switch(mode)
	{
		case AMR_MR475_HEADER: 	return 	(13) ;
		case AMR_MR515_HEADER: 	return 	(14) ;
		case AMR_MR59_HEADER: 	return 	(16) ;
		case AMR_MR67_HEADER: 	return 	(18) ;
		case AMR_MR74_HEADER: 	return 	(20) ;
		case AMR_MR795_HEADER: 	return 	(21) ;
		case AMR_MR102_HEADER: 	return  (27) ;
		case AMR_MR122_HEADER: 	return  (32) ;
		default : return 0;

	}
	return(0);

}

int amr_file_amrtopcm(amr_decode_handle_t  * pamr_decode,pfun_arm_decode pfun,void * user,const unsigned char * path)
{

	amr_decode_handle_t * phandle = pamr_decode;
	if(NULL ==phandle || NULL ==phandle->handle)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	if(NULL == path)
	{
		dbg_printf("check the param ! \n");
		return(-2);
	}

	if(access(path,F_OK) != 0 )
	{
		dbg_printf("the file is not exit ! \n");
		return(-3);
	}

	FILE * pamr_file = NULL;

	pamr_file = fopen(path,"r");
	if(NULL == pamr_file)
	{
		dbg_printf("open the file fail ! \n");
		return(-4);
	}

	unsigned int read_length = 0;
	unsigned char check_file_type[6];

	read_length = fread(check_file_type,1,6,pamr_file);
	if(6 != read_length || 0 != strncmp(check_file_type,AMR_FILE_MAGIC,6))
	{
		dbg_printf("the file type is wrong ! \n");
		fclose(pamr_file);
		return(-5);
	}

	unsigned char header_mode;
	read_length = fread(&header_mode,1,1,pamr_file);
	if(read_length != 1)
	{
		dbg_printf("the file is wrong type ! \n");
		return(-6);
	}

	dbg_printf("the hearder mode is 0x%x \n",header_mode);
	unsigned int frame_size = arm_get_frame_size(header_mode);
	if(0 == frame_size)
	{
		dbg_printf("the file is bad ! \n");
		return(-7);
	}
	fseek(pamr_file,6,SEEK_SET);

	int pcm_frame_szie = 0;
	pcm_frame_szie = phandle->sample_rate / 50;
	unsigned char amr_data[frame_size];
	unsigned char out_buff[pcm_frame_szie*2];

	while(! feof(pamr_file))
	{
		amr_data[0] = 0xFF;
		while(! feof(pamr_file))
		{
			 fread(&amr_data[0],1,1,pamr_file);
			 if(header_mode == amr_data[0])break;
		}
		if(0xFF == amr_data[0])break;

		read_length = fread(&amr_data[1],1,frame_size-1,pamr_file);
		if((frame_size-1) != read_length )continue;
		Decoder_Interface_Decode(phandle->handle, amr_data, (short*) out_buff, 0);
		if(NULL != pfun)
		{
			pfun(out_buff,pcm_frame_szie*2,user);
		}
	}

	fclose(pamr_file);
	
	return(0);
}





int amr_buff_amrtopcm(amr_decode_handle_t  * pamr_decode,pfun_arm_decode pfun,void * user,unsigned char * buff,unsigned int length )
{

	amr_decode_handle_t * phandle = pamr_decode;
	if(NULL ==phandle || NULL ==phandle->handle)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}

	if(NULL == buff || 0 == length)
	{
		dbg_printf("check the param ! \n");
		return(-2);
	}
	
	unsigned int frame_size = arm_get_frame_size(phandle->mode);
	if(0 == frame_size)
	{
		dbg_printf("check the  amr mode ! \n");
		return(-3);
	}

	int pcm_frame_szie = 0;
	pcm_frame_szie = phandle->sample_rate / 50;
	unsigned char amr_data[frame_size];
	unsigned char out_buff[pcm_frame_szie*2];
	unsigned int read_length = 0;
	while(read_length <=  length)
	{
		if(buff[read_length] != phandle->mode)
		{
			read_length +=  1;
			continue;
		}
		else
		{
			memmove(amr_data,&buff[read_length],frame_size);
			read_length += frame_size;
			Decoder_Interface_Decode(phandle->handle, amr_data, (short*) out_buff, 0);
			if(NULL != pfun)
			{
				pfun(out_buff,pcm_frame_szie*2,user);
			}
		}
	}

	return(0);
}





