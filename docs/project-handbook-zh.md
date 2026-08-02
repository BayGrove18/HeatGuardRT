# HeatGuardRT 项目手册与面试问答

> 用途：这是该项目唯一的面试主文档。先看第 1、2、3、6 节，再按第 7 节准备追问。
> 
> 事实边界：本文以当前 `main` 分支代码、Keil 编译记录和 `tests/` 为准。旧版用户手册是功能来源和调试背景，不作为当前实现的唯一证据。

## 1. 项目介绍

### 1.1 一句话介绍

**HeatGuardRT 是基于 STM32F103 和 FreeRTOS 的加热设备控制固件。**它把按键、门锁、温度采集、PWM 执行器、升级和低功耗统一到事件驱动的状态机中；异常时以硬件安全输出优先于界面和业务处理。

### 1.2 场景与目标

项目以小型加热设备为载体，模拟无人值守设备的三个核心问题：

1. 人机交互、传感器和定时控制并发发生时，不能让多个任务直接修改设备状态。
2. 门被打开、温度超限、传感器失效或调度异常时，必须优先切断加热和转盘输出。
3. 固件升级不能在收到一半数据时破坏当前运行程序，空闲时还要降低 CPU 和 LCD 的耗电。

### 1.3 技术栈

| 类别 | 当前使用内容 |
| --- | --- |
| 平台 | STM32F103C8，C，Keil MDK 5，STM32 Standard Peripheral Library |
| RTOS | FreeRTOS 9，抢占式调度，任务通知、队列、互斥锁、Tickless Idle |
| 输入与控制 | EXTI，DHT11，TIM2/TIM3 PWM，TIM4 倒计时中断，LCD，蜂鸣器 |
| 升级 | USART1，DMA1 Channel 5 环形接收，IDLE 中断，W25Q64，CRC16-CCITT，CRC32，BootManifest |
| 可靠性 | IWDG，安全输出钩子，队列溢出故障，栈/堆/队列水位诊断 |
| 低功耗 | RTC Alarm，STOP 模式，LSE 优先、LSI 回退，PA0 外部升级唤醒 |
| 测试 | Keil 全量构建，宿主机状态机和 Manifest 单元测试，GitHub Actions |

### 1.4 当前可宣称结果

- Keil 编译：`0 Error(s), 0 Warning(s)`；最新记录的程序规模为 Code `24644 B`、RO-data `6076 B`、RW-data `232 B`、ZI-data `13192 B`。
- 宿主机测试：状态机覆盖开门停机、超温锁定、传感器超时、倒计时结束、升级安全停机、队列溢出；Manifest 覆盖 CRC32 基准向量、构造和损坏拒绝。
- CI：GitHub Actions 在 push 和 PR 上运行上述宿主机测试。

### 1.5 不能过度宣称的内容

当前未接实物闭环测试，因此**不能**写成已经测得或认证完成：实际待机电流、STOP 唤醒时延、DHT11 信号质量、PWM 波形、门锁到输出的真实时延、端到端升级成功率、MISRA 合规或 50 ms 响应时间。面试时应主动说明代码已编译和单测，硬件验证计划见第 4.6 节。

---

## 2. 整体架构与运行流程

### 2.1 分层原则

```text
EXTI / DMA / TIM4 / DHT11
          |
          v
输入任务和 ISR：采样、投递事件、立即安全关断
          |
          v
control_task：唯一拥有 ControlSnapshot，执行状态转移
          |
          v
actuator.c：唯一写 PWM、定时器使能和门舵机输出
          |
          +--> LCD / 蜂鸣器 / LED
          +--> 升级安全交接
          +--> diagnostics
```

核心约束有两个：

- `ControlSnapshot` 只由 `control_task` 读写，其他任务和 ISR 不直接篡改业务状态。
- `actuator.c` 是唯一的执行器写入边界；其 `actuator_force_safe()` 可被门锁 ISR、看门狗、FreeRTOS 异常钩子调用，统一将加热和转盘 PWM 置零。

### 2.2 任务模型

