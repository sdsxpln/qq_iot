#ifndef _video_record_h
#define _video_record_h


#define	 RECORD_MAGINC	"record"
#define  RECORD_PATH	"/mnt/record"

#define IFRAME_COUNTS_MAX	(3600)
#define	INDEX_FILE_OFFSET	(0)  
#define DATA_FILE_OFFSET	(72000)

int record_push_record_data(void * data );
void  fetch_history_video(unsigned int last_time, int max_count, int *count, void * range);
int record_start_up(void);

#endif /*_video_record_h*/