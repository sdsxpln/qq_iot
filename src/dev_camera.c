
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "akuio.h"
#include "isp_conf_file.h"
#include "isp_module.h"
#include "dev_camera.h"
#include "common.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"dev_camera:"


#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define ENOUGH_MEM(min_type, max_type) assert(sizeof(min_type) <= sizeof(max_type))

#define RGB2Y(R,G,B)   ((306*R + 601*G + 117*B)>>10)
#define RGB2U(R,G,B)   ((-173*R - 339*G + 512*B + 131072)>>10)
#define RGB2V(R,G,B)   ((512*R - 428*G - 84*B + 131072)>>10)





static int xioctl (int camera_fd,int request,void * arg)
{
    int r = 0;
	if(camera_fd <=0 )
	{
		dbg_printf("please int the camera dev ! \n");
		return(-1);
	}
    do 
	{
		r = ioctl(camera_fd, request, arg);
	}while (-1 == r && EINTR == errno);
    return r;
}



static int camera_set_channel(int width, int height)
{

	int ret;
	AK_ISP_USER_PARAM param;
	struct isp_channel2_info *p_chn;
	CLEAR(param);
	ENOUGH_MEM(struct isp_channel2_info, param.data);
	param.id = AK_ISP_USER_CID_SET_SUB_CHANNEL;
	p_chn = (struct isp_channel2_info *)param.data;
	p_chn->width = width;
	p_chn->height = height;
	ret = Isp_Module_Set_User_Params(&param);

	return ret;

}


static int camera_set_occ_color(int color_type, int transparency, int y_component, int u_component, int v_component)
{
	int ret;
	AK_ISP_USER_PARAM param;
	struct isp_occlusion_color *p_occ_color;

	CLEAR(param);
	ENOUGH_MEM(struct isp_occlusion_color, param.data);

	param.id = AK_ISP_USER_CID_SET_OCCLUSION_COLOR;

	p_occ_color = (struct isp_occlusion_color *) param.data;
	p_occ_color->color_type = color_type;
	p_occ_color->transparency = transparency;
	p_occ_color->y_component = y_component;
	p_occ_color->u_component = u_component;
	p_occ_color->v_component = v_component;
	ret = Isp_Module_Set_User_Params(&param);

	return ret;
}


static int isp_init_param(void * dev)
{

	unsigned char *cfgbuf = NULL;
	unsigned long cfgsize = 0;
	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || handle->camera_fd <=0 )
	{
		dbg_printf("please int the camera dev ! \n");
		return(-1);
	}
	
	cfgbuf = (unsigned char*)calloc(1,ISP_CFG_MAX_SIZE);
	if(NULL == cfgbuf)
	{
		dbg_printf("calloc is fail ! \n");
		return(-1);
	}
	ISP_Conf_FileLoad(cfgbuf, &cfgsize);
    handle->ion_isp_mem = akuio_alloc_pmem(handle->video_width * handle->video_height*3/2*2);//扩大一倍是3D降噪需要
    
	int ret = Isp_Module_Init(cfgbuf, cfgsize, akuio_vaddr2paddr(handle->ion_isp_mem), handle->video_width, handle->video_height);
	if(0 != ret)
	{
		dbg_printf("Isp_Module_Init is fail ! \n");

	}
	if (NULL != cfgbuf)
	{
		free(cfgbuf);
		cfgbuf = NULL;
	}
	return ret;

}


static int isp_open(void)
{
	return Isp_Dev_Open();
}



static int isp_close(void *dev )
{
	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || handle->camera_fd <=0 )
	{
		dbg_printf("please int the camera dev ! \n");
		return(-1);
	}

	if(NULL != handle->ion_isp_mem)
	{
		free(handle->ion_isp_mem);
		handle->ion_isp_mem = NULL;
	}

	return Isp_Dev_Close();
}


int camera_stop (void * dev)
{
	int ret = -1;

	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || handle->camera_fd <=0 )
	{
		dbg_printf("please int the camera dev ! \n");
		return(-1);
	}
	
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = xioctl(handle->camera_fd, VIDIOC_STREAMOFF, &type);
	if(0 != ret)
	{
		dbg_printf("stop camera stream fail ! \n");
		return(-1);
	}
	return(0);
    
}



