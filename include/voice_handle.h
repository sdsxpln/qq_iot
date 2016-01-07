#ifndef _voice_handle_h
#define _voice_handle_h




int voice_system_start_up(void);
int voice_send_start(void);
int voice_send_stop(void);
int voice_record_start(void);
int voice_record_stop(void);
int voice_net_send(unsigned char *enc_data, int data_len);


#endif  /*_voice_handle_h*/