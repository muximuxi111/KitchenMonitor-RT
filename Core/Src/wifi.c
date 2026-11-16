/* ==========================================================================
 * wifi.c —— ESP-12F 联网（顺序流程版）
 * --------------------------------------------------------------------------
 * 只用这几条 AT 指令：
 *      AT / AT+CWMODE=1 / AT+CWJAP / AT+CIPMUX=0 / AT+CIPSTART / AT+CIPSEND
 *
 * 整体就三层：
 *   1) Wifi_RxIsr()  串口中断只往环形缓冲塞字节（很快返回）
 *   2) rx_poll()     把环形缓冲搬进 s_buf，然后一条一条切出来：
 *                       "+IPD,n:xxx" → 当成云端指令执行
 *                       普通一行     → 判 OK / ERROR / FAIL / '>'
 *   3) Wifi_zTask()   先顺序跑完 5 步联网，再进在线循环上报 + 收指令
 *
 * 全程只用 osDelay，不用 HAL_Delay，所以 WiFi 怎么折腾都不影响
 * 本地采集、显示和报警。
 * ========================================================================== */

#include "wifi.h"
#include "sensor.h"
#include "adc_monitor.h"
#include "task_state.h"
#include "system_state.h"
#include "alarm.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* freertos.c 里由 CubeMX 生成的 2s 软件定时器 */
extern osTimerId_t WifiTimerHandle;

/* ============================ 对外变量 ============================ */

int connect_flag = 0;                  /* 1 = 已连上服务器（老代码还在用它） */
volatile uint8_t wifi_connected = 0;   /* 同一个意思，给界面用 */

/* ============================ 账号与服务器 ============================ */

static char     s_ssid[33] = WIFI_SSID_DEFAULT;
static char     s_pass[65] = WIFI_PASS_DEFAULT;
static char     s_ip[24]   = ServerIP;
static uint16_t s_port     = ServerPort;

/* ============================ 接收 ============================ */

/* 环形缓冲区：中断写、任务读，只在这一对之间共享 */
#define RING_SIZE   512
static uint8_t           s_ring[RING_SIZE];
static volatile uint16_t s_head = 0;			//头指针负责下一个数据写入的位置
static volatile uint16_t s_tail = 0;			//尾指针负责数据读取的位置
static osSemaphoreId_t   s_sem  = NULL;   /* 中断收到 '\n' 就释放，唤醒 WifiTask */

/* 拼装缓冲区：从环形缓冲搬过来，按行 / 按 +IPD 一条条消费 */
#define BUF_SIZE    512
static char     s_buf[BUF_SIZE];
static uint16_t s_len = 0;

/* 一行判定的结果 */
#define EV_NONE    0
#define EV_OK      1
#define EV_PROMPT  2
#define EV_ERR    (-1)
static volatile int s_ev = EV_NONE;

/* ============================ 事件与状态 ============================ */

static volatile uint8_t s_ap_lost     = 0;   /* WIFI DISCONNECT */
static volatile uint8_t s_link_closed = 0;   /* CLOSED */
static volatile uint8_t s_ap_back     = 0;   /* WIFI GOT IP */
static volatile uint8_t s_report_due  = 0;   /* 2s 定时器举手 */
static volatile uint8_t s_restart_req = 0;   /* 界面要求立刻重连 */

static wifi_state_t s_state   = WIFI_ST_BOOT;
static uint8_t      s_inited  = 0;
static uint32_t     s_last_ok = 0;           /* 上次上报成功的时刻 */

/* ============================ 底层收发 ============================ */

//向esp12发送数据
static void wifi_write(const char *data, uint16_t len)
{
    if (data == NULL || len == 0) return;	
    HAL_UART_Transmit(&huart3, (uint8_t *)data, len, 1000);
}

/* UART3 中断里调用：只入队，不做任何耗时的事 */
void Wifi_RxIsr(const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        uint16_t next = (uint16_t)((s_head + 1U) % RING_SIZE);

        if (next != s_tail)                 /* 满了就丢新字节 */
        {
            s_ring[s_head] = buf[i];
            s_head = next;
        }

        if (buf[i] == '\n' && s_sem != NULL)
        {
            (void)osSemaphoreRelease(s_sem);		//收到换行符释放信号量
        }
    }
}