| 任务 | 优先级 | 栈深度 | 触发方式 | 职责 |
| --- | ---: | ---: | --- | --- |
| `control_task` | 6 | 256 words | 有界事件队列或 250 ms 心跳超时 | 状态机、输出发布、诊断刷新、升级安全交接 |
| `start_task` | 5 | 128 words | PB10 EXTI 任务通知 | 按键消抖、开始请求、等待松键 |
| `mode_task` | 4 | 128 words | PB1 EXTI 任务通知 | 按键消抖、长短按判定、菜单事件 |
| `sensor_task` | 3 | 192 words | 1 s `vTaskDelayUntil` | 读取 DHT11，发送温度或传感器错误事件 |
| `supervisor_task` | 2 | 128 words | 250 ms 周期 | 观察控制心跳，健康时喂 IWDG，否则安全关断 |
| `upgrade_task` | 1 | 256 words | UART 字节队列 | 帧解析、Flash 暂存、CRC 校验、发布 Manifest |
| `boot_task` | 1 | 128 words | 上电一次 | 创建任务、投递初始门状态后自删除 |

为什么 `control_task` 最高：它负责处理门锁、故障和升级安全状态，不能被低优先级 Flash 读取或 UI 刷新拖住。为什么升级任务最低：升级可重传，而加热安全事件不能延后。

### 2.3 中断模型

| 中断 | 优先级 | ISR 内动作 | ISR 外动作 |
| --- | ---: | --- | --- |
| PB1 / EXTI1 | 5 | `vTaskNotifyGiveFromISR` 通知模式键任务 | 消抖、长短按识别、发送模式事件 |
| PB10 / EXTI10 | 4 | 通知开始键任务 | 消抖、发送开始事件 |
| PB12 / EXTI12 | 4 | 读取门状态；开门时立即 `actuator_force_safe()`；投递门状态事件 | 状态机更新门状态和界面 |
| PA0 / EXTI0 | 5 | 清中断标志 | 从 STOP 唤醒，为串口升级提供外部唤醒线 |
| TIM4 | 5 | 投递 `CONTROL_EVENT_TICK` | 状态机递减倒计时、处理完成状态 |
| USART1 IDLE | 3 | 计算 DMA 写指针，搬运环形缓冲区新字节 | `upgrade_task` 解析帧 |
| DMA1 Channel5 | 3 | 在半传输、全传输点搬运已接收字节 | `upgrade_task` 处理字节流 |
| RTC Alarm | 5 | 清 RTC/EXTI 标志 | Tickless STOP 的时间到唤醒 |

