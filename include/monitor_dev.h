#ifndef _monitor_dev_h
#define _monitor_dev_h



#define  MMC_PATH			"/mnt"
#define  MMC_DEVICE			"/dev/mmcblk0"
#define  MMC_DEVICE_P1		"/dev/mmcblk0p1"




int mmc_get_free(char * path);
int mmc_process(int flag);
int monitor_start_up(void);
int mmc_get_status(void);

#endif  /*_monitor_dev_h*/

