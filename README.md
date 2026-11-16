# KitchenMonitor-RT

**基于 STM32F407 + FreeRTOS + LVGL 的智能厨房环境监控终端**

厨房是家庭里火灾与用电风险最集中的空间。本项目做的是一个能**自己判断**的监控终端：同时采集 5 路环境数据，任何一路越限就进入分级响应（预警 / 报警），驱动声光提示与风扇，并把状态每 2 秒上报到云端；断网时本地功能完全不受影响。

> MCU `STM32F407IGH6` · RTOS `FreeRTOS (CMSIS-RTOS v2)` · GUI `LVGL 8.3` · 通信 `ESP-12F (AT 固件)` · IDE `Keil MDK 5` / `STM32CubeMX`

---

## 功能特性

| 模块 | 实现方式 | 说明 |
| --- | --- | --- |
| 环境感知 | SHT20 (I2C) + 光敏 (ADC) + 电压电流 (ADC+DMA) | 温度 / 湿度 / 光照 / 电压 / 电流 5 路 |
| 状态裁决 | FreeRTOS 事件组 | `NORMAL` / `WARN` / `ALARM` 三态，明火与用电异常直接进 ALARM |
| 声光报警 | 蜂鸣器 PA15 + LED2 / LED3 | 按状态分级响应，支持一键消音 |
| 主动散热 | TIM3_CH1 PWM (PC6) | 1 kHz，0~100% 占空比线性可调 |
| 人机界面 | LVGL 8.3 + ILI9341 (SPI) + FT6336 (I2C) | 主页 / 设置 / 报警 / WiFi 四屏 |
| 云端上报 | ESP-12F AT 指令 (USART3) | 2 s 一帧 CSV，支持远程指令下发 |
| 报警历史 | RAM 环形缓冲 | 最近 20 条，带边沿检测与 1 s 去抖 |
| 可靠性 | WWDG 窗口看门狗 | 30 ms 喂狗，落在 24.2~49.9 ms 窗口内 |
| 参数在线可调 | 设置页滑块 | 温度阈值 20~60 °C、湿度阈值 20~95 %，无需重新编译 |

---

## 系统架构

七个任务，三条通信线。**数据用队列传，结论用事件组传，事件用信号量唤醒。**

### 任务划分

| 任务 | 栈 (Byte) | 优先级 | 职责 |
| --- | --- | --- | --- |
| `Sensor_sendTask` | 2048 | Normal | 采集 5 路数据，组帧投递队列，并释放"数据就绪"信号量 |
| `Sensor_recvTask` | 2048 | Normal | 从队列取帧，刷新全局测量值 |
| `set_eventTask` | 512 | Normal | 阻塞等待数据就绪 → 逐项判限 → 置位对应事件位 |
| `wait_eventTask` | 512 | Normal | 等待事件组 → 裁决三态 → 驱动报警输出与风扇 |
| `LCD_TASK` | 4096 | Normal | 唯一调用 `lv_timer_handler()` 的任务，处理按键请求与界面刷新 |
| `WifiTask` | 2048 | Normal | AT 状态机：联网、2 s 上报、解析远程指令 |
| `WWDG_Task` | 512 | AboveNormal | 周期喂窗口看门狗 |

### 通信对象

| 对象 | 类型 | 参数 | 用途 |
| --- | --- | --- | --- |
| `Sensor_queue` | 消息队列 | 深度 4，`Sensor_Data_t` | 采集任务 → 接收任务，满则丢弃不阻塞 |
| `myBinarySem02` | 二值信号量 | max 1 / init **0** | "首帧数据就绪"，避免上电拿 0 值误报警 |
| `myEvent01` | 事件组 | 5 位 | `EVT_TEMP` `EVT_HUMI` `EVT_FIRE` `EVT_ELEC` `EVT_MUTED` |
| `printfMutex01` | 互斥量 | — | 串口打印互斥，防止多任务交错输出 |
| `WifiTimer` | 软件定时器 | 2000 ms | 回调里只置标志，不直接操作串口 |
| `KeyTimer` | 软件定时器 | 10 ms | 按键扫描节拍 |

### 数据流

```
SHT20 / ALS / ADC-DMA
        │
        ▼
  Sensor_sendTask ──queue(4)──▶ Sensor_recvTask
        │
        └──sem(init=0)──▶ set_eventTask ──event bits──▶ wait_eventTask
                                                              │
                                          ┌───────────────────┼──────────────────┐
                                          ▼                   ▼                  ▼
                                     BEEP / LED         FAN_Update()        WifiTask
                                                                           (2s 上报)
```

---

## 硬件与外设映射

