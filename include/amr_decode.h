#ifndef _amr_decode_h
#define _amr_decode_h


enum arm_header_mode 
{
	AMR_MR475_HEADER 	= 0x04,
	AMR_MR515_HEADER 	= 0x0C,   
	AMR_MR59_HEADER 	= 0x14,    
	AMR_MR67_HEADER 	= 0x1c,     
	AMR_MR74_HEADER 	= 0x24,  
	AMR_MR795_HEADER 	= 0x2c,    
	AMR_MR102_HEADER 	= 0x34,    
	AMR_MR122_HEADER 	= 0x3c,    
	AMR_MRDTX_HEADER,   
	AMR_N_MODES_HEADER  

};

typedef  struct amr_decode_handle
{
	void * handle;
	unsigned char channels;
	unsigned int bitsPerSample;
	unsigned int sample_rate;
	enum arm_header_mode  mode;

}amr_decode_handle_t;




typedef void * (*pfun_arm_decode)(void *data,unsigned int length,void * puser);


int amr_decodelib_open(void);
int amr_decodelib_close(void);
void * amr_new_decode(unsigned char channels,unsigned int bitsPerSample,unsigned int sample_rate,enum arm_header_mode  mode);
void amr_free_dehandle(amr_decode_handle_t * decode_handle);
int amr_file_amrtopcm(amr_decode_handle_t  * pamr_decode,pfun_arm_decode pfun,void * user,const unsigned char * path);
int amr_buff_amrtopcm(amr_decode_handle_t  * pamr_decode,pfun_arm_decode pfun,void * user,unsigned char * buff,unsigned int length );





#endif

