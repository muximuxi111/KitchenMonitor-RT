#include "system_state.h"
#include "task_state.h"     /* alarm_enable */
#include "adc_monitor.h"    /* test_V / test_A */
#include "usart.h"          /* Uart_Printf */


/* 温湿度报警阈值，上电取默认值，之后由设置页滑块改 */
float temp_alarm_th = TEMP_ALARM_DEFAULT;
float humi_alarm_th = HUMI_ALARM_DEFAULT;


//火焰报警标志位
uint8_t fire_warning = 0;

//电压电流异常标志位
uint8_t VA_abnormal_flag = 0;

//火焰报警器
void fire_alarm(void)
{
    static uint8_t last = 0;
    /* PB3 低电平 = 有火焰。模块上 LM393 是开集输出经 R62 10k 上拉到 3V3，
       没有火焰时输出高电平，检测到红外时拉低（同时 LD6 点亮）。 */
    uint8_t fire = (HAL_GPIO_ReadPin(FIRE_GPIO_Port, FIRE_Pin) == GPIO_PIN_SET);

    if (fire != last)
    {
        /* 时间戳由 Uart_Printf 统一加（前缀 [%8lu]），这里不再手写，
           否则会打出两个 tick，看日志时容易对不上 */
        Uart_Printf("FIRE %s\r\n",fire ? "detected" : "cleared");

        if (fire) {
					if (alarm_enable & 0x04){          /* 只有报警标志位第4位为1才置位 */
							fire_warning = 1;
						}
				}
				else {
					fire_warning = 0;
			}
        last = fire;
    }
}

//判断电压电流是过高还是过低
int A_V_state(){
	if(((test_V >=VOLTAGE_ALARMH) || (test_A >=CURRENT_ALARMH))){
			VA_abnormal_flag = 1;
	}
	else if(((test_V <=VOLTAGE_ALARML) || (test_A <=CURRENT_ALARML))){
			VA_abnormal_flag = 2;
	}
	else{
			VA_abnormal_flag = 0;
	}
	return VA_abnormal_flag;
} 

