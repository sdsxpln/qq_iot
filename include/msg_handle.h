#ifndef  _msg_handle_h
#define _msg_handle_h


typedef enum cmd_type
{

	LOGIN_COMPLETE_CMD,
	ONLINE_STATUS_CMD,
	START_VIDEO_CMD,
	STOP_VIDEO_CMD,
	SET_BITRATE_CMD,
	VIDEO_DATA_CMD,
	VOICE_DATA_CMD,
	


}cmd_type_t;



typedef struct msg_header
{
	cmd_type_t cmd;


}msg_header_t;


typedef struct login_complete
{
	int error_code;
}login_complete_t;


typedef struct online_status
{
	int old_status;
	int new_status;
	
}online_status_t;


typedef struct set_bitrate
{
	int bit_rate;

}set_bitrate_t;




typedef struct video_data
{
	int nFrameType;
	int nTimeStamps;
	int nGopIndex;
	int nFrameIndex;
	int nTotalIndex;
	int nEncDataLen;
	unsigned char data[40*1024];
	
}video_data_t;



#define  DEV_ONLINE			(11)
#define  DEV_OFFLINE		(21)


int  msg_push(void * data );
int msg_handle_start_up(void);

#endif   /*_msg_handle_h*/