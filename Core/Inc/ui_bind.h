#ifndef __UI_BIND_H
#define __UI_BIND_H
#include "ui.h" 
/*
 * UI 绑定层
 * ------------------------------------------------------------
 * 作用：把「SquareLine 生成的控件」和「业务变量」对接起来。
 *       业务逻辑（三态裁决、报警判断）不写在这里。
 *
 * 使用：LCD 任务的 while(1) 里调一次 Ui_Task() 即可。
 * ------------------------------------------------------------
 */

/* 以下函数已由 Ui_Task() 统一调用，一般不需要单独调 */
void Ui_Refresh(void);        /* Home：传感器数值 → 标签 */
void Ui_State_Refresh(void);  /* Home：三态 → 圆点图片 + 状态文字 */
void Checkbox_EventCb(lv_event_t *e);	//判断复选框状态执行或清除相应事件
void Ui_Bind_Init(void);		
void Ui_Warning_Task(void);   /* Warning：消音开关 ↔ EVT_MUTED */
void Ui_AlarmLog_Task(void);  /* Warning：报警历史 → 滚轮 ui_Roller2 */
void Ui_Key_Task(void);       /* 按键请求 → 切屏（KEY1 轮换 / KEY2 进设置） */
void Ui_solid_control(void);  /* Setting：滑块 → fan_speed（手动模式转速） */
void Ui_Fan_Mode_Task(void);  /* Setting：自动/手动开关 ↔ fan_manual */
void Ui_Threshold_Init(void); /* Setting：温湿度阈值滑块范围 + 上电位置 */
void Ui_Threshold_Task(void); /* Setting：温湿度阈值滑块 → temp_alarm_th / humi_alarm_th */
void Ui_Fan_Level_Init(void); /* Home：风扇档位标签，只建一次 */
void Ui_Fan_Level_Refresh(void); /* Home：风扇档位 → 标签文字 */
void Wifi_Ui_Init(void);      /* WIFI 页：账号输入框回填 + 主页网络图标定时刷新 */
/* 对外唯一入口：LCD 任务循环里调用 */
void Ui_Task(void);

#endif
