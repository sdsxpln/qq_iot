#ifndef _system_up_h
#define _system_up_h


typedef struct system_config
{
	void * dev_info;  
	void * dev_notify;
	void * dev_path; 	
	void * dev_av;	

	char  online_status;
	


}system_config_t;




extern system_config_t * system_config_info;

int system_up(void);



#endif   /*_system_up_h*/

