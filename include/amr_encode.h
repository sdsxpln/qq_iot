#ifndef _amr_encode_h
#define _amr_encode_h


#define  FRAME_NUM_PER_PACKET	(8)
#define	 ENCODE_PCM_SIZE	(FRAME_NUM_PER_PACKET*320)
#define	 ENCODE_AMR_SIZE	(FRAME_NUM_PER_PACKET*32)


void *amr_enc_open(int nChannels, int nSampleRate,int nBitsPerSample);
int  amr_enc_close(void *handle);
int amr_encode( void * encode_handle, unsigned char * pRawData, unsigned int nRawDataSize, unsigned char * pEncBuf, unsigned int nEncBufSize );

#endif /*_amr_encode_h*/