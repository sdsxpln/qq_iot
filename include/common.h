#ifndef  _COMMON_H
#define  _COMMON_H


#undef  DBG_ON
#undef  FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"common:"
#define 	dbg_printf(fmt,arg...) \
	do{if(DBG_ON)fprintf(stderr,FILE_NAME"%s(line=%d)->"fmt,__FUNCTION__,__LINE__,##arg);}while(0)



#define err_abort(code,text) do { \
	fprintf (stderr, "%s at \"%s\":%d: %s\n", \
	text, __FILE__, __LINE__, strerror (code)); \
	abort (); \
	} while (0)

	
#define errno_abort(text) do { \
	fprintf (stderr, "%s at \"%s\":%d: %s\n", \
	text, __FILE__, __LINE__, strerror (errno)); \
	abort (); \
	} while (0)
	


#endif