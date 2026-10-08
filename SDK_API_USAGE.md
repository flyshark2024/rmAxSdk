# rmAxSdk1 SDK API 使用说明

本文档面向 SDK 使用方，内容以当前公开头文件
`appSrc/rmAxSdkApi.h`、`appSrc/rmAxSdkTypes.h` 和对应实现为准。
`SDK使用记录.md` 中经过实际项目验证的集成经验也已整理到本文的“最佳实践与常见问题”章节。

所有公开 API 和数据类型均位于 `rmaxSdk` 命名空间。公开边界只使用标准 C++ 类型，
宿主无需使用特定 UI 框架。

---

## 1. SDK 工作方式

宿主通过两条通道与 SDK 交互：

- **宿主到 SDK/设备**：调用 `rcGet*`、`rcSet*`、`rcTask*` 等公开函数。
- **SDK/设备到宿主**：通过 `setMessageCallback()` 注册统一消息回调。

SDK 内部包含：

- 一个 SDK 工作线程；
- 一个独立的消息输出分发线程；
- 有界的输入、输出消息队列。

大多数 getter/setter 会把操作同步投递到 SDK 工作线程，因此函数返回时本次内存访问已经完成。
以 `task` 开头的接口只负责启动任务，设备通信、网络请求和升级过程仍然异步完成。

消息回调在 SDK 的**输出分发线程**执行，不在宿主主线程，也不在 SDK 工作线程。
回调必须线程安全、短小且非阻塞。不得在回调中直接操作要求固定线程访问的对象；
应先复制消息数据，再通过宿主自己的任务队列、事件循环或线程调度器转交目标线程。

---

## 2. 编译、链接与部署

### 2.1 产物和公开头文件

| 平台 | 动态库 | 链接文件 |
| --- | --- | --- |
| Windows | `rmAxSdk1.dll` | `rmAxSdk1.lib` |
| Android/Linux | `librmAxSdk1.so` | `librmAxSdk1.so` |

公开头文件：

```cpp
#include <rmAxSdkApi.h>
#include <rmAxSdkTypes.h>
```

`rmAxSdkApi.h` 已包含 `rmAxSdkTypes.h`，一般只包含前者即可。

### 2.2 构建要求

- C++20 或更高版本。
- 当前 Windows 预设使用 MSVC 2022、Ninja 和 x64。
- Windows 宿主必须使用与 SDK ABI 兼容的 MSVC 工具链和运行库。
- 使用预编译 SDK 时，宿主只需要标准 C++20，不需要提供额外的消息循环。
- 发布时应同时部署 SDK 分发包中附带的运行时依赖。

> **重要：不要混用 Debug/Release 或 `/MD`、`/MT` 运行库。**
>
> 宿主和 SDK 跨 DLL 传递 `std::string`、`std::vector`、`std::list` 和
> `std::shared_ptr`。ABI 或 CRT 不一致可能在 `initialize()`、参数析构或返回值析构时
> 表现为 heap 损坏。Windows 下建议 SDK 和宿主使用同一 VS 大版本、同一架构、
> 同一 Debug/Release 配置和 `/MD` 系列运行库。

### 2.3 宿主运行环境

SDK 不依赖宿主的消息循环。控制台程序、服务程序、游戏引擎或其他原生 C++ 应用都可以
直接调用 `initialize()`。宿主只需保证初始化期间传入的应用数据目录可读写，并在退出或
卸载动态库前调用 `shutdown()`。

---

## 3. 生命周期

除 `version()` 外，其他 API 都应在 `initialize()` 成功后调用。

### 3.1 固定调用顺序

```cpp
#include <rmAxSdkApi.h>

#include <iostream>

void onSdkMessage(const rmaxSdk::SdkMessage& message, void* userData)
{
    (void)message;
    (void)userData;
    // 只做快速解析和值拷贝，再投递到宿主业务/UI 线程。
}

int main()
{
    std::cout << "SDK version: " << rmaxSdk::version() << '\n';

    const auto result =
        rmaxSdk::initialize("C:/MyAppData/RadioMasterAX");
    if (result != rmaxSdk::InitResult::Success
        && result != rmaxSdk::InitResult::AlreadyInitialized) {
        return 1;
    }

    if (!rmaxSdk::setMessageCallback(&onSdkMessage, nullptr)) {
        rmaxSdk::shutdown();
        return 1;
    }

    rmaxSdk::taskReadConfigFromDevice();

    // 运行期调用 rcGet*/rcSet*/rcTask*。

    rmaxSdk::setMessageCallback(nullptr, nullptr);
    rmaxSdk::shutdown();
    return 0;
}
```

### 3.2 生命周期 API

```cpp
std::string_view rmaxSdk::version() noexcept;

rmaxSdk::InitResult rmaxSdk::initialize(
    std::string appDataPath) noexcept;

bool rmaxSdk::setMessageCallback(
    rmaxSdk::MessageCallback callback,
    void* userData = nullptr) noexcept;

void rmaxSdk::shutdown() noexcept;
```

`version()` 返回 SDK 内部静态构建日期/时间字符串的只读视图，不得释放其数据。

`appDataPath` 是 SDK 保存全局配置、模型、模板、升级缓存等数据的目录。SDK 会在其中维护
配置文件和子目录。删除整个目录会导致下次初始化时重建默认配置，应只在明确执行
“恢复本地出厂数据”时这样做。

`initialize()` 返回值：

