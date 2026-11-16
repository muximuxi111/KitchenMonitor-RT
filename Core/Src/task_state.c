#include "task_state.h"
#include "usart.h"          /* Uart_Printf */
#include "system_state.h"   /* temp_alarm_th / humi_alarm_th 等阈值 */
#include "sensor.h"         /* Tem_value / Hum_value */
#include "adc_monitor.h"    /* test_V / test_A */

/* ==========================================================================
 * 本模块负责的两个全局变量（原来写在 freertos.c 里，现在搬到这里）
 * ========================================================================== */

//初始化三态枚举
sys_state_t sys_state = SYS_NORMAL;

// 报警标志位默认全使能 
uint8_t alarm_enable = 0x0F;


//配置事件
void Evt_UpdateFromSensors(void)
{
    if (myEvent01Handle == NULL) return;

	/* bit0 温度：超了置位，退回来清位，报警标志位0位为1才触发事件*/
    if ((alarm_enable & 0x01) && Tem_value >= temp_alarm_th)
				osEventFlagsSet(myEvent01Handle, EVT_TEMP);
		else
				osEventFlagsClear(myEvent01Handle, EVT_TEMP);
		
   /* bit0 湿度：超了置位，退回来清位，报警标志位1位为1才触发事件*/
    if ((alarm_enable & 0x02) && Hum_value >= humi_alarm_th)
			osEventFlagsSet(myEvent01Handle, EVT_HUMI);
		else
			osEventFlagsClear(myEvent01Handle, EVT_HUMI);

    /* bit2 明火：PB3 低电平有效。在fire_alarm(void)设置标志位*/ 
		if ((alarm_enable & 0x04) && (fire_warning == 1))
			osEventFlagsSet(myEvent01Handle, EVT_FIRE);
		else
			osEventFlagsClear(myEvent01Handle, EVT_FIRE );
		
    /* bit3 用电异常，报警标志位3位为1才触发事件*/
    if ((alarm_enable & 0x08) && ((A_V_state() == 1)||( A_V_state() == 2)))	//A_V_state()为1表示电压电流过高，为2表示过低
				osEventFlagsSet(myEvent01Handle, EVT_ELEC);
		else
				osEventFlagsClear(myEvent01Handle, EVT_ELEC);
		
		/* bit4 蜂鸣器静音，在Mute_Key3里实现事件置位*/
}


