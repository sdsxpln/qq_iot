#ifndef _dev_camera_h
#define	_dev_camera_h



#define VIDEO_WIDTH_720P     (1280u)
#define VIDEO_HEIGHT_720P	 (720u)
#define VIDEO_WIDTH_VGA      (640u)
#define VIDEO_HEIGHT_VGA	 (480u)
#define VIDEO_WIDTH_320P     (320u)
#define VIDEO_HEIGHT_240P	 (240u)

#define  CAMERA_DEVICE		"/dev/video0"
#define	 BUFFER_NUM			(4)


typedef struct  
{
    void * start;
    size_t length;
}buffer_t;


typedef struct camera_dev
{
	int camera_fd;
	int video_height;
	int video_width;
	void * pmem_addres;
	void *ion_isp_mem;
	buffer_t data[BUFFER_NUM]; 
	
}camera_dev_t;



int camera_stop (void * dev);
int camera_start(void * dev);
void * camera_read_frame(void * dev);
int camera_free_frame(void * dev,void * frame_buff);
void * camera_new_dev(const char * dev_path);
void  camera_free_dev(void * dev);

#endif/*_dev_camera_h*/