#ifndef __ALARM_LOG_H
#define __ALARM_LOG_H

#include <stdint.h>

/*
 * 报警历史记录（RAM 环形缓冲）
 * ------------------------------------------------------------
 * 最多保存最近 20 条「报警 / 恢复」记录，掉电丢失。
 * 触发方式：每轮轮询 7 类报警状态，检测到上升沿/下降沿就记一条。
 * 读取方式：AlarmLog_Get(0) 是最新一条，AlarmLog_Get(count-1) 是最旧一条。
 * ------------------------------------------------------------
 */

#define ALARM_LOG_MAX       20      /* 环形缓冲容量：最近 20 条 */
#define ALARM_LOG_LINE_LEN  28      /* 单条显示文本最大字节数（含结尾 0） */

/* 报警类型：7 类分开做边沿检测，所以电压/电流的过高和过低能分别记录 */
#define ALOG_TEMP_HIGH   0      /* 温度超限 */
#define ALOG_HUMI_HIGH   1      /* 湿度超限 */
#define ALOG_FIRE        2      /* 发现明火 */
#define ALOG_VOLT_HIGH   3      /* 电压过高 */
#define ALOG_VOLT_LOW    4      /* 电压过低 */
#define ALOG_CURR_HIGH   5      /* 电流过高 */
#define ALOG_CURR_LOW    6      /* 电流过低 */
#define ALOG_TYPE_NUM    7

typedef struct
{
    uint32_t tick;      /* 发生时刻：osKernelGetTickCount()，单位 ms */
    uint8_t  type;      /* ALOG_xxx */
    uint8_t  recover;   /* 0 = 报警触发   1 = 恢复正常 */
} alarm_log_t;

/* 边沿检测 + 记录：放在 Ui_Task() 里每轮调一次 */
void AlarmLog_Task(void);

/* 当前已记录条数（0 ~ ALARM_LOG_MAX） */
uint8_t AlarmLog_Count(void);

/* 取第 idx 条，idx=0 是最新的一条；越界返回 NULL */
const alarm_log_t *AlarmLog_Get(uint8_t idx);

/* 有新记录时返回 1（读完自动清零），UI 用它决定要不要重建滚轮 */
uint8_t AlarmLog_TakeChanged(void);

/* 把全部记录格式化成滚轮用的 "\n" 分隔文本
   buf 至少 ALARM_LOG_MAX * ALARM_LOG_LINE_LEN 字节 */
void AlarmLog_BuildOptions(char *buf, uint16_t size);

#endif
