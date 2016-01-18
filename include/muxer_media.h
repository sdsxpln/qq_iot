#ifndef _muxer_media_h
#define _muxer_media_h



#define  RECORD_PATH	"/mnt/record"
#define	 RECORD_TMP_FLAG	".tmp"
#define	 RECORD_MP4_FLAG	".mp4"

int muxer_media_handle_up(void);
int mux_push_record_data(void * data );




#endif