| 返回值 | 含义 |
| --- | --- |
| `InitResult::Success` | 初始化成功 |
| `InitResult::AlreadyInitialized` | SDK 已经运行，本次没有重复创建运行时 |
| `InitResult::ThreadStartFailed` | SDK 工作线程或输出线程启动失败 |
| `InitResult::CommunicationInitFailed` | 内部通信/业务对象初始化失败 |

`initialize()` 幂等，`shutdown()` 可重复调用。完整关闭后允许再次初始化。

退出时建议：

1. 停止宿主继续发起 SDK 业务调用；
2. 清除消息回调；
3. 等待宿主自己排队的 SDK 消息处理完成；
4. 释放宿主持有的 SDK payload；
5. 调用 `shutdown()`；
6. 最后再卸载动态库。

---

## 4. 通用消息接口

### 4.1 消息结构

```cpp
struct SdkMessage {
    std::uint32_t msgId;
    std::shared_ptr<const void> payload;
};

using MessageCallback =
    void (*)(const SdkMessage& message, void* userData);
```

`msgId` 决定 `payload` 的真实类型。转换成错误类型属于未定义行为。
回调中应先检查 `msgId` 和 `payload`，再使用 `std::static_pointer_cast` 转换。

建议在回调中把 payload 内容复制为宿主自己的值类型，再投递到其他线程。尤其是
`RC_MSGID_ELRS_TELEMETRY`，应立即复制 `TelemetryDataUI` 快照，不要依赖底层对象长期不变。

### 4.2 输出消息表

| 消息 ID                         | payload 类型                | 使用方式                                                |
| ------------------------------- | --------------------------- | ------------------------------------------------------- |
| `RC_MSGID_LVTG_NOTIFY`          | 无                          | 低电压事件                                              |
| `RC_MSGID_PFC_NOTIFY`           | `std::vector<std::int32_t>` | 36 路预飞行检查状态                                     |
| `RC_MSGID_TIM_REPORT`           | `RcTimeReportUI`            | 定时器索引、方向、启动时间和当前秒数                    |
| `RC_MSGID_CNT_REPORT`           | `RcTimeReportUI`            | 计数器索引、方向、初始值和当前计数值                    |
| `RC_MSGID_TIM_NOTIFY`           | `RcTimeNotifyUI`            | 定时器索引、运行状态、提示代码和当前计数                |
| `RC_MSGID_TRIM_NOTIFY`          | `std::uint8_t`              | 微调变化事件                                            |
| `RC_MSGID_SRCINPUT_UPDATE`      | 未定义                      | 当前保留消息 ID                                         |
| `RC_MSGID_MIXOUT_UPDATE`        | 无                          | 输入、输出、微调和遥控器状态已刷新，重新调用 getter     |
| `RC_MSGID_CFGTODEV_END`         | 无                          | 当前配置已写入设备                                      |
| `RC_MSGID_GETDEVCFG_END`        | 无                          | 设备配置读取完成                                        |
| `RC_MSGID_RCTELEMETER_NOTIFY`   | 无                          | 遥测通知队列非空，调用`RCgetTelemetryItemNotify()` 排空 |
| `RC_MSGID_APPINFO_UPDATED`      | `AppUpdateInfoUI`           | APP 更新信息已刷新                                      |
| `RC_MSGID_APPDOWNLOAD_PROGRESS` | `AppUpdateInfoUI`           | APP 下载字节进度                                        |
| `RC_MSGID_APPDOWNLOAD_END`      | `AppUpdateInfoUI`           | APP 下载完成，读取`cacheAppFile`                        |
| `RC_MSGID_FWVERSION_NOT_MATCH`  | 无                          | 设备固件版本过低或不兼容                                |
| `RC_MSGID_FWINF_UPDATED`        | 无                          | 固件信息已刷新，调用`rcGetFwInf()`                      |
| `RC_MSGID_FWUPDATE_PROGRESS`    | `std::int32_t`              | `0..100` 为进度，`-1` 无可用固件，`-2` 更新失败         |
| `RC_MSGID_FWFILE_DOWNLOAD_END`  | 无                          | 固件缓存文件下载完成                                    |
| `RC_MSGID_ELRS_SET_ACK`         | `ElrsSetEndAckUI`           | ELRS 菜单设置回复                                       |
| `RC_MSGID_ELRS_STATUS`          | `ElrsStatusInfoUI`          | ELRS 状态回复                                           |
| `RC_MSGID_ELRS_TELEMETRY`       | `TelemetryDataUI`           | ELRS 位置、姿态或电池快照                               |
| `RC_MSGID_ELRS_DEVINF_UPDATE`   | `std::string`               | ELRS 设备名称                                           |
| `RC_MSGID_ELRS_MENUITEM_UPDATE` | `ElrsSetItemUI`             | 一个 ELRS 菜单项                                        |
| `RC_MSGID_CRSF_DATA_RECEIVED`   | `std::vector<std::uint8_t>` | 完整 CRSF 帧                                            |
| `RC_MSGID_TELEMETRY_UPDATED`    | 无                          | 遥测列表已变化，按需重新获取                            |
| `RC_MSGID_TELEMETRY_LOST`       | 无                          | 接收机遥测丢失                                          |
| `RC_MSGID_TELEMETRY_RECOVER`    | 无                          | 接收机遥测恢复                                          |

消息解析示例：

