#include <stdio.h>
#include <string.h>
#include <unistd.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  
#include <linux/netlink.h>
#include <sys/mount.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/types.h>
#include <linux/rtnetlink.h>
#include <sys/statfs.h>
#include "monitor_dev.h"
#include "msg_handle.h"
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


int mmc_process(int flag)
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

int mmc_get_free(char * path)/*MB*/
{
	int ret = -1;
	if(0 == mmc_status || NULL == path)return(0);
	struct statfs mmc_fs;
	memset(&mmc_fs,0,sizeof(mmc_fs));
	ret = statfs(path,&mmc_fs);
	if(0 != ret)
	{
		dbg_printf("statfs is fail ! \n");
		return(-1);
	}
	
	return((mmc_fs.f_bavail*mmc_fs.f_bsize)/(1024*1024));
}



static int eth_init_socket(void)
{

	int eth_fd = -1;
	int ret = -1;
	struct sockaddr_nl addr = {0};
	eth_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if(eth_fd < 0 )
	{
		dbg_printf("socket is fail !\n");
		return(-1);
	}
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK;
    addr.nl_pid = getpid();
	ret = bind(eth_fd, (struct sockaddr *) &addr, sizeof(addr));
	if(ret < 0)
	{
		close(eth_fd);
		eth_fd = -1;
	}

	return(eth_fd);
}


static int eth_link_status(void)
{
	int status = -1;
	int read_counts = 0;
	char buff[2] = {0}; 
	FILE * file = fopen("/sys/class/net/eth0/carrier","r");
	if(NULL == file)
	{
		dbg_printf("open the file fail ! \n");
		return(-1);
	}
	read_counts = fread(buff,1,2,file);
	if(read_counts <= 0)
	{
		fclose(file);
		file = NULL;
		return(-1);
	}
	status = atoi(buff);
	fclose(file);
	file = NULL;

	return(status);
	

}

static void * monitor_fun(void * arg)
{

	int ret = -1;
	int i = 0;
	
	int epfd = -1;
	int mmc_fd = -1;
	int eth_fd = -1;

	int count_bytes = -1;
	char buf[2048] = {0}; 
	
	mmc_fd = mmc_init_socket();
	if(mmc_fd < 0 )
	{
		dbg_printf("mmc_init_socket is fail ! \n");
	}

	eth_fd = eth_init_socket();
	if(eth_fd < 0)
	{
		dbg_printf("eth_init_socket is fail ! \n");
	}


	struct epoll_event ev;
	struct epoll_event events[16];
	epfd = epoll_create(16);
    ev.events = EPOLLIN;

	if(mmc_fd > 0)
	{
	    ev.data.fd = mmc_fd;
	    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, mmc_fd, &ev);
	}

	if(eth_fd > 0 )
	{
	    ev.data.fd = eth_fd;
	    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, eth_fd, &ev);	
	
	}
	

	while(1)
	{
		ret = epoll_wait(epfd, events, 8, -1);	
		if(ret <= 0)continue;
		for(i=0;i<ret;++i)
		{
			memset(buf,'\0',sizeof(buf));
			if(mmc_fd == events[i].data.fd)
			{
				int flag = -1;
				recv(mmc_fd, buf, sizeof(buf), 0); 
				if(NULL != strstr(buf,"remove") && NULL != strstr(buf,"mmc_host")) {flag = 0;}
				else if( NULL != strstr(buf,"add") && NULL != strstr(buf,"mmc_host")) { flag = 1; }

				if(-1 == flag)continue;
			  
				msg_header_t * msg = calloc(1,sizeof(msg_header_t)+sizeof(monitor_mmc_t)+1);
				if(NULL == msg)continue;
				monitor_mmc_t * mmc_dev = (monitor_mmc_t*)(msg+1);
				msg->cmd = MMC_MSG_CMD;
				mmc_dev->status = flag;
				ret = msg_push(msg);
				if(0 != ret)
				{
					free(msg);
					msg = NULL;
				}
				//dbg_printf("%s\n",buf);
				  
			}
			else if(eth_fd == events[i].data.fd)
			{
				count_bytes = recv(eth_fd,buf, sizeof(buf), 0);
				struct nlmsghdr *p = (struct nlmsghdr *) buf;
				for (; count_bytes > 0; p = NLMSG_NEXT(p, count_bytes))
				{
		            if (!NLMSG_OK(p, count_bytes) || (size_t) count_bytes < sizeof(struct nlmsghdr) || (size_t) count_bytes < p->nlmsg_len)
					{
						dbg_printf("the packet is wrong ! \n");
						break;
		            }
					if(p->nlmsg_type == RTM_NEWLINK || RTM_DELLINK == p->nlmsg_type)
					{
						int status = -1;
						status = eth_link_status();
						dbg_printf("status====%d\n",status);

					}

		
		        }


			}

				
		}
		
	}
	
	return(NULL);
	
}




  
 int monitor_start_up(void)
 {

 	umount(MMC_PATH);
	mmc_process(1);
	
	
	pthread_t monitor_pthid;
	pthread_create(&monitor_pthid,NULL,monitor_fun,NULL);
	pthread_detach(monitor_pthid);
	
	return(0);
 }
