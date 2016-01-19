#ifndef _demuxer_media_h
#define _demuxer_media_h

int demuxer_media_handle_up(void);
void  demuxer_media_fetch_history(unsigned int last_time, int max_count, int *count, void * range );
int  demuxer_media_add_history(char* file_name, unsigned long start_time, unsigned long end_time);
int demuxer_push_replay_info(unsigned int play_time, unsigned long long base_time);
int  demuxer_set_cur_start_time(unsigned long start_time);
int  demuxer_set_cur_end_time(unsigned long end_time);
int  demuxer_set_cur_file_name(char * file_name);
int  demuxer_init_cur_file(void);
int demuxer_replay_send_stop(void);

#endif  /*_demuxer_media_h*/