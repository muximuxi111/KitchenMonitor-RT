/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "bsp_ft6336.h"
#include "bsp_ili9341_4line.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include "stdio.h"

#include "ui.h"
#include "stdarg.h"
#include "string.h"
#include "task_state.h"
#include "sensor.h"
#include "key.h"
#include "alarm_log.h"
#include "alarm.h"
#include "wifi.h"
#include "ui_bind.h"
#include "wwdg.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* 全局变量已经按功能拆到各自的模块里，这里不再放：
 *    sys_state / alarm_enable  ->  task_state.c（声明在 task_state.h）
 *    USART3_RxBuff             ->  usart.c（声明在 usart.h）
 *    Uart_Printf()             ->  usart.c（声明在 usart.h） */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* printf / scanf 的重定向（fputc / fgetc）也在 usart.c 里，
   因为它们是"把 stdio 接到哪个串口"的事，属于串口模块。 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

#define WDOG_FEED_MS      30u   /* 喂狗周期，落在 24.2~49.9ms 窗口中间 */


/* USER CODE END Variables */
/* Definitions for Sensor_sendTask */
osThreadId_t Sensor_sendTaskHandle;
const osThreadAttr_t Sensor_sendTask_attributes = {
  .name = "Sensor_sendTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LCD_TASK */
osThreadId_t LCD_TASKHandle;
const osThreadAttr_t LCD_TASK_attributes = {
  .name = "LCD_TASK",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Sensor_recvTask */
osThreadId_t Sensor_recvTaskHandle;
const osThreadAttr_t Sensor_recvTask_attributes = {
  .name = "Sensor_recvTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for set_eventTask */
osThreadId_t set_eventTaskHandle;
const osThreadAttr_t set_eventTask_attributes = {
  .name = "set_eventTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for wait_eventTask */
osThreadId_t wait_eventTaskHandle;
const osThreadAttr_t wait_eventTask_attributes = {
  .name = "wait_eventTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for WifiTask */
osThreadId_t WifiTaskHandle;
const osThreadAttr_t WifiTask_attributes = {
  .name = "WifiTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for WWDG_Task */
osThreadId_t WWDG_TaskHandle;
const osThreadAttr_t WWDG_Task_attributes = {
  .name = "WWDG_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for Sensor_queue */
osMessageQueueId_t Sensor_queueHandle;
const osMessageQueueAttr_t Sensor_queue_attributes = {
  .name = "Sensor_queue"
};
/* Definitions for WifiTimer */
osTimerId_t WifiTimerHandle;
const osTimerAttr_t WifiTimer_attributes = {
  .name = "WifiTimer"
};
/* Definitions for KeyTimer */
osTimerId_t KeyTimerHandle;
const osTimerAttr_t KeyTimer_attributes = {
  .name = "KeyTimer"
};
/* Definitions for printfMutex01 */
osMutexId_t printfMutex01Handle;
const osMutexAttr_t printfMutex01_attributes = {
  .name = "printfMutex01"
};
/* Definitions for printfBinarySem01 */
osSemaphoreId_t printfBinarySem01Handle;
const osSemaphoreAttr_t printfBinarySem01_attributes = {
  .name = "printfBinarySem01"
};
/* Definitions for myBinarySem02 */
osSemaphoreId_t myBinarySem02Handle;
const osSemaphoreAttr_t myBinarySem02_attributes = {
  .name = "myBinarySem02"
};
/* Definitions for myEvent01 */
osEventFlagsId_t myEvent01Handle;
const osEventFlagsAttr_t myEvent01_attributes = {
  .name = "myEvent01"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void Sensor_send(void *argument);
void LCD(void *argument);
void Sensor_recv(void *argument);
void set_event(void *argument);
void wait_event(void *argument);
void Wifi(void *argument);
void feed_dogTask(void *argument);
void Report_Callback(void *argument);
void KeyTimer_Callback(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationTickHook(void);

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
	 lv_tick_inc(1);
}

//解析云端指令（具体处理都在 wifi.c 里，这里只做转发）
void ParseCommand(char *line)
{
    if (line == NULL) return;
    Wifi_HandleCommand(line);
}
/* USER CODE END 3 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of printfMutex01 */
  printfMutex01Handle = osMutexNew(&printfMutex01_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of printfBinarySem01 */
  printfBinarySem01Handle = osSemaphoreNew(1, 1, &printfBinarySem01_attributes);

  /* creation of myBinarySem02 */
  myBinarySem02Handle = osSemaphoreNew(1, 0, &myBinarySem02_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */

  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of WifiTimer */
  WifiTimerHandle = osTimerNew(Report_Callback, osTimerPeriodic, NULL, &WifiTimer_attributes);

  /* creation of KeyTimer */
  KeyTimerHandle = osTimerNew(KeyTimer_Callback, osTimerPeriodic, NULL, &KeyTimer_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of Sensor_queue */
  Sensor_queueHandle = osMessageQueueNew (4, sizeof(Sensor_Data_t), &Sensor_queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */

  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Sensor_sendTask */
  Sensor_sendTaskHandle = osThreadNew(Sensor_send, NULL, &Sensor_sendTask_attributes);

  /* creation of LCD_TASK */
  LCD_TASKHandle = osThreadNew(LCD, NULL, &LCD_TASK_attributes);

  /* creation of Sensor_recvTask */
  Sensor_recvTaskHandle = osThreadNew(Sensor_recv, NULL, &Sensor_recvTask_attributes);

  /* creation of set_eventTask */
  set_eventTaskHandle = osThreadNew(set_event, NULL, &set_eventTask_attributes);

  /* creation of wait_eventTask */
  wait_eventTaskHandle = osThreadNew(wait_event, NULL, &wait_eventTask_attributes);

  /* creation of WifiTask */
  WifiTaskHandle = osThreadNew(Wifi, NULL, &WifiTask_attributes);

  /* creation of WWDG_Task */
  WWDG_TaskHandle = osThreadNew(feed_dogTask, NULL, &WWDG_Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of myEvent01 */
  myEvent01Handle = osEventFlagsNew(&myEvent01_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_Sensor_send */
/**
  * @brief  Function implementing the Sensor_sendTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_Sensor_send */
void Sensor_send(void *argument)
{
  /* USER CODE BEGIN Sensor_send */
	//先延时20ms防止als环境光采集寄存器使能失败
	osDelay(20);
	ALS_ENABLE();
	Sensor_Data_t Sensor_send_msg;
	osStatus_t status;
	uint32_t count;
	uint32_t space;
	// 启动 ADC + DMA，只需要启动一次
   A_V_DMA_Start();
  /* Infinite loop */
  for(;;)
  {
		Sht20Task();
		if(Sht20_DataReady == 1)
		{
			Sht20_DataReady = 0; 
			// 获取 ADC 数据并转换
			A_V_datacollect();
			Sensor_send_msg.temperature = Tem_value;
			Sensor_send_msg.humidity = Hum_value;
			Sensor_send_msg.light = Als_value;
			Sensor_send_msg.voltage = test_V;
			Sensor_send_msg.current = test_A;

			/*  首次释放会把阻塞在 osSemaphoreAcquire 的 set_event 唤醒，
			   之后它不再取，这个信号量就一直留 1。 */
			osSemaphoreRelease(myBinarySem02Handle);


			//使用0表示队列满了就立即返回，不等待。
			status = osMessageQueuePut(Sensor_queueHandle,&Sensor_send_msg,NULL,0);
			if(status == osOK)	//队列还有空间
			{
				//获取当前消息数量
        count = osMessageQueueGetCount(Sensor_queueHandle);
				//获取剩余空闲位置
        space = osMessageQueueGetSpace(Sensor_queueHandle);

					Uart_Printf("Queue Count:%lu Space:%lu\r\n",
								 (unsigned long)count,
								 (unsigned long)space);
			}
			else
			{
					Uart_Printf("Sensor Queue Send Failed!\r\n");
			}
    }
		osDelay(120);
  }
  /* USER CODE END Sensor_send */
}

/* USER CODE BEGIN Header_LCD */
/**
* @brief Function implementing the LCD_TASK thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LCD */
void LCD(void *argument)
{
  /* USER CODE BEGIN LCD */
  /* Infinite loop */
	ILI9341_Init();
	FT6336_init();
	
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();
	
	ui_init();
	Ui_Bind_Init();
	Wifi_Ui_Init();     /* WiFi 页的账号输入 + 主页网络图标 */
  for(;;)
  {
		Ui_Task();
    osDelay(5);
  }
  /* USER CODE END LCD */
}

/* USER CODE BEGIN Header_Sensor_recv */
/**
* @brief Function implementing the Sensor_recvTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Sensor_recv */
void Sensor_recv(void *argument)
{
  /* USER CODE BEGIN Sensor_recv */
	Sensor_Data_t Sensor_recv_msg;
	osStatus_t status;
  /* Infinite loop */
   for(;;)
    {
				//接收消息队列osWaitForever 表示：如果消息队列中没有消息，当前任务就一直阻塞等待，直到有消息到来
        status = osMessageQueueGet(Sensor_queueHandle,&Sensor_recv_msg,NULL,osWaitForever);

        if(status == osOK)
        {
            Uart_Printf("Tem: %.2f C | Hum: %.2f %% | Light: %.2f | Voltage: %.2f V | Current: %.2f A\r\n",
                   Sensor_recv_msg.temperature,
                   Sensor_recv_msg.humidity,
                   Sensor_recv_msg.light,
                   Sensor_recv_msg.voltage,
                   Sensor_recv_msg.current);
        }
        else
        {
            Uart_Printf("Queue Receive Failed! status=%d\r\n", status);
        }
    }
  /* USER CODE END Sensor_recv */
}

/* USER CODE BEGIN Header_set_event */
/**
* @brief Function implementing the set_eventTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_set_event */
void set_event(void *argument)
{
  /* USER CODE BEGIN set_event */
  uint32_t t0 = osKernelGetTickCount();
  osStatus_t ready;

  /* 阻塞等"首帧数据采完"这个事件，由 Sensor_send 采集完成后 osSemaphoreRelease 唤醒 */
  ready = osSemaphoreAcquire(myBinarySem02Handle, 5000U);

  if (ready == osOK)
    Uart_Printf("[EVT] sensor ready in %lu ms\r\n", (unsigned long)(osKernelGetTickCount() - t0));
  else
    Uart_Printf("[EVT] sensor NOT ready in 5s, run anyway\r\n");

  /* Infinite loop */
  for(;;)
  {
		//设置对应事件
		Evt_UpdateFromSensors();
		AlarmLog_Task();   /* 报警历史边沿检测（和判报警放同一个任务，取样时刻一致） */
    osDelay(10);
  }
  /* USER CODE END set_event */
}

/* USER CODE BEGIN Header_wait_event */
/**
* @brief Function implementing the wait_eventTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_wait_event */
void wait_event(void *argument)
{
  /* USER CODE BEGIN wait_event */
    uint32_t flags;
    sys_state_t state;
		uint16_t cnt = 0;        // 数数用，0~49 循环
		osTimerStart(KeyTimerHandle, 10);	//启动定时器按键扫描
    Uart_Printf("[TASK] EventTask started\r\n");
    /* Infinite loop */
    for(;;)
    {
        fire_alarm();    /* 每 10ms 轮询火焰，更新 EVT_FIRE */

        flags = osEventFlagsGet(myEvent01Handle);   /* 直接读状态 */

        /* ===== 事件组 → 三态裁决 ===== */
        if (flags & (EVT_FIRE | EVT_ELEC))		//火焰或者电压电流异常
            state = SYS_ALARM;
        else if (flags & (EVT_TEMP | EVT_HUMI))	//温湿度异常
            state = SYS_WARN;
        else
            state = SYS_NORMAL;		//正常状态

        /* ===== 按三态输出 ===== */
        switch (state)
        {
        case SYS_NORMAL:	//正常状态
            LED1_OFF(); 
						LED2_OFF(); 
						LED3_OFF();
						BEEP_OFF();
            break;

        case SYS_WARN:		/* WARN：LED1+风扇 */
            LED1_ON();  
            LED2_OFF(); 
						LED3_OFF(); 
						BEEP_OFF();
            break;

        case SYS_ALARM:		/* ALARM：LED2+LED3+蜂鸣器（除非静音） */
            LED2_ON();  
						LED3_ON();
            LED1_OFF(); 
            if (flags & EVT_MUTED)
						{
								BEEP_OFF();
								cnt = 0;                    // 静音时归零,防止干扰下次响的频率
						}
						else
						{
								cnt++;                      // 每轮 10ms 数一下
								if (cnt >= 50) cnt = 0;     // 数到 50 回到 0

								if (cnt < 20)  BEEP_ON();   // 前 20 次响
								else           BEEP_OFF();  // 后 30 次停
						}
						break;
        }

        sys_state = state;   /* 存全局，供 UI / 串口读 */
        FAN_Update();        /* 风扇统一在这里输出：手动跟滑块，自动跟状态 */
        osDelay(10);
    }
  /* USER CODE END wait_event */
}

/* USER CODE BEGIN Header_Wifi */
/**
* @brief Function implementing the WifiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Wifi */
void Wifi(void *argument)
{
  /* USER CODE BEGIN Wifi */
  /* 联网流程全部在 wifi.c 的 Wifi_Task() 里：
     状态机、每步超时+重试、2s 上报、+IPD 指令解析。
     这里只管调它，它自己会阻塞等串口数据，不占 CPU。 */
  for(;;)
  {
    Wifi_Task();
  }
  /* USER CODE END Wifi */
}

/* USER CODE BEGIN Header_feed_dogTask */
/**
* @brief Function implementing the WWDG_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_feed_dogTask */
void feed_dogTask(void *argument)
{
  /* USER CODE BEGIN feed_dogTask */
  /* 第一口必须等过窗口下沿 24.2ms，0~24.2ms 之内刷新会被判"喂太早"直接复位,WDOG_FEED_MS为30ms */
  osDelay(WDOG_FEED_MS);

  for(;;)
  {
    HAL_WWDG_Refresh(&hwwdg);
    osDelay(WDOG_FEED_MS);
  }
  /* USER CODE END feed_dogTask */
}

/* Report_Callback function */
void Report_Callback(void *argument)
{
  /* USER CODE BEGIN Report_Callback */
  /* 2s 软件定时器回调：只举一下手，真正的 AT+CIPSEND 由 WifiTask 去做。
     定时器回调跑在定时器任务里，不能在这里发串口 —— 会和状态机抢 UART3。 */
  (void)argument;
  Wifi_OnReportTimer();
  /* USER CODE END Report_Callback */
}

/* KeyTimer_Callback function */
void KeyTimer_Callback(void *argument)
{
  /* USER CODE BEGIN KeyTimer_Callback */
		Key_Scan(); //按键状态扫描
  /* USER CODE END KeyTimer_Callback */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* Uart_Printf() 已经搬到 usart.c（串口打印归串口模块），
   声明在 usart.h，这里直接用就行。 */

/* USER CODE END Application */

