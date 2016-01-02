#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include "common.h"

#define NTP_VERSION 		(0xe3)
#define NTP_DEFAULT_PORT	"123"
#define UNIX_OFFSET 		(2208988800ULL)

static char * ntp_servers[ ] = { \
	"0.pool.ntp.org",
	"1.pool.ntp.org",
	"2.pool.ntp.org",
	"3.pool.ntp.org",
	NULL,
};

				 			 
struct ntpPacket {
	uint8_t flags;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint8_t referenceID[4];
	uint32_t ref_ts_sec;
	uint32_t ref_ts_frac;
	uint32_t origin_ts_sec;
	uint32_t origin_ts_frac;
	uint32_t recv_ts_sec;
	uint32_t recv_ts_frac;
	uint32_t trans_ts_sec;
	uint32_t trans_ts_frac;
} __attribute__((__packed__))ntpPacket_t;


#undef  DBG_ON
#undef  FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"sync_time:"

				
static void * sync_time(void * arg) 
{
	char *server; 
	char *port = NTP_DEFAULT_PORT;
	struct addrinfo hints, *res, *ap; 
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	struct ntpPacket packet;
	int server_sock; 
	int i = 0;
	int ret = -1;
	int is_ok = -1;
	unsigned long recv_secs;
	time_t total_secs;
	struct tm *now;
	
	is_ok = -1;
	for(i=0,server=ntp_servers[i]; NULL != server; i++)
	{
		memset(&packet, 0, sizeof(struct ntpPacket));
		packet.flags = NTP_VERSION;
		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_family = AF_UNSPEC;
		ret = getaddrinfo(server, port, &hints, &res);
		if (ret != 0)
		{
			dbg_printf("getaddrinfo  fail ! \n");
			continue;
		}
		for (ap = res; ap != NULL; ap = ap->ai_next)
		{
			server_sock = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
			if (server_sock < 0 )
				continue;
			struct timeval recv_tv_out;
			recv_tv_out.tv_sec  = 5;
			recv_tv_out.tv_usec = 0;
			ret = setsockopt(server_sock,SOL_SOCKET,SO_RCVTIMEO,&recv_tv_out, sizeof(recv_tv_out));
			if(0 == ret )break;
			else 
				continue;
		}
		
		if (ap == NULL)
		{
			dbg_printf("loop the next ! \n");
			continue;
		}

		ret = sendto(server_sock, &packet, sizeof(struct ntpPacket), 0, ap->ai_addr, addrlen);
		if (ret < 0)
		{
			dbg_printf("send fail ! \n");
			continue;
		}
		ret = recvfrom(server_sock, &packet, sizeof(struct ntpPacket), 0, ap->ai_addr, &addrlen);
		if (ret < 0 )
		{
			dbg_printf("recvfrom  fail ! \n");
			continue;
		}
		
		freeaddrinfo(res); 
		if(server_sock > 0 )
		{
			close(server_sock);
			server_sock = -1;
		}
		is_ok = 1;
		break;
		

	}

	if(1 == is_ok)
	{
		unsigned char set_time[128];
		memset(set_time,'\0',128);
		recv_secs = ntohl(packet.recv_ts_sec)- UNIX_OFFSET;
		total_secs = recv_secs;
		now = localtime(&total_secs);

		sprintf(set_time," date -s \"%02d-%02d-%02d  %02d:%02d:%02d\"",now->tm_year+1900, now->tm_mon+1, now->tm_mday,now->tm_hour+8, now->tm_min, now->tm_sec);
		system(set_time);
		system(" hwclock  -w ") ;
		sleep(1);
		
		dbg_printf("%02d-%02d-%02d  %02d:%02d:%02d\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday,now->tm_hour+8, now->tm_min, now->tm_sec);

	}
	else
	{
		dbg_printf("sync time is fail ! \n");
		return(NULL);

	}

	return(NULL);

}



void sync_time_up(void)
{
	
	pthread_t sync_time_pthid;
	pthread_create(&sync_time_pthid,NULL,sync_time,NULL);
	//pthread_detach(sync_time_pthid);
	pthread_join(sync_time_pthid,NULL);


}

