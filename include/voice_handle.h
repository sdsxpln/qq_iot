#ifndef _voice_handle_h
#define _voice_handle_h




int voice_handle_center_up(void);
int voice_send_net_start(void);
int voice_send_net_stop(void);
int voice_record_voice_start(void);
int voice_record_voice_stop(void);
int voice_fill_net_data(unsigned char *enc_data, int data_len);


#endif  /*_voice_handle_h*/