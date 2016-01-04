#ifndef _video_record_h
#define _video_record_h


#define	 RECORD_MAGINC	"record"
#define  RECORD_PATH	"/mnt/record"

#define IFRAME_COUNTS_MAX	(3600)
#define	INDEX_FILE_OFFSET	(0)  
#define DATA_FILE_OFFSET	(72000)


void  record_fetch_history(unsigned int last_time, int max_count, int *count, void * range );
int record_push_replay_data(unsigned int play_time, unsigned long long base_time);
int record_replay_send_stop(void);
int record_push_record_data(void * data );
int record_start_up(void);
int record_reinit_handle(void);

#endif /*_video_record_h*/