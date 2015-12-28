#ifndef _video_stream_h
#define _video_stream_h



int video_stream_up(void);
int video_capture_start(void);
int  video_capture_stop(void);
int video_encode_reSetRc(int bit_rate);

#endif /*_video_stream_h*/