| 功能 | 引脚 | 外设 |
| --- | --- | --- |
| LCD ILI9341 | PI0 NSS / PI1 SCK / PI2 MISO / PI3 MOSI | SPI + DMA |
| LCD 背光 / 复位 / TE | PE5 / PE2 / PE1 | GPIO |
| 触摸 FT6336 | I2C | I2C1 |
| SHT20 温湿度 | I2C | I2C1 |
| 火焰传感器 | PB3 | GPIO 输入（需关闭 JTAG，PB3=JTDO） |
| 蜂鸣器 | PA15 | GPIO 输出 |
| LED1 / LED2 / LED3 | PC4 / PC3 / PC13 | GPIO 输出 |
| 风扇 PWM | PC6 | TIM3_CH1（AF2），PSC=839, ARR=99 → 1 kHz |
| 按键 KEY1 / KEY2 / KEY3 | PC9 / PC8 / PC5 | GPIO 输入，低有效 |
| ESP-12F | USART3 | 115200，空闲中断 + DMA 收包 |
| 调试串口 | USART6 | 115200，`printf` 重定向到这里 |

---

## 目录结构

```
KitchenMonitor-RT/
├── Core/
│   ├── Inc/
│   │   ├── wifi_config.example.h   # WiFi 配置模板（复制成 wifi_config.h 使用）
│   │   ├── alarm.h  alarm_log.h  sensor.h  system_state.h  task_state.h
│   │   ├── key.h    wifi.h       ui_bind.h
│   │   └── main.h   usart.h      adc_monitor.h  ...
│   └── Src/
│       ├── main.c            # 时钟、外设初始化、PWM 频率设定
│       ├── freertos.c        # 任务创建与任务体（CubeMX 生成 + USER CODE）
│       ├── sensor.c          # SHT20 / 光照 / ADC 采集
│       ├── adc_monitor.c     # 电压电流 ADC+DMA
│       ├── task_state.c      # 判限逻辑、事件置位
│       ├── system_state.c    # 三态裁决、阈值变量
│       ├── alarm.c           # 蜂鸣器 / LED / 风扇输出
│       ├── alarm_log.c       # 报警历史环形缓冲
│       ├── key.c             # 三按键消抖与请求
│       ├── wifi.c            # ESP-12F AT 状态机
│       ├── ui_bind.c         # LVGL 界面与业务数据绑定
│       ├── usart.c           # 串口 + printf 重定向 + Uart_Printf
│       └── bsp_ft6336.c  bsp_ili9341_4line.c
├── LVGL_CUT/
│   ├── UI/                   # SquareLine 导出的界面代码与图片
│   ├── porting/              # LVGL 显示与触摸移植层
│   ├── src/                  # LVGL 8.3 裁剪源码
│   └── lv_conf.h
├── Drivers/                  # STM32F4 HAL + CMSIS
├── Middlewares/              # FreeRTOS
└── MDK-ARM/                  # Keil 工程（11.10_LVGL.uvprojx）
```

---

## 快速开始

### 1. 环境要求

- Keil MDK-ARM 5.x + `Keil::STM32F4xx_DFP` 器件包
- ARMCLANG 编译器（工程使用 V6.22）
- 目标板：`FS-STM32F407`（STM32F407IGH6）

### 2. 配置 WiFi（必做，否则编译不过）

仓库**不包含**真实的 WiFi 账号密码。首次编译前：

```
Core/Inc/wifi_config.example.h   ──复制并改名──▶   Core/Inc/wifi_config.h
```

然后填入自己的热点（ESP-12F **只支持 2.4 GHz**）：

```c
#define WIFI_CONFIG_SSID    "你的热点名"
#define WIFI_CONFIG_PASS    "你的密码"
```

`wifi_config.h` 已在 `.gitignore` 中，不会被提交。热点也可以在设备端 **WiFi 登录页**里改，改完不用重新编译。

### 3. 编译与下载

```bash
# 命令行编译
UV4.exe -b MDK-ARM\11.10_LVGL.uvprojx -j0 -o build.log
```

或直接用 Keil 打开 `MDK-ARM/11.10_LVGL.uvprojx`，`F7` 编译，`F8` 下载。

### 4. 云端联调（可选）

默认连接的 TCP 服务器地址在 `Core/Inc/wifi.h`：

```c
#define ServerIP    "129.211.183.41"
#define ServerPort  9010
```

在电脑上用网络调试助手开一个 **TCP Server** 监听同端口，即可看到上报帧、并下发远程指令。改了服务器地址需要重新编译。

---

## 关键设计

### 用事件组而不是队列做状态裁决

报警的本质是"当前是否存在越限条件"，而不是"发生过多少次越限"。队列会积压，积压的消息在条件消失后仍会被消费，无法表达"取消"；事件组置位/清零是幂等的，读到的永远是当前快照。

消音只是第 5 个事件位 `EVT_MUTED`，它只影响输出，**永远不可能把报警本身取消掉**。