```cpp
void onSdkMessage(const rmaxSdk::SdkMessage& message, void*)
{
    if (message.msgId == rmaxSdk::RC_MSGID_FWUPDATE_PROGRESS
        && message.payload) {
        const auto progress =
            std::static_pointer_cast<const std::int32_t>(
                message.payload);
        const std::int32_t value = *progress;
        // 把 value 投递到宿主线程。
    }

    if (message.msgId == rmaxSdk::RC_MSGID_ELRS_STATUS
        && message.payload) {
        const auto status =
            std::static_pointer_cast<
                const rmaxSdk::ElrsStatusInfoUI>(
                message.payload);
        const rmaxSdk::ElrsStatusInfoUI snapshot = *status;
        // 把 snapshot 投递到宿主线程。
    }

    if ((message.msgId == rmaxSdk::RC_MSGID_TIM_REPORT
         || message.msgId == rmaxSdk::RC_MSGID_CNT_REPORT)
        && message.payload) {
        const auto report =
            std::static_pointer_cast<
                const rmaxSdk::RcTimeReportUI>(
                message.payload);
        const rmaxSdk::RcTimeReportUI snapshot = *report;
        // 把 snapshot 投递到宿主线程。
    }
}
```

### 4.3 定时器消息载荷

当前公开头文件提供两个独立于内部设备协议的载荷类型：

```cpp
struct RcTimeReportUI {
    int timeIdx = 0;
    int direction = 0;
    std::int32_t startTime = 0;
    std::int32_t timeCount = 0;
};

struct RcTimeNotifyUI {
    std::uint8_t timeIdx;
    std::uint8_t isRunning;
    std::uint8_t notifyCode;
    std::int32_t timeCount = 0;
};
```

`RcTimeReportUI`：

| 字段 | 含义 |
| --- | --- |
| `timeIdx` | 定时器或计数器的组内索引 |
| `direction` | `0` 显示剩余量，`1` 显示已用量 |
| `startTime` | 启动值；定时器中 `0` 表示正计时，大于 `0` 表示倒计时 |
| `timeCount` | 定时器消息中为当前秒数；计数器消息中为当前计数值 |

`RcTimeNotifyUI`：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `timeIdx` | `std::uint8_t` | 定时器索引 |
| `isRunning` | `std::uint8_t` | 是否正在运行，非零表示运行中 |
| `notifyCode` | `std::uint8_t` | 设备上报的提示代码 |
| `timeCount` | `std::int32_t` | 触发提示时的当前计数 |

使用者只需包含 `rmAxSdkApi.h` 或 `rmAxSdkTypes.h`，不要包含 `rcSystem` 下的内部头文件。
保存载荷时应复制公共结构，而不是只保存 `payload.get()` 返回的裸指针。

### 4.4 输入消息队列

```cpp
bool rmaxSdk::postMessage(
    rmaxSdk::SdkMessage message) noexcept;
```

该函数把消息异步放入 SDK 输入队列。SDK 未初始化、正在关闭、队列已满或投递失败时返回
`false`。当前公开 API 没有定义具体输入消息业务协议；返回 `true` 只表示消息进入队列，
不表示存在对应业务处理器。普通 SDK 功能应优先调用明确的公开 API。

输入和输出队列当前上限均为 4096。宿主不应依赖该具体数值，应把 `false` 当作明确失败处理。

---

## 5. 配置读取、修改和保存

### 5.1 三种数据位置

配置操作涉及三个位置：

1. **SDK 当前工作配置**：`rcGet*`/`rcSet*` 访问的运行中配置；
2. **本地模型文件**：位于 `appDataPath` 的模型 JSON；
3. **遥控器设备配置**：设备实际使用的数据。

`rcSet*` 通常只修改 SDK 当前工作配置或发送实时预览，不等于已经保存到目标模型或设备。

```cpp
void rmaxSdk::taskReadConfigFromDevice() noexcept;
void rmaxSdk::taskSaveConfigToDevice() noexcept;
void rmaxSdk::rcLoadModelFile(std::string modelFile) noexcept;
void rmaxSdk::rcSaveCurrentCfgToModel(
    rmaxSdk::RcModelDataUI rcModel) noexcept;
```

- `taskReadConfigFromDevice()`：异步读取设备配置。完成时发布
  `RC_MSGID_GETDEVCFG_END`，并把读取结果写入当前模型文件。
- `taskSaveConfigToDevice()`：异步写入设备。完成时发布
  `RC_MSGID_CFGTODEV_END`，并保存当前模型文件。
- `rcLoadModelFile()`：从指定模型文件加载 SDK 当前工作配置。
- `rcSaveCurrentCfgToModel()`：把当前工作配置保存到参数指定的模型文件，可用于
  “另存到某模型”。

典型编辑流程：

```cpp
auto mix = rmaxSdk::rcGetMixCfgData(0, 0);
mix.weightUp = 80;
mix.weightDown = 80;
rmaxSdk::rcSetMixCfgData(mix);       // 修改工作配置/预览

rmaxSdk::taskSaveConfigToDevice();   // 写入设备；完成后当前模型也会落盘
```

如果要把当前配置保存到另一个模型：

```cpp
auto target = rmaxSdk::rcCreateModelCfg();
target.modelName = "My model";
rmaxSdk::rcSaveCurrentCfgToModel(target);
```

初始化后本地模型可以读取，但设备数据应等到 `RC_MSGID_GETDEVCFG_END` 后再视为就绪。
设备握手期间任务可能延迟，不要用固定 sleep 代替完成消息。

---

## 6. 输入、输出、微调和基础状态