`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 为 3。调用 `xxxFromISR()` 的中断优先级数值必须不高于该阈值，即数值不小于 3；当前 UART/DMA 为 3，其他为 4 或 5，满足约束。

---

## 3. 代码实现与模块讲解

### 3.1 配置边界：`USER/heatguard_config.h`

产品可调参数集中在一个头文件，避免把业务阈值散落在驱动里：

- 事件队列深度 `12`，发送等待 `20 ms`。
- 按键消抖 `20 ms`，采样间隔 `10 ms`，长按门限 `1000 ms`。
- 温度采样周期 `1 s`，最大倒计时 `200 s`，每次调节 `20 s`。
- 超温阈值 `30 C`，连续传感器错误 `3` 次后加热故障。
- UART RX DMA 环 `256 B`、字节队列 `384 B`、升级最大帧负载 `240 B`。
- W25Q64 前 `0x7FF000` 字节是镜像暂存区，最后一个 `4 KiB` 扇区仅保存 Manifest。

面试重点：**配置属于产品策略，不属于驱动实现。**更换阈值或队列容量应评审配置，而不是修改状态机和外设驱动。

### 3.2 状态机：`USER/control.c`

`ControlSnapshot` 包含状态、功率档位、故障码、剩余时间、门状态、传感器有效性、加热允许位和温度。`control_dispatch()` 是唯一状态转移入口，返回“快照是否变化”，让调用方仅在必要时刷新输出和显示。

| 状态 | 允许的关键动作 | 退出或保护条件 |
| --- | --- | --- |
| `STANDBY` | 短按进入时间设置；满足条件后可开始加热 | 门打开保持不加热 |
| `TIME_SETTING` | 短按按 20 s 累加时间；长按切换功率设置 | 时间超过 200 s 回到 0 |
| `POWER_SETTING` | 短按循环低/中/高功率；长按回时间设置 | 不产生加热输出 |
| `HEATING` | TIM4 每秒递减倒计时 | 开门、超温、传感器超时、系统故障、升级请求都会撤销加热 |
| `COMPLETED` | 加热完成后的显示状态 | 开门回待机 |
| `FAULT` | 故障锁定，不允许开始加热 | 当前实现没有软件清故障事件，需复位或扩展专用恢复流程 |
| `UPDATE_PENDING` | 等待 `control_task` 先应用安全输出 | 调用 Boot handoff 后复位 |

加热许可不是只看按键，而是同时要求：**门已关、传感器已有有效样本、无故障、剩余时间非零**。这是把安全前提写成可复用谓词 `control_heating_permitted()`，避免在多个分支复制条件后产生遗漏。

### 3.3 事件队列和任务通知：`USER/main.c`

按键中断不延时、不刷 LCD、不控制复杂状态。ISR 只通知专属按键任务，按键任务使用 `vTaskDelay()` 完成消抖并发送业务事件。这样避免 ISR 中忙等和把业务逻辑分散在硬件回调。

所有业务事件进入 `control_queue`。若 `xQueueSend()` 或 `xQueueSendFromISR()` 失败：

1. 立即设置 `event_overflow`。
2. 立即调用 `actuator_force_safe()`，不等待队列恢复。
3. `control_task` 下一轮把它转成 `CONTROL_EVENT_SYSTEM_FAULT`，锁存为 `CONTROL_FAULT_EVENT_OVERFLOW`。

这是“先物理安全、后软件记录”的策略。队列满意味着系统已经不能保证事件完整性，继续加热没有安全依据。

### 3.4 执行器边界：`USER/actuator.c`

| 输出 | 外设 | 处理方式 |
| --- | --- | --- |
| 门舵机 | TIM2 CH2 / PA1 | 门状态变化时更新比较值，避免每次刷新都重复写 |
| 加热 PWM | TIM2 CH3 / PA2 | 低/中/高功率对应比较值 50/70/100 |
| 转盘 PWM | TIM3 CH3 / PB0 | 加热时设置比较值 4 |
| 倒计时 | TIM4 更新中断 | 仅加热时使能；安全关断时关闭中断源和计数器 |

`actuator_force_safe()` 同时执行三件事：加热 PWM 置 0、转盘 PWM 置 0、禁用 TIM4 更新中断并停止 TIM4。只调用 `TIM_Cmd(TIM4, DISABLE)` 不足以消除已挂起的中断源，这是旧版调试中出现过 HardFault/异常中断后的修正点。

### 3.5 温度与故障处理

`sensor_task` 每秒读取 DHT11：成功则发送温度事件并清连续失败计数；失败则发送传感器错误事件。状态机逻辑如下：

- 仅在 `HEATING` 中，温度大于等于 30 C 才锁存 `OVERTEMPERATURE` 并停止加热。
- 传感器连续错误达到 3 次，会标记传感器无效；若此时正在加热，锁存 `SENSOR_TIMEOUT`。
- 门锁开边沿不等待状态机：PB12 ISR 先清硬件输出，再异步报告门状态。

这样处理的原因是温度采样属于周期性检测，门锁属于异步安全联锁；门锁必须走更短的执行路径。

### 3.6 看门狗与 FreeRTOS 异常钩子：`USER/supervisor.c`、`USER/main.c`

IWDG 的喂狗权不交给任意周期任务，而由 `supervisor_task` 每 250 ms 检查 `control_heartbeat`：

- 心跳变化：说明 `control_task` 仍在运行，喂狗。
- 心跳不变：先执行安全关断，随后停止喂狗，等待 IWDG 复位。

`vApplicationStackOverflowHook`、`vApplicationMallocFailedHook` 和 `vApplicationAssert` 也都先关断输出，再关闭中断进入死循环，交给 IWDG 完成复位。启动阶段通过 RCC 复位标志记录电源、软件、外部和 IWDG 复位原因。

### 3.7 UART DMA 升级传输：`USER/upgrade_transport.c`、`USER/upgrade.c`

#### 接收路径

```text
USART1 RX
  -> DMA1 Channel5 循环写入 256 B rx_ring
  -> 半传输 / 全传输 / IDLE ISR 计算新数据边界
  -> FromISR 写入 384 B 字节队列
  -> upgrade_task 逐字节送入帧解析器
