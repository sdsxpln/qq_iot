

CC = arm-none-linux-gnueabi-gcc


#CC = gcc



LOCAL_INC =  -I./  -I../include  -L../lib

LOCAL_LIB =    -lpthread  -lakuio  -lakstreamenclib  -ltxdevicesdk -lrt
LOCAL_SRC =  .




LOCAL_DEV_SRC := \
		$(LOCAL_SRC)/parser_inifile.c	\
		$(LOCAL_SRC)/ring_queue.c  \
		$(LOCAL_SRC)/tencent_init.c  \
		$(LOCAL_SRC)/msg_handle.c  \
		$(LOCAL_SRC)/system_up.c  \
		$(LOCAL_SRC)/main.c	


 






all:

	${CC}  ${LOCAL_INC}  ${LOCAL_DEV_SRC}   -o   test_bin   ${LOCAL_LIB}

clean:
	rm -rf  test_bin

