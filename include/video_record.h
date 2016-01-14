#ifndef _video_record_h
#define _video_record_h


#define	 RECORD_MAGINC	"record"
#define  RECORD_PATH	"/mnt/record"

#define	RECORD_FREE_SIZE	(600)   /*Mb*/
#define IFRAME_COUNTS_MAX	(3600)
#define	INDEX_FILE_OFFSET	(0)  
#define DATA_FILE_OFFSET	(72000)


static void  record_fetch_history(unsigned int last_time, int max_count, int *count, void * range ){}

static int record_push_replay_video_data(unsigned int play_time, unsigned long long base_time){}
static int record_replay_send_stop(void){}
//int record_push_record_data(void * data );
static int record_handle_center_up(void){}
static int record_reinit_handle(void){}
static int record_manage_files(void){}

#endif /*_video_record_h*/