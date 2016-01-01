#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h>  
#include <linux/netlink.h>
#include <sys/mount.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "monitor_dev.h"
#include "common.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"monitor_dev:"


static volatile unsigned int mmc_status = 0;


static int mmc_init_socket(void)  
{  
    struct sockaddr_nl snl;  
    const int buffersize = 2048;  
	int recv_timeout = 2000;
    int retval;  
    memset(&snl, 0x00, sizeof(struct sockaddr_nl));  
    snl.nl_family = AF_NETLINK;  
    snl.nl_pid = getpid();  
    snl.nl_groups = 1;  
  
    int hotplug_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);  
    if (hotplug_sock == -1) 
	{  
        return -1;  
    }  
    setsockopt(hotplug_sock, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize));  
	setsockopt(hotplug_sock,SOL_SOCKET,SO_RCVTIMEO,(char *)&recv_timeout,sizeof(int));
    retval = bind(hotplug_sock, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl));  
    if (retval < 0) 
	{  
        close(hotplug_sock);  
        hotplug_sock = -1;  
        return -1;  
    }  
	
    return hotplug_sock;  
	
}


static int mmc_process(int flag)
{
	int ret = -1;	



	if(flag)
	{
		if((mmc_status==0) && 0 == access(MMC_DEVICE_P1,F_OK))
		{
			ret = mount(MMC_DEVICE_P1, MMC_PATH, "vfat", MS_MGC_VAL | MS_SYNCHRONOUS, NULL);
			if(ret==0)compare_and_swap(&mmc_status,0,1);
			dbg_printf("mount1 ret == %d \n",ret);
		}
		else if((mmc_status==0) && 0 == access(MMC_DEVICE,F_OK))
		{
			ret = mount(MMC_DEVICE, MMC_PATH, "vfat", MS_MGC_VAL | MS_SYNCHRONOUS, NULL);
			if(ret == 0)compare_and_swap(&mmc_status,0,1);
			dbg_printf("mount2 ret == %d \n",ret);
		}
	}
	else
	{
		if((mmc_status==1) && 0 != access(MMC_DEVICE_P1,F_OK) && 0 != access(MMC_DEVICE,F_OK))
		{
			ret = umount(MMC_PATH);
			if(ret == 0)compare_and_swap(&mmc_status,1,0);
			dbg_printf("umount ret == %d \n",ret);
		}

	}

	return(ret);

}


int mmc_get_status(void)
{
	return(mmc_status);
}


static void * monitor_fun(void * arg)
{

	int ret = -1;
	int i = 0;
	int epfd = -1;
	int mmc_fd = -1;
	
	umount(MMC_PATH);
	mmc_process(1);
	struct epoll_event ev;
	struct epoll_event events[16];
	epfd = epoll_create(16);
	mmc_fd = mmc_init_socket();
	if(mmc_fd < 0 )
	{
		dbg_printf("mmc_init_socket is fail ! \n");
		return(NULL);
	}
    ev.events = EPOLLIN;
    ev.data.fd = mmc_fd;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, mmc_fd, &ev);
	if(ret < 0 )
    {
        dbg_printf("epoll_ctl is fail ! \n");
        return (NULL);
    }

	while(1)
	{
		ret = epoll_wait(epfd, events, 8, -1);	
		if(ret <= 0)continue;
		for(i=0;i<ret;++i)
		{
			if(mmc_fd == events[i].data.fd)
			{
				  char buf[2048] = {0}; 
				  recv(mmc_fd, buf, sizeof(buf), 0); 
				  if(NULL != strstr(buf,"remove") && NULL != strstr(buf,"mmc_host"))
				  {	
				  	
					 mmc_process(0);
				  }
				  else if( NULL != strstr(buf,"add") && NULL != strstr(buf,"mmc_host"))
				  {
					 mmc_process(1);
				  }
				  
				  dbg_printf("%s\n",buf);
				  memset(buf,'\0',sizeof(buf));	
			}
		}
		
	}
	
	return(NULL);
	
}



  

 
 int monitor_start_up(void)
 {
	
	pthread_t monitor_pthid;
	pthread_create(&monitor_pthid,NULL,monitor_fun,NULL);
	pthread_detach(monitor_pthid);
	
	return(0);
 }
