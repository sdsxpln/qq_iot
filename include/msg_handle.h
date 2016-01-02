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



#define  VIDEO_DATA_MAX_SIZE	 (30720)/*(30*1024)*/
typedef struct video_data
{
	unsigned int status;
	unsigned int nFrameType;
	unsigned int nEncDataLen;
	unsigned int nTimeStamps;
	unsigned int nGopIndex;
	unsigned int nFrameIndex;
	unsigned int nTotalIndex;
	unsigned char data[VIDEO_DATA_MAX_SIZE];
	
}video_data_t;



typedef struct  video_iframe_index
{
	char magic[10];
	unsigned long time_stamp;
	unsigned long offset;
	

}video_iframe_index_t;


typedef struct  video_node_header
{
	unsigned int total_size;
	unsigned int check_flag;  /*nTotalIndex+nFrameIndex+nGopIndex+nFrameType*/
}video_node_header_t;



typedef struct video_replay_info
{
	char * file_name;
	unsigned int play_time;
	unsigned long long base_time;
}video_replay_info_t;







#define  DEV_ONLINE			(11)
#define  DEV_OFFLINE		(21)




int  msg_push(void * data );
int msg_handle_start_up(void);

#endif   /*_msg_handle_h*/