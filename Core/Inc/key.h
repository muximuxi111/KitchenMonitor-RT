#ifndef __KEY_H
#define __KEY_H

#include "main.h"
#include "stdio.h"
#include "i2c.h"
#include "gpio.h"
#include "system_state.h"


/*
 * 按键请求标志
 * ------------------------------------------------------------
 * Key_Scan() 扫到按键1/按键2 按稳了就把它置 1，
 * LCD 任务里的 Ui_Key_Task() 看到是 1 就去切屏，切完清 0。
 *
 * 为什么要这么绕一下：
 *   LVGL 只能在 LCD 任务里操作，不能从 wait_event 任务里直接切屏。
 * 为什么不用加锁：
 *   只有一个字节，读和写不会被打断成两半，是原子的。
 */
extern volatile uint8_t g_key1_request;     /* 1 = 有按键1 请求：切换屏幕 */
extern volatile uint8_t g_key2_request;     /* 1 = 有按键2 请求：进入设置页 */


/* 扫一次三个按键，在 wait_event 任务循环里每 10ms 调一次（不会阻塞） */
void Key_Scan(void);

/* 单独扫某一个键，一般不用直接调，Key_Scan() 已经把它们串起来了 */
void Key1_Scan(void);
void Key2_Scan(void);
void Key3_Scan(void);

/* 按键3 的动作：翻转静音位 EVT_MUTED（只做动作，不做消抖） */
void Mute_Key3(void);

#endif
