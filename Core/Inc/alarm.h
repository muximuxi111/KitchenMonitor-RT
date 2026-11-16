#ifndef __ALARM_H
#define __ALARM_H
#include "main.h"
#include "gpio.h"
#include "tim.h"
void LED1_ON();
void LED1_OFF();
void LED2_ON();
void LED2_OFF();
void LED3_ON();
void LED3_OFF();
//控制蜂鸣器
void BEEP_ON();
void BEEP_OFF();
void FAN_SetSpeed(uint8_t percent);
void FAN_Update(void);          /* 风扇总调度：手动跟滑块，自动跟报警状态 */
extern uint8_t fan_speed;       /* 手动模式的目标转速 0~100 */
extern uint8_t fan_manual;      /* 0=自动 1=手动 */
extern uint8_t fan_duty;        /* 当前实际占空比，主页显示用 */
extern uint8_t cloud_fan_override;  /* 1 = 云端 FAN:n 强制风速 */
extern uint8_t cloud_fan_speed;     /* 云端强制风速目标 0~100 */

#endif
