#ifndef _loadlib_h
#define	 _loadlib_h


enum amr_mode {
	AMR_MR475 = 0,/* 4.75 kbps frame=13*/
	AMR_MR515,    /* 5.15 kbps frame=14*/
	AMR_MR59,     /* 5.90 kbps frame=16*/
	AMR_MR67,     /* 6.70 kbps frame=18*/
	AMR_MR74,     /* 7.40 kbps frame=20*/
	AMR_MR795,    /* 7.95 kbps frame=21*/
	AMR_MR102,    /* 10.2 kbps frame=27*/
	AMR_MR122,    /* 12.2 kbps frame=32*/
	AMR_MRDTX,    /* DTX       frame=xx*/
	AMR_N_MODES   /* Not Used  */
};

int loadlib_amrenc_init(void);
int loadlib_amrenc_deinit(void);
void* Encoder_Interface_init(int dtx);
void Encoder_Interface_exit(void* state);
int Encoder_Interface_Encode(void* state, enum amr_mode mode, const short* speech, unsigned char* out, int forceSpeech);


int loadlib_amrdec_init(void);
int loadlib_amrdec_deinit(void);
void* Decoder_Interface_init(void);
void Decoder_Interface_exit(void* state);
void Decoder_Interface_Decode(void* state, const unsigned char* in, short* out, int bfi);



int huiwei_audiofilterlib_init(void);



#endif  /*_loadlib_h*/

