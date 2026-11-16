#include "ui_bind.h"
#include "ui.h"           /* SquareLine 生成的控件：ui_Tem_label / ui_Switch2 ... */
#include "sensor.h"       /* Tem_value / Hum_value / Als_value  温湿度光照变量*/
#include "adc_monitor.h"  /* test_V / test_A  电压电流变量*/
#include "task_state.h"   /* sys_state / alarm_enable / EVT_MUTED  三态标志位和警报使能标志位变量*/
#include "alarm.h"        /* fan_speed / fan_manual / fan_duty  风扇相关变量*/
#include "system_state.h" /* temp_alarm_th / humi_alarm_th  温湿度报警阈值*/
#include "alarm_log.h"    /* AlarmLog_*  报警历史记录（环形缓冲） */
#include "key.h"          /* g_key1_request / g_key2_request  按键请求 */
#include "wifi.h"         /* wifi_connected / Wifi_SetCredentials  网络状态 */
#include "usart.h"        /* Uart_Printf */
#include <stdio.h>

/* ==================== Home 页 ==================== */
//刷新home页显示数据
void Ui_Refresh(void)
{
    char buf[40];

    if (ui_Tem_label == NULL) return;   /* 还没 ui_init 完成，直接返回，别碰空控件 */

    snprintf(buf, sizeof(buf), "温度 %.2f C", Tem_value);
    lv_label_set_text(ui_Tem_label, buf);

    snprintf(buf, sizeof(buf), "湿度 %.2f %%", Hum_value);
    lv_label_set_text(ui_Hum_label, buf);

    snprintf(buf, sizeof(buf), "电压 %.1f V", test_V);
    lv_label_set_text(ui_voltage_label, buf);

    snprintf(buf, sizeof(buf), "电流 %.2f A", test_A);
    lv_label_set_text(ui_Current_label, buf);

    snprintf(buf, sizeof(buf), "光照 %d lx", (int)Als_value);
    lv_label_set_text(ui_light_label, buf);
}

//刷新home页的状态
void Ui_State_Refresh(void)
{
    static uint8_t last_state = 0xFF;   /* 非法初值，保证上电刷一次 */

    if (ui_Image1 == NULL) return;
    if (sys_state == last_state) return;   /* 状态没变就不用重绘 */

    last_state = sys_state;			//更新当前状态，用于和下一次的状态比较

    switch (sys_state)
    {
    case SYS_WARN:
        lv_img_set_src(ui_Image1, &ui_img_warning_png);
        lv_label_set_text(ui_Label1, "系统状态:警告!!!");
        break;

    case SYS_ALARM:
        lv_img_set_src(ui_Image1, &ui_img_alarm_png);
        lv_label_set_text(ui_Label1, "系统状态:报警!!!");
        break;

    case SYS_NORMAL:
    default:
        lv_img_set_src(ui_Image1, &ui_img_normal_png);
        lv_label_set_text(ui_Label1, "系统状态:正常");
        break;
    }
}

/* 主页「风扇档位」标签
   SquareLine 导出的 Home 上没有这个控件，这里自己在 ui_Container1 里建一个，
   位置放在「光照」右边那块空位（60, 39）。
   注意用默认字体，所以只显示英文+数字；以后在 SquareLine 里加了中文标签，
   把 fan_level_label 换成那个控件名即可（顺便能显示中文）。 */
static lv_obj_t *fan_level_label = NULL;

void Ui_Fan_Level_Init(void)
{
    if (fan_level_label != NULL) return;      /* 只建一次 */
    if (ui_Container1 == NULL) return;

    fan_level_label = lv_label_create(ui_Container1);
    lv_obj_set_align(fan_level_label, LV_ALIGN_CENTER);
    lv_obj_set_x(fan_level_label, 60);
    lv_obj_set_y(fan_level_label, 39);
    lv_label_set_text(fan_level_label, "FAN 0%");
}

//刷新主页的风扇档位（用 fan_duty：真正写进 CCR 的占空比）
void Ui_Fan_Level_Refresh(void)
{
    static uint8_t last_duty = 0xFF;		//非法初值，上电必刷一次
    char buf[24];

    if (fan_level_label == NULL) return;
    if (fan_duty == last_duty) return;		//没变化就不重绘
    last_duty = fan_duty;

    snprintf(buf, sizeof(buf), "FAN %d%%", fan_duty);
    lv_label_set_text(fan_level_label, buf);		//设置主页风扇转速标签的值
}

/* ==================== Setting 页 ==================== */

