#ifndef _tencent_init_h
#define _tencent_init_h


int tencent_free_dev_info(void * dev_info);
void * tencent_get_dev_info(void);
void * tencent_get_notify_info(void);
int  tencent_free_notify_info(void * notify);
void * tencent_get_ipcamera_notify(void);
void tencent_free_ipcamera_notify(void * arg);
void  tencent_free_path_config(void * arg);
void * tencent_get_path_config(void);
void tencent_config_log(int level, const char* module, int line, const char* message);
void * tencent_get_av(void);
void  tencent_free_av(void * arg);

#endif  /*_tencent_init_h*/