```

这样做的目的：DMA 负责搬运，ISR 只完成环形缓冲区排空，Flash 擦写和 CRC 计算在低优先级任务中执行；不会把长时间 SPI/Flash 操作放在中断上下文。

#### 应用协议

帧格式采用小端序：

```text
A5 5A | type:u8 | payload_length:u16 | payload | crc16_ccitt:u16
```

| 类型 | 含义 | 关键校验 |
| --- | --- | --- |
| `BEGIN` / `0x01` | 镜像大小、CRC32、版本 | 长度为 12；大小非零且未超过暂存区；先擦除暂存区 |
| `DATA` / `0x02` | 序号加镜像数据 | 序号严格等于 `expected_sequence`；不会越过镜像长度 |
| `FINISH` / `0x03` | 上传结束 | 接收字节数相等，流式 CRC32 正确，再从 W25Q64 分块回读重算 CRC32 |
| `ABORT` / `0x04` | 取消会话 | 清除内存会话和低功耗禁止标记 |

CRC16 解决单帧传输破坏，CRC32 解决整镜像一致性。两者不是安全签名，不能抵抗恶意篡改。

### 3.8 暂存、Manifest 和 Bootloader 交接

升级任务不会直接擦内部 App 区。成功 `FINISH` 后执行：

1. 从 W25Q64 暂存区分块读回镜像，重新计算 CRC32。
2. 构建 `BootManifest`：magic、格式版本、事务号、暂存偏移、镜像长度、镜像 CRC32、镜像版本和记录 CRC32。
3. 擦除 W25Q64 尾部专用扇区，写入并读回 Manifest，再校验记录 CRC32。
4. 通过回调向 `control_task` 投递升级请求；状态机先关闭加热和转盘，进入 `UPDATE_PENDING`。
5. `boot_handoff_commit_and_reset()` 将邮箱 magic 写入 `BKP_DR1` 后软件复位。

**边界必须说清楚：**当前仓库实现了应用侧暂存、校验和交接；真正消费邮箱、安装到内部 Flash、试运行确认或回滚由独立的 Bootloader 项目负责，不能把两个仓库的实现混为同一代码库。

### 3.9 SPI 共享与 W25Q64

LCD 和 W25Q64 共用 SPI1 的 PA5/PA6/PA7。`spi_bus.c` 用 FreeRTOS mutex 将二者串行化。W25Q64 读写和擦除会先取锁，LCD 刷新取锁失败则直接跳过一帧，保证升级和安全逻辑不会因为 UI 等待而阻塞。

W25Q64 的写路径按页边界拆分，单次页编程不跨 256 B 页；擦除轮询 BUSY 位并设 500 ms 超时，避免硬件异常时无限阻塞。

### 3.10 Tickless STOP 低功耗：`USER/power_manager.c`

只有以下状态允许 STOP：`STANDBY`、时间设置、功率设置、`COMPLETED`；`HEATING`、`FAULT`、`UPDATE_PENDING` 和升级会话中禁止 STOP。

```text
FreeRTOS 预计空闲时间
  -> 检查不少于 20 tick、当前状态允许、升级未激活
  -> RTC 设置 Alarm，关 SysTick，关 LCD 背光
  -> PWR_EnterSTOPMode(WFI)
  -> RTC / 门锁 / 按键 / PA0 唤醒
  -> 恢复系统时钟、背光和 SysTick
  -> 用 RTC 实际经过计数换算 tick，vTaskStepTick 补偿内核时间