void Checkbox_EventCb(lv_event_t *e)
{
    // 获取触发当前事件的控件（即用户点击的复选框）
    lv_obj_t *cb = lv_event_get_target(e);

    // 判断当前复选框是否处于勾选状态
    // 勾选时 checked = 1，取消勾选时 checked = 0
    uint8_t checked = lv_obj_has_state(cb, LV_STATE_CHECKED) ? 1 : 0;

    // 判断是哪个复选框，并修改 alarm_enable 对应的使能位

    // 温度报警：使用 bit0（第0位）
    if (cb == ui_Tem_Checkbox)
    {
        if (checked)
            alarm_enable |= 0x01;       // 勾选：将 bit0 设置为 1，启用温度报警
        else
            alarm_enable &= ~0x01;      // 取消勾选：将 bit0 清零，关闭温度报警
    }

    // 湿度报警：使用 bit1（第1位）
    else if (cb == ui_Hum_Checkbox)
    {
        if (checked)
            alarm_enable |= 0x02;       // 勾选：将 bit1 设置为 1，启用湿度报警
        else
            alarm_enable &= ~0x02;      // 取消勾选：将 bit1 清零，关闭湿度报警
    }

    // 明火报警：使用 bit2（第2位）
    else if (cb == ui_Fire_Checkbox)
    {
        if (checked)
            alarm_enable |= 0x04;       // 勾选：将 bit2 设置为 1，启用明火报警
        else
            alarm_enable &= ~0x04;      // 取消勾选：将 bit2 清零，关闭明火报警
    }

    // 用电报警：使用 bit3（第3位）
    else if (cb == ui_volatage_Checkbox)
    {
        if (checked)
            alarm_enable |= 0x08;       // 勾选：将 bit3 设置为 1，启用用电报警
        else
            alarm_enable &= ~0x08;      // 取消勾选：将 bit3 清零，关闭用电报警
    }
}

/* 注意：这个函数只能在 ui_init() 之后调用一次！
   以前放在 Ui_Task() 里每 5ms 调一次，会不停往控件上挂事件回调，
   LVGL 堆(48KB)几秒就被吃光，之后 lv_mem_alloc 失败触发
   LV_ASSERT_HANDLER(=while(1)) 把 LCD 任务钉死 -> 白屏 + 整个界面不动。 */
void Ui_Bind_Init(void)
{
    Ui_Fan_Level_Init();        /* 主页风扇档位标签（运行时建的，只建一次） */
    Ui_Threshold_Init();        /* 温湿度阈值滑块范围 + 上电位置 */

    if (ui_Tem_Checkbox == NULL) return;

		//(指定哪个控件,指定事件发生时执行的回调函数,指定监听什么事件,用户自定义数据)
    lv_obj_add_event_cb(ui_Tem_Checkbox,       Checkbox_EventCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Hum_Checkbox,       Checkbox_EventCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Fire_Checkbox,      Checkbox_EventCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_volatage_Checkbox,  Checkbox_EventCb, LV_EVENT_VALUE_CHANGED, NULL);
}
/*lv_obj_add_event_cb() 的四个参数：
第一个	指定需要绑定事件的控件
第二个	指定事件发生时执行的回调函数
第三个	指定监听哪种事件
第四个	用户自定义数据，这里不需要，所以填 NULL
*/

/* ==================== Setting 页 风速控制==================== */
//滑动条控制pwm：把滑块的值抄到 fan_speed（0~100 就是占空比）
void Ui_solid_control(void)
{
    static lv_obj_t *last_slider = NULL;

    if (ui_Fan_Slider == NULL)			//设置页还没建出来
    {
        return;
    }

    /* SquareLine 生成的滑块默认值是 0。要求初始值设置成60%*/
    if (ui_Fan_Slider != last_slider)
    {
        last_slider = ui_Fan_Slider;
        lv_slider_set_value(ui_Fan_Slider, fan_speed, LV_ANIM_OFF);		
        return;
    }
		
		if (cloud_fan_override)	//表示在云端模式控制下
    {		
				//第三个参数：LV_ANIM_OFF立即跳转到目标数值，LV_ANIM_ON	使用动画过渡到目标数值
        lv_slider_set_value(ui_Fan_Slider, cloud_fan_speed, LV_ANIM_OFF);	
        return;
    }
    fan_speed = (uint8_t)lv_slider_get_value(ui_Fan_Slider);   /* 0~100 = 占空比 */
}

/* ==================== Setting 页：温湿度阈值 ==================== */

