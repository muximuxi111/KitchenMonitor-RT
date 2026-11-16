#ifndef __SYSTEM_STATE_H
#define __SYSTEM_STATE_H
#include "main.h"
#include "stdio.h"

//温湿度过高初始阈值（只作为上电默认值，实际用下面的变量）
#define TEMP_ALARM_DEFAULT     30.0f
#define HUMI_ALARM_DEFAULT     60.0f

/* 温湿度报警阈值：设置页的两个滑块写这两个变量，Evt_UpdateFromSensors 读它们 */
extern float temp_alarm_th;
extern float humi_alarm_th;

//阈值滑块的取值范围（设置页显示用）
#define TEMP_SLIDER_MIN        20      /* 温度滑块阈值℃ */
#define TEMP_SLIDER_MAX        60
#define HUMI_SLIDER_MIN        20      /* 湿度滑块阈值% */
#define HUMI_SLIDER_MAX        95

//电压电流过高阈值
#define VOLTAGE_ALARMH         2.8f
#define CURRENT_ALARMH         0.35f


//电压电流过低阈值
#define VOLTAGE_ALARML         0.3f
#define CURRENT_ALARML         0.03f

//火焰报警数码管显示数值
#define FIRE_DISPLAY_VALUE    99.99f


//火焰报警标志位
extern uint8_t fire_warning;

//电压电流异常标志位
extern uint8_t VA_abnormal_flag;


void fire_alarm();
int A_V_state();
#endif
