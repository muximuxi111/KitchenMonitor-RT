#ifndef __TASK_STATE_H
#define __TASK_STATE_H

#include "main.h"
#include "cmsis_os.h"

/* 需求里的 5 个事件位 */
#define EVT_TEMP    0x01u   /* bit0 温度越限 */
#define EVT_HUMI    0x02u   /* bit1 湿度越限 */
#define EVT_FIRE    0x04u   /* bit2 明火     */
#define EVT_ELEC    0x08u   /* bit3 用电异常 */
#define EVT_MUTED   0x10u   /* bit4 已消音   */

/*
	危险报警 > 环境预警 > 正常 
	ALARM	有 EVT_FIRE（明火）或 EVT_ELEC（用电异常）
	WARN	有 EVT_TEMP 或 EVT_HUMI（温湿度超限），但没有 ALARM 条件
	NORMAL	全都没有
*/
/* 三态枚举 */
typedef enum {
    SYS_NORMAL = 0,
    SYS_WARN   = 1,
    SYS_ALARM  = 2,
} sys_state_t;

extern sys_state_t sys_state;   /* 当前系统状态，供 LVGL / 串口读 */

//报警使能标志位
extern uint8_t alarm_enable;   /* bit0=温度 bit1=湿度 bit2=明火 bit3=用电，1=使能 */

/* 这个句柄是 CubeMX 在 freertos.c 里生成的，这里只做外部声明 */
extern osEventFlagsId_t myEvent01Handle;

void Evt_UpdateFromSensors(void);   /* 采集→拨开关，Sensor_send 里调 */


#endif
