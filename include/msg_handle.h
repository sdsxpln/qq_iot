#ifndef  _msg_handle_h
#define _msg_handle_h


typedef enum cmd_type
{

	LOGIN_COMPLETE_CMD,
	ONLINE_STATUS_CMD,


}cmd_type_t;



typedef struct msg_header
{
	cmd_type_t cmd;


}msg_header_t;


typedef struct login_complete
{
	int error_code;
}login_complete_t;


typedef struct online_status
{
	int old_status;
	int new_status;
	
}online_status_t;



#define  DEV_ONLINE			(11)
#define  DEV_OFFLINE		(21)


int  msg_push(void * data );
int msg_handle_start_up(void);

#endif   /*_msg_handle_h*/