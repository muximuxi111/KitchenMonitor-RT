#include "adc_monitor.h"
//因为 DMA 是异步搬运数据的。如果把数组定义在函数内部，函数结束后局部数组的生命周期就结束了
uint16_t adc_dma_buf[2] = {0};
float test_A = 0;
float test_V = 0;

// 启动 ADC + DMA
void A_V_DMA_Start()
{
    HAL_StatusTypeDef ret;

    ret = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, 2);

    if(ret == HAL_OK)
    {
					Uart_Printf("ADC DMA Start Success!\r\n");
    }
    else
    {
        Uart_Printf("ADC DMA Start Failed! ret=%d\r\n", ret);
    }
}

// 获取并处理 ADC 数据
void A_V_datacollect()
{

	test_A = adc_dma_buf[0] * 3.3f / 4095.0f / 10.1f;
	test_V = adc_dma_buf[1] * 3.3f / 4095.0f;


}