
USE_CPP := 0
LOCAL_SRC =  .
LOCAL_SRC_CFILES := \
	$(LOCAL_SRC)/parser_inifile.c	\
	$(LOCAL_SRC)/ring_queue.c  \
	$(LOCAL_SRC)/sync_time.c  \
	$(LOCAL_SRC)/monitor_dev.c  \
	$(LOCAL_SRC)/tencent_init.c  \
	$(LOCAL_SRC)/msg_handle.c  \
	$(LOCAL_SRC)/isp_conf_file.c  \
	$(LOCAL_SRC)/dev_camera.c  \
	$(LOCAL_SRC)/video_stream.c  \
	$(LOCAL_SRC)/muxer_media.c  \
	$(LOCAL_SRC)/pcm_dev.c  \
	$(LOCAL_SRC)/amr_decode.c  \
	$(LOCAL_SRC)/amr_encode.c  \
	$(LOCAL_SRC)/voice_handle.c  \
	$(LOCAL_SRC)/talk_handle.c  \
	$(LOCAL_SRC)/fs_managed.c  \
	$(LOCAL_SRC)/system_up.c  \
	$(LOCAL_SRC)/main.c	              

LOCAL_SRC_CXXFILES 	:=





LOCAL_MODULE := huiwei_ipc

LOCAL_CFLAGS := 

PREFIX := arm-none-linux-gnueabi-

#链接时指定的libs所在目录
LOCAL_SHARED_LIBRARIES :=  akuio akmedialib akaudiocodec akaudiofilter akstreamenclib txdevicesdk  rt

#编译时指定的要使用的外部头文件目录
LOCAL_C_INCLUDES :=  ./ \
					../include \
#链接时指定要链接的lib.
LOCAL_LDFLAGS := -L../lib



LOCAL_LDFLAGS += ./libpcm.a	
LOCAL_LDFLAGS += ./libispsdk.a	

		


		
include exec_rules.mk
