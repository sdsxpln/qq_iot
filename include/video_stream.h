#ifndef _video_stream_h
#define _video_stream_h


typedef enum
{
	I_FRAME = 0x00,
	P_FRAME,
	UNKNOW_FRAME
}frame_type_m;


typedef enum
{	
	SEND_VIDEO = (0x01<<1),
	RECORD_VIDEO = (0x01 << 2),
	UNKNOW_MODE,
}video_mode_t;






int video_handle_stream_up(void);
int video_encode_reSetRc(int bit_rate);

int video_send_video_start(void);
int video_send_video_stop(void);
int video_record_video_start(void);
int video_record_video_stop(void);

#endif /*_video_stream_h*/