```

RTC 优先使用 32.768 kHz LSE 并分频为 1024 Hz；无 LSE 时使用名义 40 kHz LSI 分频为 1 kHz。LSI 误差较大，量产时必须按实测校准或使用 LSE。PA0 为低电平升级唤醒线，主机应先拉低 PA0 再通过 UART 发送升级数据，避免设备处于 STOP 时丢失首帧。

### 3.11 诊断和测试

`g_app_diagnostics` 是调试器可见的全局诊断对象，记录：事件队列溢出次数、最小队列余量、控制任务最小剩余栈、剩余堆、状态、故障、温度、复位原因、STOP 次数和累计补偿 tick。

测试不依赖 STM32 外设：

```bash
gcc -std=c99 -Wall -Wextra -Werror -IUSER USER/control.c tests/control_test.c -o /tmp/control_test
/tmp/control_test

gcc -std=c99 -Wall -Wextra -Werror -IUSER USER/crc32.c USER/handoff_contract.c tests/handoff_test.c -o /tmp/handoff_test
/tmp/handoff_test
```

---

## 4. 关键设计取舍

### 4.1 为什么不用“多个任务各自改全局状态”

多个任务分别改门状态、加热标志、倒计时会产生竞态，例如温度任务判断允许加热后，门锁 ISR 已经发现门开。为避免每个字段都加锁，项目让事件串行进入最高优先级控制任务，形成单写者模型。门锁 ISR 是唯一例外：它只做“更保守”的硬件关断，不写业务快照。

### 4.2 为什么按键用任务通知，业务用队列

任务通知是一对一、无额外对象、适合“某个按键发生了”的轻量唤醒。业务事件需要承载类型和值，且多个生产者都要发送给同一个 `control_task`，因此使用结构化队列。不要把任务通知、二值信号量和队列混说成同一种东西。

### 4.3 为什么看门狗看控制心跳，不看“任务活着”

进程或任务仍在运行不代表控制逻辑在推进。`control_task` 可能卡在死锁、队列操作或驱动路径中。监督任务只在发现控制心跳递增后才喂 IWDG，能检测“任务没有崩溃但没有业务进展”的情况。

### 4.4 为什么升级前要暂存外部 Flash

串口传输是长过程，若直接擦内部 App Flash，传输失败或掉电会立即失去当前程序。先暂存到 W25Q64，再对完整镜像回读校验，最后通过 Manifest 交给 Bootloader，把“接收成功”和“允许覆盖 App”分成两个阶段。

### 4.5 为什么 Tickless 不能在加热和升级期间启用

加热需要及时处理倒计时、温度和安全事件；升级期间 W25Q64 正在擦写或会话仍在收帧。即使 STOP 能被中断唤醒，也不应在这些关键窗口引入时钟恢复和外设重启的不确定性。因此项目用状态和 `upgrade_active` 显式禁止。

### 4.6 当前应优先补的实板验证

1. 示波器测 PB12 门开沿到 PA2/PB0 PWM 归零的时延。
2. 测低/中/高档 PWM、舵机脉冲和 TIM4 倒计时是否满足实际设备要求。
3. 测 DHT11 失败、SPI BUSY 超时、队列满、控制任务停滞时的安全输出和 IWDG 复位。
4. 测 RTC Alarm、按键、门锁、PA0 对 STOP 的唤醒，记录电流、唤醒时间和 tick 漂移。
5. 做升级故障注入：传输中断、CRC 错误、W25Q64 擦写失败、Manifest 损坏、复位发生在安全交接前后。

---

## 5. 项目面试问答

### Q1：请在 90 秒内介绍这个项目。

答：这是我用 STM32F103 和 FreeRTOS 做的加热设备控制固件，重点不是做一个 LCD 演示，而是验证异步输入下的安全控制和可靠运行。我把按键、门锁、温度、定时器和升级都转成事件，由最高优先级的 `control_task` 独占状态机；执行器写入集中在 `actuator.c`。门锁打开时，PB12 外部中断会先直接把加热和转盘 PWM 置零，再把门状态投递给控制任务。温度超限、连续传感器失败和队列溢出都会进入故障锁定。升级采用 UART DMA 环形接收，先写 W25Q64，完成 CRC32 回读校验后写 Manifest，再由独立 Bootloader 安装。还加入了 IWDG 心跳监督和 RTC Tickless STOP。当前 Keil 已经 0 警告编译，状态机和 Manifest 有宿主机测试；真实电流和端到端升级还需要接板验证。

### Q2：为什么要让 `control_task` 独占 `ControlSnapshot`？

答：因为门锁、温度和倒计时可能在相近时间发生。如果各任务直接修改同一个全局状态，就需要为很多字段加锁，仍容易出现检查和修改之间被插入的问题。项目把业务事件串行投递给一个控制任务，状态转移只有一个入口，便于证明“加热许可”的条件完整。门锁 ISR 例外只做保守关断，不改业务快照，所以不会与状态机争夺所有权。

### Q3：开门为什么既在 ISR 里关 PWM，又要发事件给状态机？

答：这是两层责任。ISR 只执行最短的硬件安全动作，避免等待调度和队列；状态机负责记录门状态、停止加热状态、刷新界面和维持后续业务一致性。只在状态机中关 PWM，响应会受队列和任务调度影响；只在 ISR 里关 PWM，软件状态会过期。

### Q4：为什么按键不用在 ISR 里直接判断长短按？

答：长短按需要延时和反复采样，放 ISR 会延长关中断时间并阻塞其他中断。当前 ISR 只用 `vTaskNotifyGiveFromISR()` 唤醒专属任务；任务先延时 20 ms 消抖，再每 10 ms 读取电平并用 `xTaskGetTickCount()` 判断是否超过 1000 ms。这样不忙等，也不会把复杂交互逻辑放进 ISR。

### Q5：队列满了为什么不是简单丢弃一条按键消息？

答：对普通 UI 可以选择丢弃，但这里同一队列也承载门锁、温度和故障等安全相关事件。队列满说明软件已经无法保证事件顺序和完整性，因此项目立即关断执行器，并由控制任务把它锁存成 `EVENT_OVERFLOW` 故障。这个策略牺牲可用性，换取故障时的确定安全状态。

### Q6：FreeRTOS 的中断中为什么只能调用 `FromISR` API？

答：普通 API 可能阻塞、操作任务等待链表，调用语义面向线程上下文；ISR 不能阻塞。`xQueueSendFromISR()` 和 `vTaskNotifyGiveFromISR()` 使用适合中断的临界区路径，并通过 `higher_priority_task_woken` 告知是否需要在退出 ISR 时触发切换。当前中断优先级也遵守了 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 限制。

### Q7：加热状态的进入条件是什么？

答：不是收到开始按键就加热。`control_heating_permitted()` 同时检查门已关、传感器有有效样本、没有故障、倒计时不为零。进入加热后，开门、超温、连续三次传感器错误、队列溢出或升级请求都会撤销加热权限。这样把安全条件写成一个统一谓词，不会遗漏某个入口。

### Q8：IWDG 在项目里怎样避免“无脑喂狗”？

答：项目不让每个任务各自喂狗，而是由监督任务每 250 ms 检查控制心跳。控制任务只有在正常循环中才递增心跳；监督任务看到变化才重载 IWDG。若控制逻辑卡死，监督任务先安全关断输出，然后不再喂狗，最终由独立看门狗复位。这样检测的是业务进度，不只是 CPU 还在跑。

### Q9：USART DMA 环形接收配合 IDLE 中断是怎么工作的？

答：DMA1 Channel5 循环写入 256 B 环形数组。半传输、全传输和 IDLE 中断分别得到写指针边界，ISR 把“读指针到写指针”之间的新字节写入 FreeRTOS 队列。升级任务从队列取字节并喂给帧解析器。DMA 处理逐字节搬运，IDLE 解决最后不足半缓冲区的一帧，Flash 写入不在 ISR 中执行。

### Q10：升级为什么既有 CRC16 又有 CRC32？

答：CRC16-CCITT 对每一帧做快速校验，尽早发现帧头、长度和负载的传输损坏；CRC32 覆盖完整镜像。`FINISH` 时先比对流式 CRC32，再从 W25Q64 回读镜像重新计算 CRC32，防止“传输过程正确、外部 Flash 落盘错误”。Manifest 自身还有记录 CRC32，覆盖元数据损坏。

### Q11：当前升级方案和 Bootloader 的边界是什么？

答：当前 HeatGuardRT 负责应用侧接收、暂存、镜像回读校验、Manifest 发布和安全复位交接。复位后真正安装 App、检查启动向量、试运行确认和回滚属于独立 Bootloader 项目。面试中不能说本仓库已经实现了完整双版本回滚，除非同时明确引用另一个 Bootloader 仓库。

### Q12：LCD 和 W25Q64 共用 SPI 时如何处理？

答：它们共用 SPI1 的时钟和数据线，因此在 `spi_bus.c` 用 mutex 保护。W25Q64 擦写优先获得锁；界面刷新只等 10 ms，拿不到就跳过这一帧。这是有意识的降级：UI 丢一帧无害，升级 Flash 的命令序列被打断会破坏数据。

### Q13：Tickless STOP 的时间补偿如何实现？

答：进入 STOP 前，代码读取 RTC 计数、按预计空闲时间设置 RTC Alarm，然后停止 SysTick。唤醒后先恢复系统时钟和 SysTick，计算 RTC 实际经过的计数，换算为 FreeRTOS tick，调用 `vTaskStepTick()` 前移内核时钟。外部中断提前唤醒时按实际 RTC 时间补偿，而不是按预计空闲时间盲补。

### Q14：为什么不用 LSI 就宣称低功耗时序准确？

答：LSI 是内部低速 RC，频率误差受温度和电压影响，项目只把它作为没有 LSE 时的回退源，名义分频到 1 kHz。要把定时精度写入产品指标，必须用 32.768 kHz LSE，或者在实板上对 LSI 标定并定义误差预算。代码能编译不等于时序已被物理验证。

### Q15：如果发生 HardFault，你怎么定位？

答：先读取 CFSR/HFSR，区分总线、内存管理和用法错误；再从异常栈帧取 PC、LR、xPSR，用 map 文件定位出错指令，同时检查 MSP/PSP 是否越界。对 RTOS 项目还要查看每个任务的栈余量和 Heap。旧版项目中出现过 IAP 任务栈不足和定时器中断未完全禁用的问题；当前代码通过栈溢出钩子、安全关断和 `g_app_diagnostics.control_stack_min_words` 保留排查入口。

### Q16：`volatile` 在这个项目里该怎么用？

答：只用于确实会被异步上下文修改或硬件修改的对象，例如 `control_heartbeat`、DMA 相关的溢出标志和诊断全局变量。`volatile` 只保证每次从内存读写，不保证原子性、顺序或互斥；多步骤读改写仍要用临界区、队列或其他同步机制。不能把 `volatile` 当锁用。

### Q17：互斥锁、二值信号量、任务通知、队列分别怎么选？

答：互斥锁用于资源互斥且需要优先级继承，例如 SPI1；二值信号量适合事件同步但项目当前按键改用更轻量的任务通知；任务通知适合一对一唤醒，例如 PB1/PB10 对应专属任务；队列适合多生产者向单消费者传递带类型和值的 `ControlEvent`。选择依据是“资源互斥、事件同步还是消息传递”，不是哪个 API 更熟。

### Q18：当前验证覆盖什么，下一步怎么验证？

答：已完成 Keil 0 警告编译，且在宿主机覆盖了核心状态转移和 Manifest 完整性。未完成实板验证的部分包括门锁到 PWM 时延、DHT11、STOP 电流和唤醒、端到端升级。下一步会用示波器测安全时延和 PWM，用电流表测 STOP，用故障注入验证 CRC 错误、Flash 失败和升级中复位。

---

## 6. 对应八股速记

| 主题 | 一句话回答 | 结合项目的证据 |
| --- | --- | --- |
| 任务与中断 | 任务可阻塞和调度；ISR 必须短小，不能阻塞。 | 按键 ISR 只通知，消抖在任务中。 |
| 临界区与 mutex | 临界区保护短的共享读改写；mutex 保护长资源访问并支持优先级继承。 | 溢出标志读取用临界区，SPI1 用 mutex。 |
| 优先级反转 | 低优先级持锁、高优先级等待、中优先级抢占会造成反转；mutex 的优先级继承缓解它。 | LCD 与 W25Q64 的 SPI1 共享使用 mutex。 |
| 轮询与中断 | 高频或异步事件用中断/ DMA，周期采样可用任务延时。 | 门锁/按键用 EXTI，DHT11 每秒采样。 |
| DMA | DMA 在外设和内存间搬运，降低 CPU 逐字节中断负担；仍需处理缓冲区边界和完成通知。 | UART RX 用环形 DMA、HT/TC/IDLE 三种边界。 |
| 状态机 | 以状态、事件、动作和保护条件描述行为，避免散落的 flag 组合。 | `control_dispatch()` 是单一转移入口。 |
| 看门狗 | 看门狗应由能代表系统健康的路径喂，不应由任意定时器无条件喂。 | `supervisor_task` 观察控制心跳后才喂 IWDG。 |
| PWM | PWM 周期由 ARR/PSC 决定，占空比由 CCR 决定；同一计时器通道共享时基。 | TIM2 管门舵机和加热 PWM，TIM3 独立管理转盘。 |
| Tickless | 关闭周期 SysTick 后需另一个低功耗时基记录睡眠时长并补偿内核 tick。 | RTC Alarm + `vTaskStepTick()`。 |
| CRC | CRC 检测随机传输或存储损坏，不提供身份认证。 | 帧 CRC16、镜像 CRC32、Manifest CRC32。 |
| WFI / STOP | WFI 是等待中断指令；STOP 会停主时钟，唤醒后需恢复系统时钟和依赖时钟的外设。 | `PWR_EnterSTOPMode()` 后调用 `SystemClock_RestoreAfterStop()`。 |

---

## 7. 面试前检查清单

- 能不看代码画出“中断/任务 -> 事件队列 -> 状态机 -> 执行器”的数据流。
- 能写出 7 个状态、4 个故障来源和加热许可的 4 个条件。
- 能解释“门锁 ISR 先关 PWM”的原因，不把它说成普通状态机逻辑。
- 能解释 DMA 环形缓冲区中的读指针、写指针、IDLE、半传输和全传输各自作用。
- 能区分本项目应用侧 Manifest 交接和独立 Bootloader 的安装/回滚职责。
- 能解释 Tickless 的“预计空闲时间、RTC Alarm、实际时间、`vTaskStepTick()` 补偿”四步。
- 不宣称未实测的响应时间、升级成功率、功耗或 MISRA 认证；主动给出下一步实板验证方法。

## 8. 代码入口索引

| 主题 | 首读文件 |
| --- | --- |
| 初始化、任务创建、ISR | `USER/main.c` |
| 状态和故障规则 | `USER/control.c`、`USER/control.h` |
| PWM 和安全关断 | `USER/actuator.c` |
| 看门狗监督 | `USER/supervisor.c` |
| UART DMA 接收 | `USER/upgrade_transport.c` |
| 升级解析和整镜像校验 | `USER/upgrade.c` |
| Manifest 和备份寄存器交接 | `USER/handoff_contract.c`、`USER/boot_handoff.c` |
| STOP 和 RTC 补时 | `USER/power_manager.c` |
| 共享 SPI | `USER/spi_bus.c`、`USER/w25q64.c` |
| 诊断、单元测试 | `USER/diagnostics.c`、`tests/control_test.c`、`tests/handoff_test.c` |