```cpp
std::vector<rmaxSdk::RcSrcCfgUI>
rmaxSdk::rcGetSrcCfgList() noexcept;

std::vector<rmaxSdk::RcChOutCfgUI>
rmaxSdk::rcGetChOutList() noexcept;

rmaxSdk::RcTrimmingTopKeyUI
rmaxSdk::rcGetTrimming() noexcept;

rmaxSdk::RadioStatusUI
rmaxSdk::rcGetRadioStatus() noexcept;

void rmaxSdk::rcSetChOutCfgData(
    rmaxSdk::RcChOutCfgUI chOutCfg) noexcept;
```

- `rcGetSrcCfgList()` 返回输入源名称、类型、原始值、逻辑值、输出值和权重。
- `rcGetChOutList()` 返回输出通道配置及实时值。
- `rcGetTrimming()` 返回四个微调值。
- `rcGetRadioStatus()` 返回 RF 链路、遥控器/接收机电压、顶部按键和 ADC 校准状态。
- `rcSetChOutCfgData()` 更新一个输出通道配置。

不要用无间隔循环轮询实时 getter。收到 `RC_MSGID_MIXOUT_UPDATE` 后，可按一个节奏重新获取：

- `rcGetSrcCfgList()`；
- `rcGetChOutList()`；
- `rcGetTrimming()`；
- `rcGetRadioStatus()`。

设备原始数据更新约为 40 ms 一次。UI 仍应根据页面可见性和渲染能力合并更新。

修改输出通道时应先读取、只改目标字段、再写回，避免用未初始化字段覆盖其他配置：

```cpp
auto channels = rmaxSdk::rcGetChOutList();
if (!channels.empty()) {
    auto channel = channels.front();
    channel.weight = 80;
    channel.min = -100;
    channel.max = 100;
    rmaxSdk::rcSetChOutCfgData(channel);
}
```

---

## 7. 混控、曲线、DR、定时器和系统配置

### 7.1 混控

```cpp
rmaxSdk::RcMixCfgDataUI rmaxSdk::rcGetMixCfgData(
    int chIdx,
    int mixIdx) noexcept;

void rmaxSdk::rcSetMixCfgData(
    rmaxSdk::RcMixCfgDataUI mixData) noexcept;

rmaxSdk::RcMixCfgDataUI rmaxSdk::rcAddMixCfgData(
    int chIdx,
    int srcIdx) noexcept;

int rmaxSdk::rcDelMixCfgData(
    int chIdx,
    int srcIdx) noexcept;
```

- `chIdx` 是输出通道索引。
- `mixIdx` 是通道内混控项索引。
- `srcIdx` 是输入源索引。
- 删除键是 `(chIdx, srcIdx)`，不是 `(chIdx, mixIdx)`。
- `rcDelMixCfgData()` 成功时返回剩余混控数量，调用失败返回 `-1`。

```cpp
auto added = rmaxSdk::rcAddMixCfgData(0, 1);
added.weightUp = 100;
added.weightDown = 100;
rmaxSdk::rcSetMixCfgData(added);

const int remaining =
    rmaxSdk::rcDelMixCfgData(0, added.srcIdx);
```

### 7.2 曲线

```cpp
std::vector<rmaxSdk::RcCurveCfgDataUI>
rmaxSdk::rcGetCurveList(int idx = -1) noexcept;

int rmaxSdk::rcSetCurveCfg(
    const rmaxSdk::RcCurveCfgDataUI& curveCfg) noexcept;

double rmaxSdk::rcCurveApply(
    int curveIdx,
    double rawVal) noexcept;
```

- `rcGetCurveList(-1)` 返回全部曲线，传入具体索引只返回该曲线。
- 当前模型默认有 18 条曲线，调用方仍应以实际返回列表为准。
- `rcSetCurveCfg()` 成功返回 `0`，无效配置或调用失败返回 `-1`。
- `rcCurveApply()` 失败或索引无效时原样返回 `rawVal`。
- `curveIdx == 0xff` 表示不应用曲线。
- `xVal`、`yVal` 是固定长度 12 的数组，`ptNum` 不得超过数组容量。

```cpp
auto curves = rmaxSdk::rcGetCurveList(0);
if (!curves.empty()) {
    auto curve = curves.front();
    curve.cType = 4;
    curve.ptNum = 3;
    curve.xVal[0] = -100;
    curve.xVal[1] = 0;
    curve.xVal[2] = 100;
    curve.yVal[0] = -100;
    curve.yVal[1] = 0;
    curve.yVal[2] = 100;
    rmaxSdk::rcSetCurveCfg(curve);
}
```

### 7.3 DR

```cpp
rmaxSdk::RcChCfgDrUI rmaxSdk::rcGetDrData(
    int chIdx,
    int idx) noexcept;

int rmaxSdk::rcSetDrData(
    const rmaxSdk::RcChCfgDrUI& drCfg) noexcept;

int rmaxSdk::rcDelDrData(
    const rmaxSdk::RcChCfgDrUI& drCfg) noexcept;
```

- 每个通道当前有 3 个 DR 槽位。
- `rcGetDrData(chIdx, -1)` 查找一个空闲槽位。
- `rcSetDrData()` 成功时返回该通道已使用的 DR 数量，调用失败返回 `-1`。
- 当前实现中的 `rcDelDrData()` 只返回 `0`，尚未实际清除 DR 槽位；调用方不要把
  返回 `0` 当作删除已经持久生效。

通道锁定字段位于 `RcChOutCfgUI`，通过 `rcSetChOutCfgData()` 更新。

### 7.4 定时器和计数器

