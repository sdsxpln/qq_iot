#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "isp_conf_file.h"


#define ISP_CONFIG_FILE_PATH	"/etc/jffs2/isp.conf"

static FILE *fd = NULL;


T_BOOL ISP_Conf_FileCreate(T_VOID)
{
	if (NULL != fd)
	{
		fclose(fd);
	}

	fd = fopen(ISP_CONFIG_FILE_PATH, "w+b");

	if (NULL == fd)
	{
		printf("ISP_Conf_FileCreate Failed!\n");
		return AK_FALSE;
	}

	return AK_TRUE;
}

T_VOID ISP_Conf_FileClose(T_VOID)
{
	if (NULL != fd)
	{
		fclose(fd);
		fd = NULL;
	}
}


T_BOOL ISP_Conf_FileRead(T_U8* buf, T_U32 size, T_U32 offset)
{
	if (NULL == buf)
	{
		printf("ISP_Conf_FileRead buf NULL!\n");
		return AK_FALSE;
	}

	
	if (NULL == fd)
	{
		fd = fopen (ISP_CONFIG_FILE_PATH, "r+b");
	}

	if (NULL != fd)
	{
		fseek(fd, offset, SEEK_SET);
		if (size == fread(buf, 1, size, fd))
		{
			return AK_TRUE;
		}
		else
		{
			printf("fd read failed!\n");
		}
	}
	else
	{
		printf("fd open failed!\n");
	}


	return AK_FALSE;
}

T_BOOL ISP_Conf_FileLoad(T_U8* buf, T_U32* size)
{
	T_BOOL ret = AK_FALSE;
	
	if (NULL == buf || NULL == size)
	{
		printf("ISP_Conf_FileLoad param NULL!\n");
		return ret;
	}
	
	if (NULL == fd)
	{
		fd = fopen (ISP_CONFIG_FILE_PATH, "r+b");
	}

	if (NULL != fd)
	{
		fseek(fd, 0, SEEK_END);

		*size = ftell(fd);

		ret = ISP_Conf_FileRead(buf, *size, 0);
	}
	else
	{
		printf("fd open failed!\n");
	}

	ISP_Conf_FileClose();

	return ret;
}


T_BOOL ISP_Conf_FileWrite(T_U8* buf, T_U32 size, T_U32 offset)
{
	if (NULL == buf)
	{
		printf("ISP_Conf_FileWrite buf NULL!\n");
		return AK_FALSE;
	}
	
	if (NULL == fd)
	{
		fd = fopen (ISP_CONFIG_FILE_PATH, "r+b");
	}
	
	if (NULL != fd)
	{
		fseek(fd, offset, SEEK_SET);
		if (size == fwrite(buf, 1, size, fd))
		{
			return AK_TRUE;
		}
		else
		{
			printf("fd write failed!\n");
		}
	}
	else
	{
		printf("fd open failed!\n");
	}

	return AK_FALSE;
}

T_BOOL ISP_Conf_FileStore(T_U8* buf, T_U32 size)
{
	T_BOOL ret = AK_FALSE;
	
	if (NULL == buf || 0 == size)
	{
		printf("ISP_Conf_FileStore param NULL!\n");
		return ret;
	}

	if (ISP_Conf_FileCreate())
	{
		ret = ISP_Conf_FileWrite(buf, size, 0);
	}
	else
	{
		printf("fd open failed!\n");
	}

	ISP_Conf_FileClose();


	return ret;
}