/* 滑块范围一次设好：温度 20~60℃，湿度 20~95%
   ARR 那套百分比跟这里无关，滑块值直接就是阈值本身（整数够用） */
//这里只需要初始化一次，如果不断初始化会影响滑块的滑动
void Ui_Threshold_Init(void)
{
    if (ui_Tem_Slider != NULL)
    {
        lv_slider_set_range(ui_Tem_Slider, TEMP_SLIDER_MIN, TEMP_SLIDER_MAX);	//设置滑块值的范围
        lv_slider_set_value(ui_Tem_Slider, (int)temp_alarm_th, LV_ANIM_OFF);	//设置滑块的初始值
    }

    if (ui_Hum_Slider != NULL)
    {
        lv_slider_set_range(ui_Hum_Slider, HUMI_SLIDER_MIN, HUMI_SLIDER_MAX);
        lv_slider_set_value(ui_Hum_Slider, (int)humi_alarm_th, LV_ANIM_OFF);
    }
}

/* 拖动滑块 → 改阈值变量 → Evt_UpdateFromSensors 下一轮就用新值判断
   同时把标题文字改成「温度阈值 30度」，不额外占界面空间 */
void Ui_Threshold_Task(void)
{
    static int16_t last_tem = -1;		//非法初值，上电必刷一次
    static int16_t last_hum = -1;
    char buf[24];

    if (ui_Tem_Slider != NULL)
    {
        int v = lv_slider_get_value(ui_Tem_Slider);
        if (v != last_tem)	//滑块的值改变才赋值
        {
            last_tem = v;
            temp_alarm_th = (float)v;		//新的温度阈值
            if (ui_Label != NULL)
            {
                snprintf(buf, sizeof(buf), "温度阈值 %d度", v);
                lv_label_set_text(ui_Label, buf);
            }
        }
    }

    if (ui_Hum_Slider != NULL)
    {
        int v = lv_slider_get_value(ui_Hum_Slider);
        if (v != last_hum)
        {
            last_hum = v;
            humi_alarm_th = (float)v;
            if (ui_Label4 != NULL)
            {
                snprintf(buf, sizeof(buf), "湿度阈值 %d%%", v);
                lv_label_set_text(ui_Label4, buf);
            }
        }
    }
}


/* ==================== Setting 页：风扇自动挡手动挡开关设置 ==================== */
/* 设置页「自动模式」开关 ↔ fan_manual 双向同步
   未开启 = 自动模式：标签"自动模式"，风扇由报警状态决定（WARN60% / ALARM100%）
   开启   = 手动模式：标签"手动模式"，风扇跟滑块走 */
void Ui_Fan_Mode_Task(void)
{
    static lv_obj_t *last_obj     = NULL;
    static uint8_t   last_checked = 0;	//上次开关状态

    if (ui_Fan_Switch == NULL)			//设置页还没建出来
    {
        return;
    }

    uint8_t cur = lv_obj_has_state(ui_Fan_Switch, LV_STATE_CHECKED) ? 1 : 0;	//当前开关状态

    /* 第一次见到这个开关（或设置页被重建）：按 fan_manual 初始化显示 */
    if (ui_Fan_Switch != last_obj)
    {
        last_obj     = ui_Fan_Switch;
        last_checked = fan_manual;

        if (fan_manual)
        {
            lv_obj_add_state(ui_Fan_Switch, LV_STATE_CHECKED);		//手动：开关打开
            if (ui_Fan_Label) lv_label_set_text(ui_Fan_Label, "手动模式");
        }
        else
        {
            lv_obj_clear_state(ui_Fan_Switch, LV_STATE_CHECKED);	//自动：开关关闭
            if (ui_Fan_Label) lv_label_set_text(ui_Fan_Label, "自动模式");
        }
        return;
    }

    /* 用户拨动了开关 */
    if (cur != last_checked)
    {
        last_checked = cur;
        fan_manual   = cur;			//0=自动 1=手动，wait_event 里的 FAN_Update() 会读它

        if (cur)
        {
            if (ui_Fan_Label) lv_label_set_text(ui_Fan_Label, "手动模式");
        }
        else
        {
            if (ui_Fan_Label) lv_label_set_text(ui_Fan_Label, "自动模式");
        }
    }
}

/* ==================== Warning 页 消音开关控制 ==================== */

/*
 * 消音开关与 EVT_MUTED 双向同步：
 *   1) 用户拨开关   → 写事件位
 *   2) KEY3 / 云端 MUTE 改了事件位 → 更新开关显示
 * 用「上一轮记下的值」区分这两种变化，避免互相覆盖。
 */
