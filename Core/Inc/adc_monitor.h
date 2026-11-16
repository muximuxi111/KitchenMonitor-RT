#ifndef __ADC_MONITOR_H
#define __ADC_MONITOR_H
#include "main.h"
#include "adc.h"
#include "stdio.h"
#include "usart.h"      /* Uart_Printf */
extern float test_A;
extern float test_V;
void A_V_datacollect();
void A_V_DMA_Start();
#endif