### LVGL 只在唯一任务里碰

LVGL 不是线程安全的。所有 `lv_*` 调用集中在 `LCD_TASK`，其它任务要改界面只能置请求标志：

```c
/* 按键中断/其他任务里 */
g_key1_request = 1;

/* LCD_TASK 循环里消费 */
if (g_key1_request) { g_key1_request = 0; /* 真正的界面操作 */ }
```

同时 `lv_timer_handler()` 必须被周期调用，且 `Ui_Bind_Init()` 这类"创建对象"的初始化只能跑一次——放进循环会持续创建对象直到耗尽 LVGL 堆（默认 48 KB），触发 `LV_ASSERT` 死循环，表现为**白屏且串口静默**。

### 风扇三级调度

优先级从高到低：**云端强制 `FAN:n` > 手动滑块 > 自动（跟随状态）**。自动模式下 `NORMAL=0%`、`WARN=60%`、`ALARM=100%`，全部收敛到唯一出口 `FAN_Update()`，避免多处写 CCR 冲突。

PWM 频率取 1 kHz 而非 10 kHz：两线无刷小风扇在 10 kHz 下无法建立换向，表现为"只有 100% 占空比才转"。

### 上电不误报

判断依据是全局测量值，而首帧数据到达前它们是 0。若直接比较，`0 V` 会被判成"电压过低"并触发报警。解决方式是用初值为 0 的二值信号量把"数据就绪"显式表达出来，让判断任务阻塞等待，而不是靠延时猜时间。

---

## 云端协议

### 上报帧（2 s 一帧，CSV 单行）

```
Tem=25.02,Hum=59.92,V=0.70,A=0.008,Light=370,State=0\r\n
```

| 字段 | 含义 | 单位 |
| --- | --- | --- |
| `Tem` | 温度 | °C |
| `Hum` | 湿度 | %RH |
| `V` | 电压 | V |
| `A` | 电流 | A |
| `Light` | 光照（ADC 相对值） | — |
| `State` | 系统状态：0 正常 / 1 警告 / 2 报警 | — |

### 下行指令

| 指令 | 作用 |
| --- | --- |
| `MUTE` | 消音（置 `EVT_MUTED`） |
| `UNMUTE` | 取消消音 |
| `FAN:n` | 强制风扇转速 n%（0~100），覆盖本地逻辑 |
| `AUTO` | 解除强制，回到本地自动 / 手动 |
| `STAT` | 立刻回报一帧 |

---

## 常见问题

**编译报 `cannot open source input file "wifi_config.h"`**
没做第 2 步。把 `wifi_config.example.h` 复制成 `wifi_config.h` 即可。

**烧录后屏幕全白、串口也没有任何输出**
串口也静默说明程序没进主循环，通常是 LVGL 堆耗尽后卡在断言死循环。检查是否在循环里重复创建界面对象。

**风扇只有拉到 100% 才转**
PWM 频率过高。把定时器预分频调到使 PWM 落在 1 kHz 附近（本工程 `PSC=839`、`ARR=99`）。

**上电时蜂鸣器响一下、历史里多一条"电压恢复"**
首帧数据未到就参与判限。确认 `myBinarySem02` 初值为 0，且判断任务在开始前先 `osSemaphoreAcquire`。

**WiFi 一直连不上**
确认是 2.4 GHz 热点；用串口看 AT 状态机停在哪个状态码（`CWJAP` 失败多为密码错误或 5 GHz）。

**我用 ST-Link 调试时火焰检测不工作**
`PB3` 默认是 JTDO，需要把调试口配成 SWD（4 线）或关闭 JTAG，否则该引脚不能当普通 IO。

**编译出现两条 `Please define or replace the macro MY_DISP_HOR_RES` 警告**
这是 LVGL 移植层模板自带的提示，工程使用默认分辨率，不影响运行。若屏幕分辨率与默认值不同，在 `LVGL_CUT/porting/lv_port_disp.c` 顶部显式定义实际宽高即可，警告随之消失。

---

## 已知限制

- 报警历史保存在 RAM，掉电丢失；工程未启用 RTC，时间戳是"开机后运行时长"，不是真实时间。
- 除看门狗任务为 `AboveNormal` 外，其余任务同为 `Normal`，靠阻塞让出 CPU 保证实时性；任务负载加重时需要重新划分优先级。
- ESP-12F 走 AT 指令而非透传固件，吞吐和实时性受 AT 交互限制。
- 阈值与风扇模式未做掉电保存，重启恢复默认值。

---

## 致谢

界面基于 SquareLine Studio 导出，GUI 基于 [LVGL](https://lvgl.io)，RTOS 基于 [FreeRTOS](https://www.freertos.org)，外设驱动基于 ST `STM32F4xx_HAL_Driver`。