void Ui_Warning_Task(void)
{
    static lv_obj_t *last_obj     = NULL;
    static uint8_t   last_checked = 0;	//用于保存上次蜂鸣器消音的状态

    if (ui_Switch2 == NULL) return;   /* 还没进过报警页 */

    uint8_t cur   = lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED) ? 1 : 0;		//获取蜂鸣器静音开关状态
    uint8_t muted = (osEventFlagsGet(myEvent01Handle) & EVT_MUTED) ? 1 : 0;		//获取当前蜂鸣器是否被消音的状态

    /* 第一次见到这个开关：按事件位初始化显示 */
    if (ui_Switch2 != last_obj)
    {
        last_obj     = ui_Switch2;	//第一次进入报警页 ui_Switch2 指向新创建的开关。
        last_checked = muted;

        if (muted)
        {
            lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);		//如果蜂鸣器处于消音状态开关置位
            lv_obj_clear_flag(ui_Mute_Image, LV_OBJ_FLAG_HIDDEN);	//显示静音图标
            lv_obj_add_flag(ui_UNMUTE_Image, LV_OBJ_FLAG_HIDDEN);	//隐藏未静音图标
        }
        else
        {
            lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);	//如果蜂鸣器不处于消音状态开关取消置位
            lv_obj_add_flag(ui_Mute_Image, LV_OBJ_FLAG_HIDDEN);		//隐藏静音图标
            lv_obj_clear_flag(ui_UNMUTE_Image, LV_OBJ_FLAG_HIDDEN);	//显示未静音图标
        }
    }
    if (cur != last_checked)            /* 用户拨动了开关 */
    {
        last_checked = cur;

        if (cur)
        {
            osEventFlagsSet(myEvent01Handle, EVT_MUTED);
            lv_obj_clear_flag(ui_Mute_Image, LV_OBJ_FLAG_HIDDEN);	//显示静音图标
            lv_obj_add_flag(ui_UNMUTE_Image, LV_OBJ_FLAG_HIDDEN);	//隐藏未静音图标
        }
        else
        {
            osEventFlagsClear(myEvent01Handle, EVT_MUTED);
            lv_obj_add_flag(ui_Mute_Image, LV_OBJ_FLAG_HIDDEN);		//隐藏静音图标
            lv_obj_clear_flag(ui_UNMUTE_Image, LV_OBJ_FLAG_HIDDEN);	//显示未静音图标
        }
    }
    else if (muted != cur)              /* 别处改了状态：同步显示 */
    {
        last_checked = muted;

        if (muted)
        {
            lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
            lv_obj_clear_flag(ui_Mute_Image, LV_OBJ_FLAG_HIDDEN);	//显示静音图标
            lv_obj_add_flag(ui_UNMUTE_Image, LV_OBJ_FLAG_HIDDEN);	//隐藏未静音图标
        }
        else
        {
            lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
            lv_obj_add_flag(ui_Mute_Image, LV_OBJ_FLAG_HIDDEN);		//隐藏静音图标
            lv_obj_clear_flag(ui_UNMUTE_Image, LV_OBJ_FLAG_HIDDEN);	//显示未静音图标
        }
    }
    /* TODO: 报警原因 ui_Season_label、发生时间 ui_TIme_label 之后在这里刷新 */
}



/* ==================== Warning 页 报警历史滚轮控制 ==================== */
/* 滚轮的 options 文本缓冲区：20 条 × 28 字节 = 560 字节。
   必须放 static —— LCD 任务栈只有 4KB，放成局部变量会直接栈溢出。 */
static char alog_opts[ALARM_LOG_MAX * ALARM_LOG_LINE_LEN];

/*
 * 报警页滚轮 ui_Roller2：显示最近 20 条「报警 / 恢复」记录。
 */