```cpp
rmaxSdk::RcTimeCfgDataUI rmaxSdk::rcGetTimerCfgData(
    int timerIdx,
    int isCounter) noexcept;

void rmaxSdk::rcSetTimerCfgData(
    const rmaxSdk::RcTimeCfgDataUI& timerCfg,
    int isCounter) noexcept;

void rmaxSdk::rcResetTimer(int timerIdx) noexcept;
```

- `isCounter == 0` 操作定时器数组。
- `isCounter != 0` 操作计数器数组。
- 当前每组索引为 `0..2`，调用方必须先校验索引。
- `timerCfg.timeIdx` 是写入时使用的组内索引。
- `timerCfg.direction` 中 `1` 表示正计时，`0` 表示倒计时。
- `timerCfg.startTime == 0` 表示正计时，大于 `0` 表示倒计时周期；计数器模式下
  该字段表示计数器最大值，不能为 `0`。
- `swCmpType`、`rstSwCmpType` 和 `cmpType` 的比较值均为：
  `0` 禁用、`1` 大于、`2` 等于、`3` 小于。
- `outputMin`、`outputMax` 的范围为 `-100..100`。
- 定时器设置会发送实时预览；计数器设置更新当前工作配置。
- `rcResetTimer()` 发送设备复位命令，参数使用设备定时器编号。
- `RC_MSGID_TIM_REPORT` 和 `RC_MSGID_CNT_REPORT` 的 payload 均为
  `RcTimeReportUI`。其中 `direction == 0` 表示显示剩余量，`direction == 1`
  表示显示已用量；`timeCount` 对定时器表示当前秒数，对计数器表示当前计数值。
- `RC_MSGID_TIM_NOTIFY` 的 payload 为 `RcTimeNotifyUI`，包含索引、运行状态、
  通知码和当前计数。前三个字段是 `std::uint8_t`，打印数值时应先转换为整数，
  避免被流输出当作字符。

```cpp
auto timer = rmaxSdk::rcGetTimerCfgData(0, 0);
timer.timeIdx = 0;
timer.startTime = 60;
timer.beepTimeSec = 10;
rmaxSdk::rcSetTimerCfgData(timer, 0);
```

消息处理示例：

```cpp
if (message.msgId == rmaxSdk::RC_MSGID_TIM_NOTIFY
    && message.payload) {
    const auto notify =
        std::static_pointer_cast<
            const rmaxSdk::RcTimeNotifyUI>(
            message.payload);

    const int timerIndex =
        static_cast<int>(notify->timeIdx);
    const bool running = notify->isRunning != 0;
    const int code =
        static_cast<int>(notify->notifyCode);
    const std::int32_t count = notify->timeCount;
}
```

### 7.5 系统配置

```cpp
rmaxSdk::RcSysCfgDataUI
rmaxSdk::rcGetRcSysConfig() noexcept;

void rmaxSdk::rcSetRcSysConfig(
    const rmaxSdk::RcSysCfgDataUI& rcSysCfg) noexcept;
```

当前实现实际读写：

- `alramVtg`；
- `rfModuleMode`；
- `stickMode`；
- `preFlightCheckChannel[36]`。

`RcSysCfgDataUI` 中其他字段目前没有在 getter/setter 中完整映射。调用方不要假设
`fsMode`、`fsChVal`、`joytickBeep`、`modelMatchId`、`crsfBaudRate` 已通过这两个 API
持久读写。

模型匹配值应通过 `RcModelDataUI::modelMatch` 和模型管理 API 更新。

---

## 8. 设备控制

```cpp
void rmaxSdk::rcResetDefault() noexcept;
void rmaxSdk::rcSkipPreFlightCheck() noexcept;
void rmaxSdk::rcSetAdcCalibration(int isStart) noexcept;
void rmaxSdk::rcSetUsbToVcp(std::uint8_t mode) noexcept;
void rmaxSdk::rcRebootRFModule() noexcept;
```

| API | 用途 |
| --- | --- |
| `rcResetDefault()` | 恢复默认配置并触发重新读取 |
| `rcSkipPreFlightCheck()` | 跳过当前预飞行检查 |
| `rcSetAdcCalibration(isStart)` | 非零开始 ADC 校准，`0` 停止 |
| `rcSetUsbToVcp(mode)` | 设置 USB 虚拟串口透传模式，值由设备协议定义 |
| `rcRebootRFModule()` | 请求重启 RF 模块 |

ADC 当前状态可从 `rcGetRadioStatus().adcSetState` 获取。

---

## 9. 模型与模板

```cpp
std::list<rmaxSdk::RcModelDataUI>
rmaxSdk::rcGetRcModelList() noexcept;

rmaxSdk::RcModelDataUI
rmaxSdk::rcCreateModelCfg() noexcept;

rmaxSdk::RcModelDataUI rmaxSdk::rcSetModelCfg(
    rmaxSdk::RcModelDataUI rcModel) noexcept;

void rmaxSdk::rcSaveCurrentCfgToModel(
    rmaxSdk::RcModelDataUI rcModel) noexcept;

void rmaxSdk::rcLoadModelFile(
    std::string modelFile) noexcept;

void rmaxSdk::rcExportModelCfg(
    std::string modelName,
    std::string modelFile) noexcept;

int rmaxSdk::rcImportModelCfg(
    std::string importFile,
    std::string modelFile) noexcept;

void rmaxSdk::rcDeleteModel(
    std::string filePath) noexcept;

int rmaxSdk::rcGetCurrentModelCfgIndex() noexcept;

std::list<rmaxSdk::RcModelDataUI>
rmaxSdk::rcGetTemplateList() noexcept;

void rmaxSdk::rcModelToTemplate(
    std::string modelFile,
    std::string tempName) noexcept;
```

