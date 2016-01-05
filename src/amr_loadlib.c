#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include "amr_loadlib.h"
#include "common.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"loadlib:"


typedef void* (*Encoder_Interface_init_pfun)(int dtx);
typedef void (*Encoder_Interface_exit_pfun)(void* state);
typedef int (*Encoder_Interface_Encode_pfun)(void* state, enum amr_mode mode, const short* speech, unsigned char* out, int forceSpeech);



static  Encoder_Interface_init_pfun  Encoder_Interface_init_amr = NULL;
static  Encoder_Interface_exit_pfun	 Encoder_Interface_exit_amr = NULL;
static  Encoder_Interface_Encode_pfun  Encoder_Interface_Encode_amr = NULL;

#define	 AMR_ENC_LIB		"/tmp/libamr_enc.so"

static  void *pamrenc_lib = NULL;


int loadlib_amrenc_init(void)
{
	if(NULL != pamrenc_lib)
	{
		dbg_printf("lib has been init ! \n");
		return(-1);
	}
	if(access(AMR_ENC_LIB,F_OK) != 0 )
	{
		dbg_printf("the lib is not exit ! \n");
		return(-2);
	}
	pamrenc_lib = dlopen(AMR_ENC_LIB, RTLD_NOW | RTLD_GLOBAL);
	if(NULL == pamrenc_lib)
	{
		dbg_printf("open the lib fail ! \n");
		return(-3);
	}
	
	Encoder_Interface_init_amr= dlsym(pamrenc_lib,"Encoder_Interface_init");
	if(NULL == Encoder_Interface_init_amr)
	{
		dbg_printf("Encoder_Interface_init_amr fail ! \n");
	}

	Encoder_Interface_exit_amr= dlsym(pamrenc_lib,"Encoder_Interface_exit");
	if(NULL == Encoder_Interface_exit_amr)
	{
		dbg_printf("Encoder_Interface_exit_amr fail ! \n");
	}

	Encoder_Interface_Encode_amr= dlsym(pamrenc_lib,"Encoder_Interface_Encode");
	if(NULL == Encoder_Interface_Encode_amr)
	{
		dbg_printf("Encoder_Interface_Encode_amr fail ! \n");
	}


	
	return(0);
}


int loadlib_amrenc_deinit(void)
{
	if(NULL != pamrenc_lib)
	{
		dlclose(pamrenc_lib);	
		pamrenc_lib = NULL;
		
	}

	return(0);
}

void* Encoder_Interface_init(int dtx)
{
	if(NULL == Encoder_Interface_init_amr)
	{
		dbg_printf("null ! \n");
		return NULL;
	}
	return(Encoder_Interface_init_amr(dtx));
}


void Encoder_Interface_exit(void* state)
{

	if(NULL == Encoder_Interface_exit_amr)
	{
		dbg_printf("null ! \n");
		return;
	}
	Encoder_Interface_exit_amr(state);
}

int Encoder_Interface_Encode(void* state, enum amr_mode mode, const short* speech, unsigned char* out, int forceSpeech)
{
	if(NULL == Encoder_Interface_Encode_amr)
	{
		dbg_printf("null ! \n");
		return(-1);
	}
	return(Encoder_Interface_Encode_amr(state, mode, speech,out, forceSpeech));
}




typedef void* (*Decoder_Interface_init_pfun)(void);
typedef void (*Decoder_Interface_exit_pfun)(void* state);
typedef void (*Decoder_Interface_Decode_pfun)(void* state, const unsigned char* in, short* out, int bfi);


static Decoder_Interface_init_pfun  Decoder_Interface_init_amr = NULL;
static Decoder_Interface_exit_pfun  Decoder_Interface_exit_amr = NULL;
static Decoder_Interface_Decode_pfun  Decoder_Interface_Decode_amr = NULL;


#define	 AMR_DEC_LIB		"/tmp/libamr_dec.so"
static  void *pamrdec_lib = NULL;





int loadlib_amrdec_init(void)
{
	if(NULL != pamrdec_lib)
	{
		dbg_printf("lib has been init ! \n");
		return(-1);
	}
	if(access(AMR_DEC_LIB,F_OK) != 0 )
	{
		dbg_printf("the lib is not exit ! \n");
		return(-2);
	}
	pamrdec_lib = dlopen(AMR_DEC_LIB, RTLD_NOW | RTLD_GLOBAL);
	if(NULL == pamrdec_lib)
	{
		dbg_printf("open the lib fail ! \n");
		return(-3);
	}


	Decoder_Interface_init_amr= dlsym(pamrdec_lib,"Decoder_Interface_init");
	if(NULL == Decoder_Interface_init_amr)
	{
		dbg_printf("Decoder_Interface_init_amr fail ! \n");
	}

	Decoder_Interface_exit_amr= dlsym(pamrdec_lib,"Decoder_Interface_exit");
	if(NULL == Decoder_Interface_exit_amr)
	{
		dbg_printf("Decoder_Interface_exit_amr fail ! \n");
	}

	Decoder_Interface_Decode_amr= dlsym(pamrdec_lib,"Decoder_Interface_Decode");
	if(NULL == Decoder_Interface_Decode_amr)
	{
		dbg_printf("Decoder_Interface_Decode_amr fail ! \n");
	}

	return(0);
}



int loadlib_amrdec_deinit(void)
{
	if(NULL != pamrdec_lib)
	{
		dlclose(pamrdec_lib);	
		pamrdec_lib = NULL;
	}
	return (0);
}

void* Decoder_Interface_init(void)
{
	if(NULL == Decoder_Interface_init_amr)
	{
		dbg_printf("null ! \n");
		return(NULL);
	}
	return(Decoder_Interface_init_amr());

}


void Decoder_Interface_exit(void* state)
{

	if(NULL == Decoder_Interface_exit_amr)
	{
		dbg_printf("null ! \n");
		return;
	}
	Decoder_Interface_exit_amr(state);

}


void Decoder_Interface_Decode(void* state, const unsigned char* in, short* out, int bfi)
{
	if(NULL == Decoder_Interface_Decode_amr)
	{
		dbg_printf("null ! \n");
		return;
	}
	Decoder_Interface_Decode_amr(state, in, out, bfi);
}



