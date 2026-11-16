#include "alarm_log.h"
#include "cmsis_os2.h"      /* osKernelGetTickCount */
#include "usart.h"          /* Uart_Printf */
#include "sensor.h"         /* Tem_value / Hum_value */
#include "adc_monitor.h"    /* test_V / test_A */
#include "system_state.h"   /* temp_alarm_th / humi_alarm_th / 电压电流阈值 / fire_warning */
#include "task_state.h"     /* alarm_enable */
#include <stdio.h>
#include <string.h>

/* ==================== 环形缓冲 ==================== */

static alarm_log_t ring[ALARM_LOG_MAX];

static uint8_t  head      = 0;      /* 下一条要写的位置 */
static uint8_t  count     = 0;      /* 当前条数 */
static uint8_t  changed   = 0;      /* 有新记录，等 UI 来取 */
static uint8_t  baselined = 0;      /* 第一轮只建立基线，不记录 */

/* 上一轮的 7 类报警状态，用来做边沿检测 */
static uint8_t  prev_state[ALOG_TYPE_NUM] = {0};

/* 每一类上次记录的时刻，用来做 1 秒去抖 */
static uint32_t last_tick[ALOG_TYPE_NUM]  = {0};

/* ==================== 工具函数 ==================== */

/* 报警类型 + 是否恢复 → 显示文字
   ui_font_KaiTi 现在已收录：过、高、低、焰、恢、复、记、录、环、历、史、页、上、下……
   所以电压电流的过高/过低可以分开写清楚，恢复统一说「XX恢复」 */
static const char *TypeText(uint8_t type, uint8_t recover)
{
    switch (type)
    {
    case ALOG_TEMP_HIGH: return recover ? "温度恢复" : "温度过高";
    case ALOG_HUMI_HIGH: return recover ? "湿度恢复" : "湿度过高";
    case ALOG_FIRE:      return recover ? "明火恢复" : "发现明火";
    case ALOG_VOLT_HIGH: return recover ? "电压恢复" : "电压过高";
    case ALOG_VOLT_LOW:  return recover ? "电压恢复" : "电压过低";
    case ALOG_CURR_HIGH: return recover ? "电流恢复" : "电流过高";
    case ALOG_CURR_LOW:  return recover ? "电流恢复" : "电流过低";
    default:             return "?";
    }
}

/* ms → HH:MM:SS（系统没开 RTC，这里只能给"开机以来运行时长"） */
static void FormatTime(uint32_t tick, char *buf, uint16_t size)
{
    uint32_t sec = tick / 1000;

    snprintf(buf, size, "%02lu:%02lu:%02lu",
             (unsigned long)(sec / 3600),
             (unsigned long)((sec % 3600) / 60),
             (unsigned long)(sec % 60));
}

/* 写一条记录 */
static void Add(uint8_t type, uint8_t recover)
{
    char     tbuf[12];
    uint32_t t = osKernelGetTickCount();

    ring[head].tick    = t;
    ring[head].type    = type;
    ring[head].recover = recover;

    head = (uint8_t)((head + 1) % ALARM_LOG_MAX);
    if (count < ALARM_LOG_MAX) count++;     /* 满了就覆盖最旧的一条 */
    changed = 1;

    /* 同时往调试串口打一份，方便不看屏也能对时间 */
    FormatTime(t, tbuf, sizeof(tbuf));
    Uart_Printf("[ALARM] %s %s\r\n", tbuf, TypeText(type, recover));
}

/* ==================== 状态采集 ==================== */

/* 读当前 7 类报警状态：1 = 触发，0 = 正常
   带 alarm_enable 闸门，和 Evt_UpdateFromSensors() / fire_alarm() 保持一致：
   设置页没勾选的项，不会真的报警，这里也不记日志 */
