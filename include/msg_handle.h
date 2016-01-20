#ifndef  _msg_handle_h
#define _msg_handle_h


typedef enum cmd_type
{

	LOGIN_COMPLETE_CMD,
	ONLINE_STATUS_CMD,
	START_VIDEO_CMD,
	STOP_VIDEO_CMD,
	START_MIC_CMD,
	STOP_MIC_CMD,
	SET_BITRATE_CMD,
	VIDEO_DATA_CMD,
	VOICE_DATA_CMD,
	MMC_MSG_CMD,
	ETH_MSG_CMD,
	KEY_MSG_CMD,
	


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


typedef enum
{
	RECORD_VIDEO_DATA,
	RECORD_VOICE_DATA,
}record_data_type_t;


#define  FRAME_FPS_NUM		(30)
#define  FRAME_GAP_NUM		(60)
#define	 FRAME_BIT_RATE	 	(500)

#define  VIDEO_DATA_MAX_SIZE	 (51200)/*(50*1024)*/
typedef struct video_data
{
	record_data_type_t type;
	unsigned int status;
	unsigned int nFrameType;
	unsigned int nEncDataLen;
	unsigned int nTimeStamps;
	unsigned int nGopIndex;
	unsigned int nFrameIndex;
	unsigned int nTotalIndex;
	unsigned char data[VIDEO_DATA_MAX_SIZE];
	
}video_data_t;


#define  FRAME_NUM_PER_PACKET	(8)
#define	 ENCODE_PCM_SIZE	(FRAME_NUM_PER_PACKET*320)
#define	 ENCODE_AMR_SIZE	(FRAME_NUM_PER_PACKET*32)


typedef struct voice_data
{
	record_data_type_t type;
	unsigned int data_length;
	unsigned long time_sample;
	unsigned char data[ENCODE_AMR_SIZE];
	
}voice_data_t;





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


typedef struct voice_replay_info
{
	char * file_name;
	unsigned long offset;
	
}voice_replay_info_t;


#define	TALK_NET_DATA_SIZE	(256+1)

typedef struct talk_net_data
{
	int pack_num;
	int length;
	unsigned char data[TALK_NET_DATA_SIZE];

}talk_net_data_t;


#define	TALK_RAW_DATA_SIZE	(1024*3)
typedef struct talk_raw_data
{
	int length;
	unsigned char data[TALK_RAW_DATA_SIZE];
}talk_raw_data_t;



typedef struct monitor_mmc
{
	char status;
	
}monitor_mmc_t;

typedef struct monitor_eth
{
	char status;
}monitor_eth_t;




#define  DEV_ONLINE			(11)
#define  DEV_OFFLINE		(21)





int  msg_push(void * data );
int msg_handle_center_up(void);

#endif   /*_msg_handle_h*/