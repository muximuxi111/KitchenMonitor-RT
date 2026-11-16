/***************************温湿度与光照采集***************************/
#ifndef __SENSOR_H
#define __SENSOR_H
#include "main.h"
#include "stdio.h"
#include "i2c.h"
#include "usart.h"      /* Uart_Printf */

/* 数据结构 —— 必须放这里，别的文件才用得上（队列里存的就是它） */
typedef struct {
    float temperature;
    float humidity;
    float light;
    float voltage;
    float current;
} Sensor_Data_t;

typedef enum
{
    Sht20_free_state,
    Tem_send_Complete,
    Tem_recv_data_state,
    Tem_recv_Complete,
    Hum_send_state,
    Hum_send_Complete,
    Hum_recv_data_state,
    Hum_recv_Complete,
		Als_send_state,
		Als_send_Complete,
		Als_recv_data_state,
		Als_recv_Complete,
    Sht20_handle_data_state
} Sht20_Status;

extern float Tem_value;
extern float Hum_value;
extern float Als_value;
extern volatile uint8_t Sht20_DataReady;
void ALS_ENABLE();
void Tem_Transmit();
void Hum_Transmit();
void ALS_Tramsmit();
void Tem_Recv();
void Hum_Recv();
void Als_Recv();
void handle_Tem_data();
void handle_Hum_data();
void handle_Als_data();
void Sht20Task(void);
#endif