int camera_start(void * dev)
{
	int ret = -1;

	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || handle->camera_fd <=0 )
	{
		dbg_printf("please int the camera dev ! \n");
		return(-1);
	}
	
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = xioctl(handle->camera_fd, VIDIOC_STREAMON, &type);
	if(0 != ret)
	{
		dbg_printf("start camera stream fail ! \n");
		return(-1);
	}
	return(0);
    
}


static int camera_mmap(void * dev,unsigned int buff_szie)
{

	int ret = -1;
	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || handle->camera_fd <= 0 || 0 == buff_szie)
	{
		dbg_printf("please check the param ! \n");
		return(-1);
	}
	struct v4l2_requestbuffers req;
	memset(&req,0,sizeof(req));
    req.count  = BUFFER_NUM;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;
	ret = xioctl(handle->camera_fd, VIDIOC_REQBUFS, &req);
	if(0 != ret )
	{
		dbg_printf("fail !!!\n");
		return(-2);
	}

	if(NULL != handle->pmem_addres)
	{
		dbg_printf("pmem_addres has been init ! \n");
		return(-1);

	}

	handle->pmem_addres = akuio_alloc_pmem(BUFFER_NUM*buff_szie);
	if(NULL == handle->pmem_addres)
	{
		dbg_printf("akuio_alloc_pmem fail ! \n");
		return(-3);
	}

	unsigned int addres = akuio_vaddr2paddr(handle->pmem_addres) & 7;
	unsigned char * pmem_phyaddres = ((unsigned char *)handle->pmem_addres) + ((8-addres)&7);
	int i = 0;
    for (i = 0; i < BUFFER_NUM; ++i)
	{
		
		handle->data[i].length = buff_szie;
		handle->data[i].start = pmem_phyaddres + (handle->data[i].length) *i;
		if(NULL == handle->data[i].start)
		{
			dbg_printf("the addres is wrong ! \n");
			return(-4);
		}

		struct v4l2_buffer buf;
		memset(&buf,0,sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = i;
        buf.m.userptr = (unsigned long)handle->data[i].start;
        buf.length = handle->data[i].length;
		ret =  xioctl(handle->camera_fd, VIDIOC_QBUF, &buf);
		if(ret != 0 )
		{
			dbg_printf("VIDIOC_QBUF  fail ! \n");
			akuio_free_pmem(handle->pmem_addres);
			return(-5);
		}
		
    }


	return(0);
}



int camera_unmmap(void * dev)
{

	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || NULL == handle->pmem_addres)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	akuio_free_pmem(handle->pmem_addres);
	handle->pmem_addres = NULL;
	return(0);
}



void * camera_read_frame(void * dev)
{

	int ret = -1;
	int i = -1;
	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL == handle || handle->camera_fd <=0 )
	{
		dbg_printf("please init the camera dev ! \n");
		return(NULL);
	}

    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(handle->camera_fd, &fds);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    ret = select(handle->camera_fd + 1, &fds, NULL, NULL, &tv);	
	if(ret <= 0)
	{
		dbg_printf("the camera is not ready ! \n");
		return(NULL);
	}
	struct v4l2_buffer *frame_buf = calloc(1,sizeof(*frame_buf));
	if(NULL == frame_buf)
	{
		dbg_printf("calloc fail ! \n");
		return(NULL);
	}
	frame_buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	frame_buf->memory = V4L2_MEMORY_USERPTR;
	frame_buf->m.userptr = 0;

	ret = xioctl(handle->camera_fd, VIDIOC_DQBUF, frame_buf);
	if(0 != ret )
	{
		free(frame_buf);
		frame_buf = NULL;
		dbg_printf("the camera data is not ready ! \n");
		return(NULL);
	}
	for (i = 0; i < BUFFER_NUM; ++i) 
	{
	   if (frame_buf->m.userptr == (unsigned long)handle->data[i].start)
	   {
			return(frame_buf);
	   }
	}

	if(NULL != frame_buf)
	{
		free(frame_buf);
		frame_buf = NULL;
	}

	return(NULL);

}




int camera_free_frame(void * dev,void * frame_buff)
{
	int ret = -1;

	camera_dev_t * handle = (camera_dev_t*)dev;
	if(NULL==handle || NULL == frame_buff || handle->camera_fd <= 0)
	{
		dbg_printf("check the param ! \n");
		return(-1);
	}
	ret = xioctl(handle->camera_fd, VIDIOC_QBUF, frame_buff);
	free(frame_buff);
	frame_buff = NULL;
	if(ret != 0 )
	{
		dbg_printf("fail ! \n");
		return(-2);
	}
	return(0);
}