static void ReadState(uint8_t *s)
{
    s[ALOG_TEMP_HIGH] = ((alarm_enable & 0x01) && (Tem_value >= temp_alarm_th)) ? 1 : 0;
    s[ALOG_HUMI_HIGH] = ((alarm_enable & 0x02) && (Hum_value >= humi_alarm_th)) ? 1 : 0;
    s[ALOG_FIRE]      = ((alarm_enable & 0x04) && fire_warning)                 ? 1 : 0;

    s[ALOG_VOLT_HIGH] = ((alarm_enable & 0x08) && (test_V >= VOLTAGE_ALARMH))   ? 1 : 0;
    s[ALOG_VOLT_LOW]  = ((alarm_enable & 0x08) && (test_V <= VOLTAGE_ALARML))   ? 1 : 0;
    s[ALOG_CURR_HIGH] = ((alarm_enable & 0x08) && (test_A >= CURRENT_ALARMH))   ? 1 : 0;
    s[ALOG_CURR_LOW]  = ((alarm_enable & 0x08) && (test_A <= CURRENT_ALARML))   ? 1 : 0;
}

/* ==================== 对外接口 ==================== */

void AlarmLog_Task(void)
{
    uint8_t  now[ALOG_TYPE_NUM];
    uint32_t t;
    uint8_t  i;

    ReadState(now);

    /* 第一轮只把当前状态当作基线，不记录。
       调用方（set_event 任务）已经阻塞在 myBinarySem02 上等到首帧真实数据，
       所以这里的基线是准的。留这个机制是为了防止"一进循环就把当前状态当成刚发生的变化"。
       另外 1 秒去抖也以这一轮的时间戳为起点。 */
    if (!baselined)
    {
        baselined = 1;
        memcpy(prev_state, now, ALOG_TYPE_NUM);
        t = osKernelGetTickCount();
        for (i = 0; i < ALOG_TYPE_NUM; i++) last_tick[i] = t;
        return;
    }

    t = osKernelGetTickCount();

    for (i = 0; i < ALOG_TYPE_NUM; i++)
    {
        if (now[i] == prev_state[i]) continue;      /* 状态没变 */

        prev_state[i] = now[i];

        /* 去抖：同一类 1 秒内只记一条。
           温度在阈值上下 0.1℃ 抖的时候，不清这个会刷满 20 条。 */
        if ((t - last_tick[i]) < 1000) continue;
        last_tick[i] = t;

        Add(i, now[i] ? 0 : 1);
    }
}

uint8_t AlarmLog_Count(void)
{
    return count;
}

const alarm_log_t *AlarmLog_Get(uint8_t idx)
{
    uint8_t pos;

    if (idx >= count) return NULL;

    /* head 指向下一条要写的位置，所以最新一条在 head-1，往回倒着取 */
    pos = (uint8_t)((head + ALARM_LOG_MAX - 1 - idx) % ALARM_LOG_MAX);
    return &ring[pos];
}

uint8_t AlarmLog_TakeChanged(void)
{
    uint8_t c = changed;

    changed = 0;
    return c;
}

void AlarmLog_BuildOptions(char *buf, uint16_t size)
{
    uint16_t pos = 0;
    uint8_t  i;

    if (buf == NULL || size < 2) return;

    if (count == 0)
    {
        snprintf(buf, size, "系统正常");
        return;
    }

    for (i = 0; i < count; i++)
    {
        const alarm_log_t *e = AlarmLog_Get(i);
        char    tbuf[12];
        int     n;

        FormatTime(e->tick, tbuf, sizeof(tbuf));

        n = snprintf(buf + pos, (size_t)(size - pos), "%s %s",
                     tbuf, TypeText(e->type, e->recover));
        if (n < 0) break;

        /* snprintf 返回的是"本该写多长"，可能超过剩余空间，必须自己钳位 */
        if ((uint16_t)n >= (uint16_t)(size - pos))
        {
            pos = (uint16_t)(size - 1);
            break;
        }
        pos = (uint16_t)(pos + n);

        if (i + 1 < count)                  /* 滚轮用 \n 分隔每一项 */
        {
            if ((uint16_t)(pos + 1) >= size) break;
            buf[pos++] = '\n';
        }
    }

    buf[pos] = '\0';
}
