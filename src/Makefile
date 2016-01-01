

CC = arm-none-linux-gnueabi-gcc


#CC = gcc



LOCAL_INC =  -I./  -I../include  -L../lib

LOCAL_STATIC_LIB = ./libispsdk.a
LOCAL_SHARE_LIB =    -lpthread  -lakuio  -lakstreamenclib  -ltxdevicesdk -lrt  
LOCAL_SRC =  .





LOCAL_DEV_SRC := \
		$(LOCAL_SRC)/parser_inifile.c	\
		$(LOCAL_SRC)/ring_queue.c  \
		$(LOCAL_SRC)/sync_time.c  \
		$(LOCAL_SRC)/monitor_dev.c  \
		$(LOCAL_SRC)/tencent_init.c  \
		$(LOCAL_SRC)/msg_handle.c  \
		$(LOCAL_SRC)/isp_conf_file.c  \
		$(LOCAL_SRC)/dev_camera.c  \
		$(LOCAL_SRC)/video_stream.c  \
		$(LOCAL_SRC)/video_record.c  \
		$(LOCAL_SRC)/system_up.c  \
		$(LOCAL_SRC)/main.c	


 






all:

	${CC}  ${LOCAL_INC}   ${LOCAL_DEV_SRC}  ${LOCAL_STATIC_LIB}    -o   test_bin   ${LOCAL_SHARE_LIB}

clean:
	rm -rf  test_bin