void Ui_AlarmLog_Task(void)
{
    static lv_obj_t *last_roller  = NULL;
    static uint8_t   need_refresh = 1;		//第一次见到滚轮要刷一次

    if (ui_Roller2 == NULL)				//报警页还没建出来
    {
        return;
    }

    /* 第一次见到滚轮（或报警页被重建）：换中文字体 + 强制刷一次 */
    if (ui_Roller2 != last_roller)
    {
        last_roller = ui_Roller2;
        lv_obj_set_style_text_font(ui_Roller2, &ui_font_KaiTi, LV_PART_MAIN);
        need_refresh = 1;
    }

    if (AlarmLog_TakeChanged()) need_refresh = 1;	//alarm_log 那边新记了一条
    if (!need_refresh) return;

    need_refresh = 0;

    AlarmLog_BuildOptions(alog_opts, sizeof(alog_opts));		//将报警缓冲区中的记录转换成字符串，保存到 alog_opts

    /* 记住用户翻到第几行，重建后尽量保持不动 */
    uint16_t sel = lv_roller_get_selected(ui_Roller2);

    lv_roller_set_options(ui_Roller2, alog_opts, LV_ROLLER_MODE_NORMAL);	

    if (sel >= AlarmLog_Count()) sel = 0;
    lv_roller_set_selected(ui_Roller2, sel, LV_ANIM_OFF);	//表示不使用动画，直接切换
}


/* ==========================================================================
 * 按键 → 切屏
 * ========================================================================== */
void Ui_Key_Task(void)
{
    lv_obj_t *cur = NULL;       /* 当前正在显示的是哪一屏 */

    /* ui_init() 还没跑完，屏幕都还不存在，什么都别做 */
    if (ui_Home == NULL)
    {
        return;
    }

    cur = lv_scr_act();         /* 问 LVGL：现在显示的是哪一屏 */

    /* ------------------------------------------------------------------
     * 按键2：进入设置页
     * ------------------------------------------------------------------ */
    if (g_key2_request == 1)
    {
        g_key2_request = 0;             /* 请求用掉了，清 0，否则每轮都切 */

        if (cur != ui_Setting)          /* 已经在设置页就不用重复切了 */
        {
            _ui_screen_change(&ui_Setting,              /* 要切到哪一屏 */
                              LV_SCR_LOAD_ANIM_FADE_ON, /* 淡入效果 */
                              300,                      /* 动画 300ms */
                              0,                        /* 不延时，立刻开始 */
                              &ui_Setting_screen_init); /* 万一屏还没建，用它建 */
        }

        return;                         /* 一次只处理一个键 */
    }

    /* ------------------------------------------------------------------
     * 按键1：循环切换 主页 → 设置页 → 报警页 → 主页
     * ------------------------------------------------------------------ */
    if (g_key1_request == 1)
    {
        g_key1_request = 0;

        if (cur == ui_Home)
        {
            /* 主页 → 设置页 */
            _ui_screen_change(&ui_Setting,
                              LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                              &ui_Setting_screen_init);
        }
        else if (cur == ui_Setting)
        {
            /* 设置页 → 报警页 */
            _ui_screen_change(&ui_Warning,
                              LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                              &ui_Warning_screen_init);
        }
        else
        {
            /* 报警页（或者其它任何情况）→ 主页 */
            _ui_screen_change(&ui_Home,
                              LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                              &ui_Home_screen_init);
        }
    }
}

/* ==========================================================================
 * WiFi 页：账号输入 + 主页网络图标
 * --------------------------------------------------------------------------
 * 这一段原来写在 wifi.c 里，但它全是 LVGL 操作，而 LVGL 只有 LCD 任务能碰，
 * 所以搬到 ui_bind.c（界面绑定层），wifi.c 从此只管「联网 + 收发」。
 * ========================================================================== */
static lv_obj_t *s_bound_btn  = NULL;
static lv_obj_t *s_icon_obj   = NULL;    /* 上次刷过的图标对象 */
static uint8_t   s_icon_shown = 0xFF;
static uint8_t   s_prefilled  = 0;

/* 主页 WiFi 图标：直接换图，不依赖 LV_STATE_CHECKED
 *
 * 原来 SquareLine 里这个 ImgButton 是这样配的：
 *   RELEASED         → 断开连接.png
 *   PRESSED          → WIFI (1).png
 *   CHECKED_RELEASED → WIFI (1).png
 * 也就是说"按住"时才是 Wi-Fi 图，松手就回到断开。
 * 光靠 lv_obj_add_state(CHECKED) 会和 PRESSED 图打架（按一下变 Wi-Fi、松手又断），
 * 所以这里把四个状态的图全设成同一张，视觉效果完全由 wifi_connected 决定。 */
static void wifi_icon_apply(uint8_t on)
{
    static const lv_imgbtn_state_t st[4] = {
        LV_IMGBTN_STATE_RELEASED, LV_IMGBTN_STATE_PRESSED,
        LV_IMGBTN_STATE_CHECKED_RELEASED, LV_IMGBTN_STATE_CHECKED_PRESSED
    };
    const void *src = on ? (const void *)&ui_img_wifi__1__png
                         : (const void *)&ui_img______png;
    int i;

    for (i = 0; i < 4; i++) lv_imgbtn_set_src(ui_Wifi_ImgButton, st[i], NULL, src, NULL);
    lv_obj_invalidate(ui_Wifi_ImgButton);
}

