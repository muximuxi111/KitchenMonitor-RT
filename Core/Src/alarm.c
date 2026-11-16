#include "alarm.h"
#include "task_state.h"     /* sys_state / SYS_NORMAL / SYS_WARN / SYS_ALARM */

/* ==================== 风扇 ====================
 * 三种角色分清楚：
 *   fan_manual : 0=自动模式  1=手动模式（设置页开关决定）
 *   fan_speed  : 手动模式下滑块给的目标转速 0~100
 *   fan_duty   : 当前真正写进 CCR 的占空比，给主页显示用
 * ============================================ */
uint8_t fan_manual = 0;      /* 上电默认自动模式 */
uint8_t fan_speed  = 60;     /* 手动模式默认 60% */
uint8_t fan_duty   = 0;      /* 当前实际输出 */

/* 云端 FAN:n 强制风速：优先级最高，压过自动逻辑和设置页滑块 */
uint8_t cloud_fan_override = 0;
uint8_t cloud_fan_speed    = 0;

void FAN_SetSpeed(uint8_t percent)
{
    if (percent > 100) percent = 100;

    fan_duty = percent;
    /* 输出比较,ARR = 99，所以 CCR 直接等于百分比 */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, percent);
}

/* 风扇总调度：由 wait_event() 每 10ms 调一次
 *   云端强制   → 云端 FAN:n 说了算（最高优先级）
 *   手动模式   → 滑块说了算
 *   自动模式   → 报警逻辑说了算：WARN 60%、ALARM 100%、NORMAL 停 */
void FAN_Update(void)
{
    if (cloud_fan_override)     /* 云端 FAN:n 强制 */
    {
        FAN_SetSpeed(cloud_fan_speed);
        return;
    }

    if (fan_manual)							/*通过ui_Fan_Slider手动控制 */
    {
        FAN_SetSpeed(fan_speed);
        return;
    }

    switch (sys_state)
    {
    case SYS_ALARM:
        FAN_SetSpeed(100);      /* 报警：全速排烟 */
        break;

    case SYS_WARN:
        FAN_SetSpeed(60);       /* 预警：60% */
        break;

    case SYS_NORMAL:
    default:
        FAN_SetSpeed(0);        /* 正常：停 */
        break;
    }
}

void LED1_ON(){
	HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,GPIO_PIN_SET);
}
void LED1_OFF(){
	HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,GPIO_PIN_RESET);
}
void LED2_ON(){
	HAL_GPIO_WritePin(LED2_GPIO_Port,LED2_Pin,GPIO_PIN_SET);
}
void LED2_OFF(){
	HAL_GPIO_WritePin(LED2_GPIO_Port,LED2_Pin,GPIO_PIN_RESET);
}
void LED3_ON(){
	HAL_GPIO_WritePin(LED3_GPIO_Port,LED3_Pin,GPIO_PIN_SET);
}
void LED3_OFF(){
	HAL_GPIO_WritePin(LED3_GPIO_Port,LED3_Pin,GPIO_PIN_RESET);
}
void BEEP_ON(){
	HAL_GPIO_WritePin(BEEP_GPIO_Port,BEEP_Pin,GPIO_PIN_SET);
}
void BEEP_OFF(){
	HAL_GPIO_WritePin(BEEP_GPIO_Port,BEEP_Pin,GPIO_PIN_RESET);
}
