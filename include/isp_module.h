/** @file
* @brief Define the ISP server api
*
* Copyright (C) 2015 Anyka (GuangZhou) Software Technology Co., Ltd.
* @author
* @date 2015-07-24
* @version 1.0
*/

#ifndef __ISP_MODULE_H__
#define __ISP_MODULE_H__

#include "anyka_types.h"
#include "ak_isp_char.h"

#define ISP_MODULE_MAX_SIZE		(1024*12)


typedef enum {
	ISP_BB = 0,   			//黑平衡
	ISP_LSC,				//镜头校正
	ISP_RAW_LUT,			//raw gamma
	ISP_NR,					//NR
	ISP_3DNR,				//3DNR

	ISP_GB,					//绿平衡
	ISP_DEMO,				//DEMOSAIC
	ISP_GAMMA,				//GAMMA
	ISP_CCM,				//颜色校正
	ISP_FCS,				//FCS

	ISP_WDR,				//WDR
	ISP_EDGE,				//EDGE
	ISP_SHARP,				//SHARP
	ISP_SATURATION,			//饱和度
	ISP_CONSTRAST,			//对比度

	ISP_RGB2YUV,			//rgb to yuv
	ISP_YUVEFFECT,			//YUV效果
	ISP_DPC,				//坏点校正
	ISP_WEIGHT,				//权重系数
	ISP_AF,					//AF

	ISP_WB,					//WB	
	ISP_EXP,				//Expsoure
	ISP_MISC,				//杂项

	ISP_3DSTAT,				//3D降噪统计
	ISP_AESTAT,				//AE统计
	ISP_AFSTAT,				//AF统计
	ISP_AWBSTAT,			//AWB统计

	ISP_SENSOR,				//sensor参数

    ISP_MODULE_ID_NUM
} T_MODULE_ID;

typedef enum {
	MODE_NIGHTTIME,
	MODE_DAYTIME,
} T_MODULE_MODE;

T_S32 Isp_Dev_Open(void);

T_S32 Isp_Dev_Close(void);

T_S32 Isp_Module_Init(T_U8* cfgBuf, T_U32 size, T_U32 phyaddr, T_U32 width, T_U32 height);

T_S32 Isp_Module_Get(T_U16 module_id, T_U8* buf, T_U32* size);

T_S32 Isp_Module_Set(T_U16 module_id, T_U8* buf, T_U32 size);

T_S32 Isp_Module_StatInfo_Get(T_U16 module_id, T_U8* buf, T_U32* size);

T_S32 Isp_Sensor_Get(T_U16 addr, T_U16 *value);

T_S32 Isp_Sensor_Set(T_U16 addr, T_U16 value);

T_S32 Isp_Sensor_Load_Conf(T_U16 *sensor_regs);

T_S32 Isp_Module_Set_User_Params(AK_ISP_USER_PARAM *param);

T_S32 Isp_Module_Switch_Mode(T_MODULE_MODE mode);

#endif