/* 「连接」按钮：把输入框里的账号抄下来，然后重连 */
static void wifi_connect_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    const char *ssid = (ui_nameTextArea != NULL)     ? lv_textarea_get_text(ui_nameTextArea)     : "";
    const char *pass = (ui_password_TextArea != NULL) ? lv_textarea_get_text(ui_password_TextArea) : "";

    Wifi_SetCredentials(ssid, pass);
    Wifi_RequestReconnect();
}

static void wifi_ui_timer_cb(lv_timer_t *t)
{
    (void)t;

    /* 1. 主页右上角图标：连上服务器才显示 Wi-Fi，否则显示"断开连接" */
    if (ui_Wifi_ImgButton != NULL)
    {
        uint8_t on = (wifi_connected != 0) ? 1 : 0;

        /* 换了对象（界面重建过）或者状态变了，都重刷一次 */
        if (on != s_icon_shown || ui_Wifi_ImgButton != s_icon_obj)
        {
            s_icon_shown = on;
            s_icon_obj   = ui_Wifi_ImgButton;
            wifi_icon_apply(on);
            Uart_Printf("[WIFI] 主页图标 -> %s\r\n", on ? "已联网" : "断开");
        }
    }
    else
    {
        s_icon_obj = NULL;          /* 界面还没建出来，下次重建后重新刷 */
    }

    /* 2. 只有在 WIFI 页上时才碰它的控件 */
    if (ui_WIFI_Screen != NULL && lv_scr_act() == ui_WIFI_Screen)
    {
        /* 绑一次"连接"按钮（SquareLine 里没给它挂事件） */
        if (ui_Button1 != NULL && ui_Button1 != s_bound_btn)
        {
            s_bound_btn = ui_Button1;
            lv_obj_add_event_cb(ui_Button1, wifi_connect_event_cb, LV_EVENT_CLICKED, NULL);
        }

        /* 刚进这个页面时把当前账号填进去（只填一次，别跟用户抢输入框） */
        if (!s_prefilled)
        {
            s_prefilled = 1;
            if (ui_nameTextArea != NULL)      lv_textarea_set_text(ui_nameTextArea, Wifi_GetSsid());
            if (ui_password_TextArea != NULL) lv_textarea_set_text(ui_password_TextArea, Wifi_GetPass());
        }
    }
    else
    {
        s_prefilled = 0;        /* 离开页面，下次进来再填一次 */
    }
}

void Wifi_Ui_Init(void)
{
    /* ui_init() 已经跑完，控件都在了，先把当前账号显示出来 */
    if (ui_nameTextArea != NULL)      lv_textarea_set_text(ui_nameTextArea, Wifi_GetSsid());
    if (ui_password_TextArea != NULL) lv_textarea_set_text(ui_password_TextArea, Wifi_GetPass());

    /* 200ms 刷一次图标/状态，开销可以忽略 */
    lv_timer_create(wifi_ui_timer_cb, 200, NULL);
}

/* ==================== 对外唯一入口 ==================== */

void Ui_Task(void)
{
        Ui_Key_Task();       /* 按键切屏：KEY1 轮换、KEY2 进设置页 */
        Ui_Refresh();        /* Home 数值 */
        Ui_State_Refresh();  /* Home 三态圆点 + 文字 */
        Ui_Warning_Task();   /* Warning 消音开关 */
        Ui_AlarmLog_Task();  /* Warning 报警历史滚轮 */
        Ui_solid_control();  /* Setting 风速滑块 → fan_speed */
        Ui_Threshold_Task(); /* Setting 温湿度阈值滑块 → temp_alarm_th / humi_alarm_th */
        Ui_Fan_Mode_Task();  /* Setting 自动/手动开关 → fan_manual */
        Ui_Fan_Level_Refresh();  /* Home 风扇档位（读 fan_duty） */
	
    /* ---- 3. 这一句绝对不能少！----
       LVGL 内部所有 timer（界面刷新 lv_refr、动画、触摸扫描 indev 读、
       标签滚动……）都是靠 lv_timer_handler() 驱动的。
       少了它 = ui_init() 摆好控件后再也不会画一帧 -> 屏幕永远白/黑。
       是 LVGL 的心跳函数 */
    lv_timer_handler();
}