`RcModelDataUI` 包含 `modelName`、`modelImg`、`filePath`、`modelIdx` 和 `modelMatch`。

| API | 当前行为 |
| --- | --- |
| `rcGetRcModelList()` | 扫描模型目录并返回模型列表 |
| `rcCreateModelCfg()` | 生成新模型元数据和目标路径；**不会立即创建文件** |
| `rcSetModelCfg()` | 加载目标文件、更新元数据并保存；用于已有模型元数据修改 |
| `rcSaveCurrentCfgToModel()` | 把当前工作配置和传入元数据写到目标模型 |
| `rcLoadModelFile()` | 把目标模型加载为当前工作配置 |
| `rcExportModelCfg(source, destination)` | 复制模型文件到导出路径 |
| `rcImportModelCfg(source, target)` | 把来源配置导入目标模型，成功返回 `0` |
| `rcDeleteModel()` | 删除指定模型文件 |
| `rcGetCurrentModelCfgIndex()` | 返回当前模型索引，失败返回 `-1` |
| `rcGetTemplateList()` | 返回模板列表 |
| `rcModelToTemplate()` | 把模型保存为指定名称的模板 |

新建并保存当前配置：

```cpp
auto model = rmaxSdk::rcCreateModelCfg();
model.modelName = "Plane";
model.modelImg = "qrc:/image/plane.png";
model.modelMatch = 1;

rmaxSdk::rcSaveCurrentCfgToModel(model);
```

只修改已有模型名称或图片时使用 `rcSetModelCfg()`。不要把 `rcSetModelCfg()` 当成
“保存当前工作配置”的替代品。

---

## 10. 遥测与通知

### 10.1 遥测列表

```cpp
std::list<rmaxSdk::TelemetryNotifyUI>
rmaxSdk::rcGetTelemetryList(int uiOnly) noexcept;
```

- `uiOnly == 0`：返回全部遥测项目。
- `uiOnly != 0`：只返回 `showUI != 0` 的项目。
- `RC_MSGID_TELEMETRY_UPDATED` 可能高频出现，不应每条消息都刷新完整 UI。

实际项目建议：

- 后台保存最新状态；
- 对 UI 列表刷新做节流，例如 500 ms 到 1.5 s；
- 页面不可见时停止 UI 刷新；
- 显式用户请求可绕过节流立即获取。

### 10.2 通知配置

```cpp
rmaxSdk::TelemetryNotifyUI
rmaxSdk::rcGetNotifyCfgItem(
    std::uint64_t hashValue,
    int index) noexcept;

void rmaxSdk::rcSetNotifyCfgItem(
    std::uint64_t hashValue,
    const rmaxSdk::TelemetryNotifyUI& notifyCfg,
    int index) noexcept;

void rmaxSdk::rcDelNotifyItem(int index) noexcept;

std::list<rmaxSdk::TelemetryNotifyUI>
rmaxSdk::rcGetNotifyCfgList() noexcept;
```

`rcSetNotifyCfgItem()` 的当前判定规则：

- `hashValue == 0`：新增通知，`index` 不参与定位；
- `hashValue != 0`：按 `index` 修改已有通知，调用方必须保证索引有效。

新增时可传 `index == -1` 表达调用方意图，但真正决定新增的是第一个参数
`hashValue == 0`。

`hashValue` 是 `std::uint64_t`。业务层或脚本绑定不得用 32 位 `int` 保存，否则会截断。

### 10.3 触发通知队列

```cpp
int rmaxSdk::RCgetTelemetryItemNotify(
    rmaxSdk::TelemetryNotifyUI& notifyItem) noexcept;
```

`RC_MSGID_RCTELEMETER_NOTIFY` 只表示通知队列非空，不携带通知 payload。
收到消息后应循环取出：

```cpp
rmaxSdk::TelemetryNotifyUI item{};
while (rmaxSdk::RCgetTelemetryItemNotify(item) != -1) {
    // 复制 item，执行语音、弹窗或日志。
}
```

成功时返回出队前的队列长度；队列为空或调用失败时返回 `-1`。

### 10.4 遥测公共类型的选择

`rmAxSdkTypes.h` 同时声明了 `TelemetryNotifyUI` 和 `TelemetryItemUI`。当前公开 API
`rcGetTelemetryList()`、`rcGetNotifyCfgItem()`、`rcSetNotifyCfgItem()`、
`rcGetNotifyCfgList()` 和 `RCgetTelemetryItemNotify()` 均使用
`TelemetryNotifyUI`。

`TelemetryItemUI` 当前没有对应的公开 API 参数、返回值或消息 payload。使用者不应把它
替代为上述接口的参数类型；除非后续公开 API 明确采用，否则仅把它视为预留公共数据结构。

---

## 11. ELRS 与 CRSF

```cpp
void rmaxSdk::rcTaskRequestElrsMenuList(
    int reqSetDevID) noexcept;

void rmaxSdk::rcElrsSendMenuCmd(
    int idx,
    int val) noexcept;

int rmaxSdk::rcRequestElrsStatus() noexcept;

void rmaxSdk::rcSendCrsfRawData(
    std::vector<std::uint8_t> data) noexcept;
```

### 11.1 菜单