void * camera_new_dev(const char * dev_path)
{

	int ret = -1;
	int min = -1;
	if(NULL == dev_path)
	{
		dbg_printf("please check the param ! \n");
		return(NULL);
	}
	
	ret = isp_open();
	if(0 != ret)
	{
		dbg_printf("isp_open is fail ! \n");
	}
	

	struct stat st;
	ret = stat(dev_path,&st);
	if(0 != ret || !S_ISCHR (st.st_mode))
	{
		dbg_printf("please check the video device !\n");
		return(NULL);
	}

	camera_dev_t * new_camera = calloc(1,sizeof(*new_camera));
	if(NULL == new_camera)
	{
		dbg_printf("calloc fail ! \n");
		return(NULL);
	}

	new_camera->camera_fd = open (dev_path, O_RDWR| O_NONBLOCK,0);
	if(new_camera->camera_fd < 0 )
	{
		dbg_printf("open the camera dev fail ! \n");
		goto fail;
	}

	fcntl(new_camera->camera_fd, F_SETFD, FD_CLOEXEC);

	new_camera->video_height = VIDEO_HEIGHT_720P;
	new_camera->video_width = VIDEO_WIDTH_720P;
	new_camera->pmem_addres = NULL;
	
	ret = isp_init_param(new_camera);
	if(0 != ret)
	{
		dbg_printf("isp_init_param is fail ! \n");
	}
	


	struct v4l2_capability cap;
	memset(&cap,0,sizeof(cap));
	ret = xioctl(new_camera->camera_fd, VIDIOC_QUERYCAP,&cap);
	if(0 != ret)
	{
		dbg_printf("get the v4l2_capability fail ! \n");
		goto fail;
	}

	dbg_printf("max_frame===%d \n",cap.reserved[1]);
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) || !(cap.capabilities & V4L2_CAP_STREAMING))
	{
		dbg_printf("check the device driver ! \n");
		goto fail;
	}

	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	memset(&cropcap,0,sizeof(cropcap));
	memset(&crop,0,sizeof(crop));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = xioctl (new_camera->camera_fd, VIDIOC_CROPCAP, &cropcap);
	if(0 == ret )
	{
		dbg_printf("left==%d top==%d  width==%d  height==%d \n",cropcap.defrect.left,cropcap.defrect.top,cropcap.defrect.width,cropcap.defrect.height);
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
		xioctl(new_camera->camera_fd, VIDIOC_S_CROP, &crop);
	}

	struct v4l2_format fmt;
	memset(&fmt,0,sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = new_camera->video_width;  
    fmt.fmt.pix.height = new_camera->video_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	ret  = xioctl(new_camera->camera_fd, VIDIOC_S_FMT, &fmt);
	if(0 != ret)
	{
		dbg_printf("VIDIOC_S_FMT fail ! \n");
		goto fail;
	}


#if 0
	camera_set_channel(new_camera->camera_fd,VIDEO_WIDTH_VGA,VIDEO_HEIGHT_VGA,1);
#endif

	ret  = xioctl(new_camera->camera_fd, VIDIOC_G_FMT, &fmt);
	if(0 != ret)
	{
		dbg_printf("VIDIOC_G_FMT fail ! \n");
		goto fail;
	}


	
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
			fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
			fmt.fmt.pix.sizeimage = min;

#if 0
	fmt.fmt.pix.sizeimage += (new_camera->video_width*new_camera->video_height*3/2);
#endif

	camera_mmap(new_camera,fmt.fmt.pix.sizeimage);



	if (camera_set_occ_color(0, 0, RGB2Y(0, 0, 0), RGB2U(0, 0, 0), RGB2V(0, 0, 0)))
		dbg_printf("set occlusion color err\n");
	else
		dbg_printf("set occlusion color succeedded!\n");

	return(new_camera);

fail:

	if(new_camera->camera_fd > 0)
	{
		close(new_camera->camera_fd);
		new_camera->camera_fd = -1;

	}
	if(NULL != new_camera)
	{
		free(new_camera);
		new_camera = NULL;
	}
	return(NULL);
	
}

	

void  camera_free_dev(void * dev)
{
	if(NULL == dev)return;
	isp_close(dev);
	camera_unmmap(dev);
	free(dev);
	dev = NULL;
}


