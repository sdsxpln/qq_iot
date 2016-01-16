#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>

#include "common.h"
#include "system_up.h"
#include "sync_time.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"main"


static void sigprocess(int sig)
{

	int ii = 0;
	void *tracePtrs[16];
	int count = backtrace(tracePtrs, 16);
	char **funcNames = backtrace_symbols(tracePtrs, count);
	for(ii = 0; ii < count; ii++)
		printf("%s\n", funcNames[ii]);
	free(funcNames);
	fflush(stderr);
	fflush(stdout);


	if(sig == SIGINT || sig == SIGSEGV || sig == SIGTERM)
	{
		
	}

	exit(1);

}


#include "muxer_media.h"
int main(void)
{

#if 0
	dbg_printf("this is a test ! \n");
	signal(SIGSEGV, sigprocess);
	signal(SIGINT, sigprocess);
	signal(SIGTERM, sigprocess);
#endif
	sync_time_up();
	system_up();
	while(1)sleep(100);
	return(0);
}