`rcTaskRequestElrsMenuList(reqSetDevID)` 异步请求指定 ELRS 设备的参数菜单。
常用发射机地址为 `0xEE`，调用方应以实际 CRSF 设备地址为准。

响应顺序通常为：

1. `RC_MSGID_ELRS_DEVINF_UPDATE`：设备名称；
2. 多个 `RC_MSGID_ELRS_MENUITEM_UPDATE`：每次一个 `ElrsSetItemUI`；
3. 调用 `rcElrsSendMenuCmd(idx, val)` 修改；
4. `RC_MSGID_ELRS_SET_ACK`：`ElrsSetEndAckUI`。

离开 ELRS 页面后应停止宿主自己的重复刷新/请求。菜单请求仍在进行时继续发起请求会被内部状态忽略，
频繁重复拉取也会造成页面卡顿。

### 11.2 ELRS 状态

`rcRequestElrsStatus()` 发送状态请求：

- 返回 `0`：请求已发送；
- 返回 `-1`：ELRS 菜单请求正在进行，或 SDK 不可用/调用失败。

成功发送不代表已经收到响应。响应通过 `RC_MSGID_ELRS_STATUS` 返回，
payload 为 `ElrsStatusInfoUI`：

- `badPkt`：坏包数；
- `goodPkt`：好包数；
- `modelMismatch`：模型匹配状态，非零表示不匹配。

```cpp
if (rmaxSdk::rcRequestElrsStatus() != 0) {
    // 当前忙或 SDK 不可用，稍后重试。
}
```

### 11.3 CRSF 原始数据

`rcSendCrsfRawData()` 发送调用方提供的原始字节。空数组不会发送。

`RC_MSGID_CRSF_DATA_RECEIVED` 返回完整帧，格式为：

```text
ADDR LEN TYPE DATA CRC
```

总字节数应为 `LEN + 2`，CRC8-D5 覆盖 `TYPE + DATA`。

---

## 12. 固件和 APP 更新

### 12.1 设备固件

```cpp
rmaxSdk::FirmwareInfUI
rmaxSdk::rcGetFwInf() noexcept;

void rmaxSdk::rcTaskRequestDeviceInf() noexcept;
void rmaxSdk::rcTaskStartUpdateFw() noexcept;
```

流程：

1. 调用 `rcTaskRequestDeviceInf()`；
2. 收到 `RC_MSGID_FWINF_UPDATED`；
3. 调用 `rcGetFwInf()` 获取设备和缓存固件信息；
4. 按业务条件调用 `rcTaskStartUpdateFw()`；
5. 处理 `RC_MSGID_FWUPDATE_PROGRESS`。

`FirmwareInfUI` 包含系统 ID、设备 ID、飞行时间、构建时间、固件/硬件版本、
设备描述和缓存固件版本文本。

固件更新进度值：

- `0..100`：进度；
- `-1`：没有可用固件；
- `-2`：更新失败。

### 12.2 APP 更新

```cpp
rmaxSdk::AppUpdateInfoUI
rmaxSdk::rcGetAppUpdateInfo() noexcept;

void rmaxSdk::rcTaskGetAppInfoFromWeb() noexcept;

int rmaxSdk::rcTaskUpdateApp(
    std::string url) noexcept;

void rmaxSdk::rcAbortUpdateApp() noexcept;
```

流程：

1. 调用 `rcTaskGetAppInfoFromWeb()`；
2. 收到 `RC_MSGID_APPINFO_UPDATED`，读取 payload 或 `rcGetAppUpdateInfo()`；
3. 调用 `rcTaskUpdateApp(info.apkUrl)`；
4. 处理 `RC_MSGID_APPDOWNLOAD_PROGRESS`；
5. 收到 `RC_MSGID_APPDOWNLOAD_END` 后读取 `cacheAppFile` 并由宿主执行安装。

`rcTaskUpdateApp()`：

- 返回 `0`：下载任务成功启动；
- 返回 `-1`：已有下载任务、SDK 不可用或调用失败。

`rcAbortUpdateApp()` 中止正在运行的下载。

---

## 13. 返回值与失败约定

所有公开 API 都是 `noexcept`，异常不会越过 DLL/SO 边界。

| API 类型 | SDK 不可用或调用失败时 |
| --- | --- |
| `std::vector`/`std::list` getter | 空容器 |
| 结构体 getter | 值初始化的默认结构 |
| `rcGetCurrentModelCfgIndex()` | `-1` |
| `rcDelMixCfgData()` | `-1` |
| `rcSetDrData()`/`rcDelDrData()` | `-1` |
| `rcSetCurveCfg()` | `-1` |
| `rcImportModelCfg()` | `-1` |
| `rcTaskUpdateApp()` | `-1` |
| `rcRequestElrsStatus()` | `-1` |
| `RCgetTelemetryItemNotify()` | `-1` |
| `rcCurveApply()` | 原样返回 `rawVal` |
| `void` 设置/任务 API | 不执行，无法通过返回值获知失败 |

空容器或默认结构既可能表示“确实没有数据”，也可能表示失败。调用方应结合生命周期状态、
设备完成消息和自己的业务状态判断。

索引、路径、URL 和协议参数由调用方校验。部分内部实现直接按索引访问数组，越界参数不是
可恢复的业务错误。

---

## 14. 最佳实践与常见问题

### 14.1 设备就绪前不要把 getter 当作设备真值

`initialize()` 后本地模型可立即读取，但设备配置应等待
`RC_MSGID_GETDEVCFG_END`。启动握手期间任务可能排队或延迟。

