#ifndef _video_stream_h
#define _video_stream_h



int video_stream_up(void);
int video_encode_reSetRc(int bit_rate);

int video_send_video_start(void);
int video_send_video_stop(void);
int video_record_video_start(void);
int video_record_video_stop(void);

#endif /*_video_stream_h*/

