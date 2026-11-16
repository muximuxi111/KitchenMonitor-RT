#include "sensor.h"

volatile Sht20_Status Sht20_current_state = Sht20_free_state;
//温湿度数据容器
uint8_t recv_Tem_data[2] = {0};
uint8_t recv_Hum_data[2] = {0};
uint8_t recv_als_data[2] = {0};
float Tem_value = 0.0f;
float Hum_value = 0.0f;
float Als_value = 0.0f;
volatile uint8_t Sht20_DataReady = 0;
uint8_t Tem_command = 0xE3;
uint8_t Hum_command = 0xE5;
uint8_t als_reg=0x0C;

//使能环境光监测寄存器
void ALS_ENABLE(){
		uint8_t buf[2]={0};
		buf[0]=0x00;
		buf[1]=0x01;
		if(HAL_I2C_Master_Transmit(&hi2c1,0x3C,buf,2, HAL_MAX_DELAY)!=HAL_OK){
				Uart_Printf("ALS Enable Failed!\r\n");
        return;
		}
		HAL_Delay(150);
}

//发送从机地址加环境光采样地址
void ALS_Tramsmit(){
		Sht20_current_state = Als_send_Complete;
		if(HAL_I2C_Master_Transmit_IT(&hi2c1,0x3C, &als_reg, 1) != HAL_OK)
    {
        Sht20_current_state = Als_send_state;
    }
}

//发送从机地址加温度寄存器地址
void Tem_Transmit()
{
    Sht20_current_state = Tem_send_Complete;
    if(HAL_I2C_Master_Transmit_IT(&hi2c1,0x80,&Tem_command,1) != HAL_OK)
    {
        Sht20_current_state = Sht20_free_state;
    }
}

//发送从机地址加湿度寄存器地址
void Hum_Transmit()
{
    Sht20_current_state = Hum_send_Complete;
    if(HAL_I2C_Master_Transmit_IT(&hi2c1,0x80,&Hum_command,1) != HAL_OK)
    {
        Sht20_current_state = Hum_send_state;
    }
}

//发送回调
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if(hi2c == &hi2c1)
    {
        if(Sht20_current_state == Tem_send_Complete)
        {
            Sht20_current_state = Tem_recv_data_state;
        }
				else if(Sht20_current_state == Als_send_Complete){
						Sht20_current_state = Als_recv_data_state;
				}
        else if(Sht20_current_state == Hum_send_Complete)
        {
            Sht20_current_state = Hum_recv_data_state;
        }
    }
}

//要接收环境光的数据
void Als_Recv()
{
    Sht20_current_state = Als_recv_Complete;
    if(HAL_I2C_Master_Receive_IT(&hi2c1,0x3C,recv_als_data,2) != HAL_OK)
    {
        Sht20_current_state = Als_recv_data_state;
    }
}

//要接收温度的数据
void Tem_Recv()
{
    Sht20_current_state = Tem_recv_Complete;
    if(HAL_I2C_Master_Receive_IT(&hi2c1,0x80,recv_Tem_data,2) != HAL_OK)
    {
        Sht20_current_state = Tem_recv_data_state;
    }
}

//要接收湿度的数据
void Hum_Recv()
{
    Sht20_current_state = Hum_recv_Complete;
    if(HAL_I2C_Master_Receive_IT(&hi2c1,0x80,recv_Hum_data,2) != HAL_OK)
    {
        Sht20_current_state = Hum_recv_data_state;
    }
}

//接收回调
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if(hi2c == &hi2c1)
    {
        if(Sht20_current_state == Tem_recv_Complete)
        {
            Sht20_current_state = Hum_send_state;
        }
        else if(Sht20_current_state == Hum_recv_Complete)
        {
            Sht20_current_state = Als_send_state;
        }
				else if(Sht20_current_state == Als_recv_Complete){
						Sht20_current_state = Sht20_handle_data_state;
				}
    }
}

//处理环境光数据
void handle_Als_data()
{
    //高位先行
    uint16_t data=(recv_als_data[1]<<8)|recv_als_data[0];
		Als_value = data * 0.35f; //65535 * 0.35 = 22937
}

//处理温度数据
void handle_Tem_data()
{
    //只需要14数据
    recv_Tem_data[1] &= ~0x03;
    //高位先行
    uint16_t data = ((uint16_t)recv_Tem_data[0] << 8) | recv_Tem_data[1];
    Tem_value = -46.85f + 175.72f * ((float)data / 65536.0f);
}

//处理湿度数据
void handle_Hum_data()
{
    //只需要14数据
    recv_Hum_data[1] &= ~0x03;
    uint16_t data = ((uint16_t)recv_Hum_data[0] << 8) | recv_Hum_data[1];
    Hum_value = -6.0f + 125.0f * ((float)data / 65536.0f);
}


//采集温湿度数据任务
void Sht20Task()
{
    switch(Sht20_current_state)
    {
        case Sht20_free_state:
            Tem_Transmit();
            break;
        case Tem_send_Complete:
            break;
        case Tem_recv_data_state:
            Tem_Recv();
            break;
        case Tem_recv_Complete:
            break;
        case Hum_send_state:
            Hum_Transmit();
            break;
        case Hum_send_Complete:
            break;
        case Hum_recv_data_state:
            Hum_Recv();
            break;
        case Hum_recv_Complete:
            break;
				case Als_send_state:
            ALS_Tramsmit();
            break;
        case Als_send_Complete:
            break;
        case Als_recv_data_state:
             Als_Recv();
            break;
        case Als_recv_Complete:
            break;
        case Sht20_handle_data_state:
						handle_Als_data();
            handle_Tem_data();
            handle_Hum_data();
            Sht20_DataReady = 1;
            Sht20_current_state = Sht20_free_state;
            break;
    }
}
