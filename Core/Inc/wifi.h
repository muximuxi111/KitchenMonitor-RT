#ifndef _WIFI_H
#define _WIFI_H

#include <string.h>
#include "usart.h"      /* huart3 / Uart_Printf / USART3_RxBuff */
#include "wifi_config.h" /* 本机热点账号密码：不入版本库，模板见 wifi_config.example.h */

/* ==========================================================================
 * ESP-12F（AT 固件）联网：非阻塞状态机
 * --------------------------------------------------------------------------
 * 流程：AT 测试 → AT+CWMODE=1 → AT+CWJAP → AT+CIPMUX=0 → AT+CIPSTART
 *       → 每 2s 用 AT+CIPSEND 上报一帧 → 收 +IPD 里的远程指令
 *
 * 三条硬要求：
 *   1) 每一步都有超时，失败最多重试 3 次，全失败就等 30s 重来；
 *   2) 全程只用 osDelay，绝不用 HAL_Delay —— 否则会卡住别的任务；
 *   3) 联网失败不影本地采集/显示/报警，只是主页图标变成"断开连接"。
 * ========================================================================== */

/* 默认热点（可以在屏幕「WIFI登录」页里改，改完不用重新编译）
 * 真实值写在 wifi_config.h，该文件已在 .gitignore 中，不会提交到仓库 */
#define WIFI_SSID_DEFAULT   WIFI_CONFIG_SSID
#define WIFI_PASS_DEFAULT   WIFI_CONFIG_PASS

/* 教师指定的 TCP 服务器（电脑上用网络调试助手开 TCP Server 监听这个端口） */
#define ServerIP            "129.211.183.41"
#define ServerPort          9010

/* 每步最多重试几次、全失败后等多久重连 */
#define WIFI_RETRY_MAX      3
#define WIFI_RECONNECT_MS   30000U

/* 上报周期（软件定时器 WifiTimer 的周期） */
#define WIFI_REPORT_MS      2000U

/* 联网状态 */
typedef enum
{
    WIFI_ST_BOOT = 0,   /* 上电等模块启动 */
    WIFI_ST_AT,         /* AT 测试 */
    WIFI_ST_CWMODE,     /* AT+CWMODE=1  Station 模式 */
    WIFI_ST_CWJAP,      /* AT+CWJAP 连热点 */
    WIFI_ST_CIPMUX,     /* AT+CIPMUX=0  单连接 */
    WIFI_ST_CIPSTART,   /* AT+CIPSTART 连 TCP 服务器 */
    WIFI_ST_ONLINE,     /* 已连上：周期上报 + 收指令 */
    WIFI_ST_RETRY       /* 断开：等 30s 再重连 */
} wifi_state_t;

/* 供界面显示用：1 = 已连上服务器 */
extern volatile uint8_t wifi_connected;

/* 供其它模块查询进度（0 表示没连上） */
int  Wifi_State(void);

/* ---- 下面这些由 freertos.c 里的任务/回调调用 ---- */
void Wifi_Task(void);               /* 状态机主体，WifiTask 里死循环调用 */
void Wifi_RxIsr(const uint8_t *buf, uint16_t len);  /* UART3 中断里调，只做入队 */
void Wifi_OnReportTimer(void);      /* 2s 软件定时器回调里调，只置标志 */
int  Wifi_HandleCommand(const char *cmd);           /* MUTE / UNMUTE / FAN:n / STAT */

/* ---- 界面调用 ---- */
void Wifi_SetCredentials(const char *ssid, const char *pass);  /* 换热点 */
void Wifi_RequestReconnect(void);   /* 立刻重连一次 */

/* 读当前热点（只读，给 WIFI 页的输入框回填用） */
const char *Wifi_GetSsid(void);
const char *Wifi_GetPass(void);

#endif
