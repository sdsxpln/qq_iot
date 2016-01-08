
#ifndef _fs_managed_h
#define _fs_managed_h


typedef enum
{
	FILE_NEW = 100,
	FILE_DEL,
}fs_action_m;

typedef enum
{
	FILE_RECORD,
	FILE_JPEG,
	FILE_MP4,
}file_type_m;



typedef struct fs_header
{
	file_type_m type;
	fs_action_m cmd;
}fs_header_t;


typedef struct fs_node
{
	char * file_name;
	
}fs_node_t;







int fs_handle_managed_up(void);
int fs_hangle_file(	file_type_m type,fs_action_m cmd,char * file_path);


#endif