/* ============================ 接收解析 ============================ */

/* 丢掉 s_buf拼装缓冲区 前面 n 个已处理掉的字节 */
static void buf_drop(uint16_t n)
{
    if (n >= s_len) s_len = 0;
    else
    {		//memmove(目标地址, 源地址, 移动的字节数);这里的作用是将地址s_buf + n后的数据向前移动n字节
        memmove(s_buf, s_buf + n, (size_t)(s_len - n));		
        s_len = (uint16_t)(s_len - n);
    }
    s_buf[s_len] = '\0';
}

/* 一行文本 → 事件 / 断线标志 */
//判断 ESP8266 返回了什么
static void judge(char *line)
{
    while (*line == '\r' || *line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    if (strstr(line, "ERROR") != NULL || strstr(line, "FAIL") != NULL) s_ev = EV_ERR;
    else if (strstr(line, "OK") != NULL)                               s_ev = EV_OK;
    else if (strchr(line, '>') != NULL)                                s_ev = EV_PROMPT;	//返回“>”它表示 ESP8266 已经准备好接收实际数据了。

    if (strncmp(line, "WIFI DISCONNECT", 15) == 0) s_ap_lost     = 1;		//WiFi 连接断开了
    if (strncmp(line, "WIFI GOT IP", 11) == 0)     s_ap_back     = 1;		//已经成功连接 WiFi，并且获取到了 IP 地址
    if (strncmp(line, "CLOSED", 6) == 0)           s_link_closed = 1;		//表示当前 TCP 连接已经关闭
}

/* 搬数据 + 把所有已经收全的一行 / 一条 +IPD 处理掉 */
//+IPD：ESP8266 接收到云端发送的数据，也就是服务器下发的指令，指令格式参考："+IPD,4:MUTE"
static void rx_poll(void)
{
    static char payload[100];		//暂存从云端接收到的实际指令
    char *p;
    long  n;
    int   hdr;

	/* 1) 环形缓冲区数据移到拼装缓冲区也就是普通缓冲区*/
    while (s_tail != s_head)
    {
        if (s_len < BUF_SIZE - 1) s_buf[s_len++] = (char)s_ring[s_tail];
        s_tail = (uint16_t)((s_tail + 1U) % RING_SIZE);	//尾指针向前移动读取下一位数据
    }
    s_buf[s_len] = '\0';

    /* 2) 一条一条消费 */
    for (;;)
    {
        /* ---- 解析服务器指令：+IPD,5:MUTE（后面不一定跟换行） ---- */
        if (s_len >= 5 && strncmp(s_buf, "+IPD,", 5) == 0)
        {
            p = strchr(s_buf, ':');		//字符串中查找指定字符，返回这个字符第一次出现的位置
            if (p == NULL) return;                      /* 头还没收全 */

            hdr = (int)(p - s_buf) + 1;                 /* 内容前有多少字节 */
            n   = atol(s_buf + 5);                      /* 内容多少字节 */

            if (n <= 0 || n >= (long)sizeof(payload)) { buf_drop(5); continue; }
            if ((long)s_len < hdr + n) return;          /* 内容还没齐，等下次 */

            memcpy(payload, s_buf + hdr, (size_t)n);	//memcpy(目标地址,源地址,复制长度)，这里只把云端的命令复制过来
            payload[n] = '\0';
            buf_drop((uint16_t)(hdr + n));	//删除已经处理的数据
						
						//打印并且处理指令
            Uart_Printf("[WIFI] 收到指令: %s\r\n", payload);
            Wifi_HandleCommand(payload);
            continue;
        }

        /* ---- 解析的是 ESP8266 返回的 AT 响应信息 ---- */
				//在在指定长度的内存区域中查找某个字节
        p = memchr(s_buf, '\n', s_len);	
        if (p != NULL)
        {
            *p = '\0';		//OK\r\n -> OK\r\0\n
						judge(s_buf);		//识别esp反馈的数据并设置对应的状态
            buf_drop((uint16_t)(p - s_buf) + 1);	//删除已经解析好的命令
            continue;
        }

        /* ---- AT+CIPSEND 的 '>' 提示符，后面不带\r\n ---- */
        if (s_len > 0 && s_buf[0] == '>') { s_ev = EV_PROMPT; buf_drop(1); continue; }

        /* ---- 剩下半行，等下次数据；垃圾太多就清掉防溢出 ---- */
        if (s_len >= BUF_SIZE - 2) s_len = 0;
        s_buf[s_len] = '\0';
        return;
    }
}

/* ============================ AT 收发 ============================ */

static void at_send(const char *cmd)
{
	s_ev = EV_NONE;		//发送下一条指令前先清除上一条指令的状态
    wifi_write(cmd, (uint16_t)strlen(cmd));
    wifi_write("\r\n", 2);
}

/* 等一个事件，want = EV_OK 或 EV_PROMPT。返回 1 成功 0 失败/超时。
   cmd 不为空就先把它发出去（发送期间收到的行也会被处理，指令不会丢）。 */
static int at_wait(const char *cmd, int want, uint32_t tmo)	//(要发送的 AT 指令,希望收到的事件,最大的等待时间)
{
    uint32_t t0;

    if (cmd != NULL) at_send(cmd);

    t0 = osKernelGetTickCount();

    while ((osKernelGetTickCount() - t0) < tmo)	//当前时间-开始时间
    {
        rx_poll();

        if (s_ev == want) { s_ev = EV_NONE; return 1; }		//收到期望事件后将状态清空
        if (s_ev == EV_ERR) { s_ev = EV_NONE; return 0; }
        s_ev = EV_NONE;                  /* 等 '>' 时先到的 OK，忽略继续等 */

        if (s_restart_req) return 0;
        osDelay(5);
    }
    return 0;
}

/* ============================ 上报一帧 ============================ */

static int report_once(void)
{
    char buf[96];
    char cmd[32];
    int  n;

    /* 一行 CSV：温度,湿度,电压,电流,光照,系统状态(0正常/1警告/2报警) */
    n = snprintf(buf, sizeof(buf),
                 "Tem=%.2f,Hum=%.2f,V=%.2f,A=%.3f,Light=%d,State=%d\r\n",
                 Tem_value, Hum_value, test_V, test_A,
                 (int)Als_value, (int)sys_state);

    if (n <= 0 || n >= (int)sizeof(buf)) return -1;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", n);

    if (!at_wait(cmd, EV_PROMPT, 1000)) return -1;    /* 等 '>' 提示符 */

    /* 数据本身已经带 \r\n，所以这里直接写，不能再走 at_send（它会多加一对） */
    s_ev = EV_NONE;
    wifi_write(buf, (uint16_t)n);

    if (!at_wait(NULL, EV_OK, 2000)) return -1;        /* 等 SEND OK */

    return 0;
}

/* ============================ 远程指令 ============================ */

int Wifi_HandleCommand(const char *cmd)
{
    char tmp[64];
    int  len;

    if (cmd == NULL) return -1;

    /* 抄一份，去掉首尾空白（服务器那边可能多补个空格或回车） */
    strncpy(tmp, cmd, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    len = (int)strlen(tmp);
    while (len > 0 && (tmp[len - 1] == ' ' || tmp[len - 1] == '\r' ||
                       tmp[len - 1] == '\n' || tmp[len - 1] == '\t'))
    {
        tmp[--len] = '\0';
    }
    if (len == 0) return -1;

    if (strcmp(tmp, "MUTE") == 0)                 /* 消音 */
    {
        osEventFlagsSet(myEvent01Handle, EVT_MUTED);
        s_report_due = 1;                         /* 按要求回报一次 */
        return 0;
    }
    if (strcmp(tmp, "UNMUTE") == 0)               /* 取消消音 */
    {
        osEventFlagsClear(myEvent01Handle, EVT_MUTED);
        s_report_due = 1;
        return 0;
    }
    if (strcmp(tmp, "STAT") == 0)                 /* 立刻回报一次 */
    {
        s_report_due = 1;
        return 0;
    }
    if (strcmp(tmp, "AUTO") == 0)                 /* 解除强制，恢复本地自动/手动 */
    {
        cloud_fan_override = 0;
        s_report_due = 1;
        return 0;
    }
    if (strncmp(tmp, "FAN:", 4) == 0)             /* FAN:80 强制风速 */
    {
        int n = atoi(tmp + 4);

        if (n < 0)   n = 0;
        if (n > 100) n = 100;

        cloud_fan_override = 1;                   /* 标志位置 1：风扇归云端控制 */
        cloud_fan_speed    = (uint8_t)n;
        s_report_due = 1;
        return 0;
    }

    return -1;
}

/* ============================ 对外小接口 ============================ */

//获取当前 WiFi 的工作状态
int Wifi_State(void)		
{
    return (int)s_state;
}

/* 界面要显示"当前用的是哪个热点"：只读，返回内部缓冲区的地址，
   界面只拿它去 lv_textarea_set_text()，不会改内容。 */
//获取wifi名称
const char *Wifi_GetSsid(void)
{
    return s_ssid;
}
//获取wifi密码
const char *Wifi_GetPass(void)
{
    return s_pass;
}
//现在到了数据上报的时间上报标志位置1
void Wifi_OnReportTimer(void)
{
    s_report_due = 1;
}
//向 WiFi 任务发送重新连接请求，重连接标志位置1
void Wifi_RequestReconnect(void)
{
    s_restart_req = 1;
}
//修改当前使用的 WiFi 名称和密码
void Wifi_SetCredentials(const char *ssid, const char *pass)
{
    if (ssid == NULL || pass == NULL) return;

    taskENTER_CRITICAL();
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_pass, pass, sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
    taskEXIT_CRITICAL();

    Uart_Printf("[WIFI] 新热点: %s\r\n", s_ssid);
}

/* ============================ 联网 ============================ */

static const char    *const s_step_name[5] = {"AT 测试", "设 Station", "连热点", "单连接", "连服务器"};
static const uint16_t       s_step_tmo[5]  = {1000, 2000, 15000, 2000, 8000};

/* 第 i 步要发的指令 */
static void net_cmd(int i, char *buf, int sz)
{
    switch (i)
    {
    case 0:  strcpy(buf, "AT");                    break;
    case 1:  strcpy(buf, "AT+CWMODE=1");           break;		//查询/设置 Wi-Fi 模式
    case 2:  snprintf(buf, sz, "AT+CWJAP=\"%s\",\"%s\"", s_ssid, s_pass);          break;		//连接热点
    case 3:  strcpy(buf, "AT+CIPMUX=0");           break;			//设置单连接模式
    default: snprintf(buf, sz, "AT+CIPSTART=\"TCP\",\"%s\",%d", s_ip, (int)s_port); break;	//连接TCP
    }
}

/* 顺序走完 5 步，每步失败重试 3 次。全成功返回 1 */
static int wifi_join(void)
{
    char cmd[128];
    int  i, try;

    for (i = 0; i < 5; i++)
    {
        /* 顺便把进度写进 s_state，界面想知道走到哪一步可以读 Wifi_State() */
        s_state = (wifi_state_t)(WIFI_ST_AT + i);

        for (try = 0; try <= WIFI_RETRY_MAX && !s_restart_req; try++)
        {
            net_cmd(i, cmd, sizeof(cmd));	//生成当前步骤的 AT 指令

            if (at_wait(cmd, EV_OK, s_step_tmo[i]))
            {
                Uart_Printf("[WIFI] %s 完成\r\n", s_step_name[i]);
                break;
            }
            Uart_Printf("[WIFI] %s 失败，第 %d 次\r\n", s_step_name[i], try + 1);
        }

        if (s_restart_req || try > WIFI_RETRY_MAX) return 0;
    }
    return 1;
}

/* 等 30s 再重连。点了"连接"或路由器恢复（WIFI GOT IP）就提前结束 */
static void wait_retry(void)
{
    uint32_t t0 = osKernelGetTickCount();
    uint8_t  skip = s_restart_req;

    s_state       = WIFI_ST_RETRY;
    s_restart_req = 0;
    if (skip) return;

    while ((osKernelGetTickCount() - t0) < WIFI_RECONNECT_MS)
    {
        (void)osSemaphoreAcquire(s_sem, 100);
        rx_poll();

        if (s_ap_back) { s_ap_back = 0; break; }
        if (s_restart_req) { s_restart_req = 0; break; }
    }
    s_ap_back = 0;
}

/* ============================ 初始化 ============================ */

static void wifi_init(void)
{
		//创建一个二值信号量
    s_sem = osSemaphoreNew(1, 0, NULL);

    /* 打开 USART3 的"空闲中断收包"，之后每次收完都会在回调里自动续上 */
    HAL_UARTEx_ReceiveToIdle_IT(&huart3, USART3_RxBuff, (uint16_t)sizeof(USART3_RxBuff));

    /* 2 秒周期上报（触发WifiTimer软件定时器*/
    if (WifiTimerHandle != NULL) osTimerStart(WifiTimerHandle, WIFI_REPORT_MS);

		s_inited = 1;		//表示wifi已经初始化
    s_state  = WIFI_ST_BOOT;

    Uart_Printf("[WIFI] 启动，热点=%s 服务器=%s:%d\r\n", s_ssid, s_ip, (int)s_port);

    osDelay(800);                       /* 等 ESP-12F 上电就绪 */
}

/* ============================ 任务主体 ============================ */

void Wifi_Task(void)
{
    if (!s_inited) wifi_init();

    for (;;)
    {
        /* ---------------- 1. 联网（5 步跑完才往下走） ---------------- */
        while (!wifi_join())
        {
            wifi_connected = 0;
            connect_flag   = 0;
            s_report_due   = 0;
            s_ap_lost = s_link_closed = s_ap_back = 0;

            Uart_Printf("[WIFI] 联网失败，%d 秒后重试\r\n", (int)(WIFI_RECONNECT_MS / 1000U));
            wait_retry();	//连接失败等待重连
        }

        /* ---------------- 2. 在线：上报 + 收指令 ---------------- */
        wifi_connected = 1;
        connect_flag   = 1;
        s_last_ok      = osKernelGetTickCount();
        s_report_due   = 1;                 /* 连上先发一帧，服务器马上能看到 */
        s_state        = WIFI_ST_ONLINE;
        s_ap_lost = s_link_closed = s_ap_back = 0;

        Uart_Printf("[WIFI] 已连上服务器 %s:%d\r\n", s_ip, (int)s_port);

        while (wifi_connected)
        {
            (void)osSemaphoreAcquire(s_sem, 20);   /* 有数据立刻醒，没有 20ms 一轮 */
            rx_poll();		//处理收到的数据

            if (s_ap_lost || s_link_closed || s_restart_req)
            {
                Uart_Printf("[WIFI] 连接断开\r\n");
                break;
            }

            if (s_report_due)
            {
                s_report_due = 0;

							if (report_once() != 0)		//向云端上报数据
                {
                    Uart_Printf("[WIFI] 上报失败，重连\r\n");
                    break;
                }
                s_last_ok = osKernelGetTickCount();
            }

            /* 15s 一帧都没发成功，说明链路已经死了（关路由器时可能收不到 CLOSED） */
            if ((osKernelGetTickCount() - s_last_ok) > 15000U)
            {
                Uart_Printf("[WIFI] 长时间未上报成功，重连\r\n");
                break;
            }
        }

        /* ---------------- 3. 断线收尾，等 30s 重新来过 ---------------- */
        wifi_connected = 0;
        connect_flag   = 0;
        s_report_due   = 0;
        s_ap_lost = s_link_closed = s_ap_back = 0;

        Uart_Printf("[WIFI] %d 秒后重连\r\n", (int)(WIFI_RECONNECT_MS / 1000U));
        wait_retry();
    }
}
