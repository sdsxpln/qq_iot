#ifndef _amr_decode_h
#define _amr_decode_h






int amr_decode_data(void * handle,unsigned char * data,int length,unsigned char * out_buff,int out_max_size);
void * amr_decode_open(void);


#endif