### 14.2 回调不直接操作 UI

回调在独立输出线程。正确做法是：

1. 校验消息 ID 和 payload；
2. 立即复制成宿主值类型；
3. 通过线程安全队列、事件循环或任务调度器投递；
4. 立即返回。

不要在回调中执行网络访问、磁盘 IO、等待锁或长时间计算。

### 14.3 高频消息必须合并或节流

- `RC_MSGID_MIXOUT_UPDATE` 可约 40 ms 一次；
- `RC_MSGID_TELEMETRY_UPDATED` 可能按遥测帧持续出现。

不要为每条消息重建整个 UI 数据模型。使用最新值覆盖、定时批量刷新，并在页面不可见时暂停 UI 更新。

### 14.4 保存语义要区分

- `rcSet*`：改当前工作配置/实时预览；
- `rcSaveCurrentCfgToModel(model)`：保存到指定本地模型；
- `taskSaveConfigToDevice()`：异步写设备，完成后当前模型也会保存；
- `rcSetModelCfg(model)`：主要用于修改已有模型元数据。

### 14.5 大整数跨绑定层不用 32 位 int

`TelemetryNotifyUI::hashValue` 是 64 位。宿主绑定层应使用 64 位无符号整数或字符串，
不要存入 32 位 `int`。如果目标脚本语言不能精确表示全部 64 位整数，优先使用字符串。

### 14.6 只使用公开 payload 类型

定时器、计数器和定时器提示消息已经提供 `RcTimeReportUI`、`RcTimeNotifyUI`。
宿主只应包含公开头文件并转换为这些公共类型，不要引用或复制 `rcSystem` 中的内部协议结构。

### 14.7 动态库卸载前释放跨边界对象

确保回调已清除、SDK 已关闭、宿主持有的 `SdkMessage`、payload、SDK 返回容器和字符串
已经销毁，再卸载动态库。

---

## 15. CMake 接入

### 15.1 作为源码子目录

```cmake
cmake_minimum_required(VERSION 3.16)
project(SdkConsumer LANGUAGES CXX)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/rmAxSdk1)

add_executable(sdk_consumer main.cpp)
target_compile_features(sdk_consumer PRIVATE cxx_std_20)
target_link_libraries(sdk_consumer PRIVATE rmAxSdk1::rmAxSdk1)
```

### 15.2 使用 Windows 二进制库

```cmake
add_executable(sdk_consumer main.cpp)
target_compile_features(sdk_consumer PRIVATE cxx_std_20)
target_include_directories(sdk_consumer PRIVATE
    "${RMAXSDK_ROOT}/include/rmAxSdk1"
)
target_link_libraries(sdk_consumer PRIVATE
    "${RMAXSDK_ROOT}/lib/rmAxSdk1.lib"
)
```

发布目录至少需要：

- `rmAxSdk1.dll`；
- SDK 分发包附带的运行时依赖；
- 目标机器缺少时所需的 MSVC 运行库。

运行时依赖的版本和架构必须与 `rmAxSdk1.dll` 一致。

### 15.3 Android arm64 示例

```cmake
set(RMAXSDK_SO
    ${CMAKE_SOURCE_DIR}/third_party/rmAxSdk1/lib/android_arm64_v8a/librmAxSdk1.so)

target_link_libraries(RadioMasterAX PRIVATE
    ${RMAXSDK_SO}
)

add_custom_command(TARGET RadioMasterAX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RMAXSDK_SO}"
        "$<TARGET_FILE_DIR:RadioMasterAX>/android-build-RadioMasterAX/libs/arm64-v8a/"
)
```

Android/Linux 必须部署目标 ABI 对应的 SDK 及其运行时依赖。当前仓库提供的 CMake
presets 同时包含 Windows x64 和 Android arm64-v8a Release。使用当前开发机配置时，
可在 VS Code 中运行 `rmAxSdk1: Build Android arm64 Release`，或执行：

```powershell
D:\Tools\QT6.8\Tools\CMake_64\bin\cmake.exe `
    --preset Android-Arm64-Release

D:\Tools\QT6.8\Tools\CMake_64\bin\cmake.exe `
    --build --preset build-android-arm64-release --parallel
```

生成的共享库位于：

```text
build/android-arm64-v8a-release/librmAxSdk1.so
```

在 VS Code 中运行以下任务时，会在编译成功后自动剥离并更新发布目录：

```text
rmAxSdk1: Build Android arm64 Release
```

`rmAxSdk1: Package Android arm64 Release` 保留为同一流程的别名。两个任务最终生成：

```text
build/dist/android-arm64-v8a/
├── SDK_API_USAGE.md
├── include/rmAxSdk1/
│   ├── rmAxSdkApi.h
│   └── rmAxSdkTypes.h
├── lib/arm64-v8a/
│   ├── librmAxSdk1.so
│   ├── libQt6Core_arm64-v8a.so
│   ├── libQt6Network_arm64-v8a.so
│   ├── libQt6SerialPort_arm64-v8a.so
│   ├── libQt6Concurrent_arm64-v8a.so
│   └── libc++_shared.so
└── licenses/
    └── Qt-LICENSE.txt
```

`liblog.so`、`libm.so`、`libz.so`、`libdl.so` 和 `libc.so` 由 Android 系统提供，
不应复制进发布目录。

Android preset 中的 SDK、NDK、JDK 和目标工具链路径与当前开发机安装位置对应。
迁移到其他开发机时，应修改 `CMakePresets.json` 中的相关路径。
