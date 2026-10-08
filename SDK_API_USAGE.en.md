# rmAxSdk1 SDK API Usage Guide

[简体中文](SDK_API_USAGE.zh-CN.md) | [English](SDK_API_USAGE.en.md)

This document is intended for SDK consumers and reflects the current public headers,
`appSrc/rmAxSdkApi.h` and `appSrc/rmAxSdkTypes.h`, and their implementations.
Integration experience verified in real projects and recorded in `SDK使用记录.md` has
also been incorporated into “Best Practices and Frequently Asked Questions.”

All public APIs and data types are in the `rmaxSdk` namespace. The public boundary uses
only standard C++ types, so the host does not need a particular UI framework.

---

## 1. How the SDK Works

The host interacts with the SDK through two channels:

- **Host to SDK/device:** call public functions such as `rcGet*`, `rcSet*`, and `rcTask*`.
- **SDK/device to host:** register the unified message callback with `setMessageCallback()`.

Internally, the SDK contains:

- one SDK worker thread;
- one independent message-output dispatch thread;
- bounded input and output message queues.

Most getters/setters synchronously dispatch operations to the SDK worker thread, so the
requested in-memory access has completed when the function returns. Interfaces beginning
with `task` only start a task; device communication, network requests, and update
procedures remain asynchronous.

The message callback runs on the SDK **output dispatch thread**, not on the host main
thread or the SDK worker thread. It must be thread-safe, brief, and non-blocking. Do not
directly manipulate objects that require fixed-thread access. Copy the message data first,
then transfer it through the host’s task queue, event loop, or thread scheduler.

---

## 2. Building, Linking, and Deployment

### 2.1 Artifacts and public headers

| Platform | Shared library | Link file |
| --- | --- | --- |
| Windows | `rmAxSdk1.dll` | `rmAxSdk1.lib` |
| Android/Linux | `librmAxSdk1.so` | `librmAxSdk1.so` |

Public headers:

```cpp
#include <rmAxSdkApi.h>
#include <rmAxSdkTypes.h>
```

`rmAxSdkApi.h` includes `rmAxSdkTypes.h`; normally only the former is needed.

### 2.2 Build requirements

- C++20 or later.
- The current Windows preset uses MSVC 2022, Ninja, and x64.
- A Windows host must use an MSVC toolchain and runtime ABI-compatible with the SDK.
- With a prebuilt SDK, the host needs only standard C++20 and no extra message loop.
- Deploy the runtime dependencies supplied with the SDK distribution.

> **Important: do not mix Debug/Release or `/MD` and `/MT` runtimes.**
>
> The host and SDK pass `std::string`, `std::vector`, `std::list`, and
> `std::shared_ptr` across the DLL boundary. An ABI or CRT mismatch can manifest as heap
> corruption during `initialize()`, argument destruction, or return-value destruction.
> On Windows, use the same major Visual Studio version, architecture, Debug/Release
> configuration, and `/MD` runtime family for both SDK and host.

### 2.3 Host runtime environment

The SDK does not depend on the host’s message loop. Console programs, services, game
engines, and other native C++ applications can call `initialize()` directly. Ensure that
the application-data directory passed during initialization is readable and writable,
and call `shutdown()` before exiting or unloading the shared library.

---

## 3. Lifecycle

Except for `version()`, call APIs only after `initialize()` succeeds.

### 3.1 Required call order

```cpp
#include <rmAxSdkApi.h>

#include <iostream>

void onSdkMessage(const rmaxSdk::SdkMessage& message, void* userData)
{
    (void)message;
    (void)userData;
    // Only parse and copy values quickly, then dispatch them to the host business/UI thread.
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

    // Call rcGet*/rcSet*/rcTask* during normal operation.

    rmaxSdk::setMessageCallback(nullptr, nullptr);
    rmaxSdk::shutdown();
    return 0;
}
```

### 3.2 Lifecycle APIs

```cpp
std::string_view rmaxSdk::version() noexcept;

rmaxSdk::InitResult rmaxSdk::initialize(
    std::string appDataPath) noexcept;

bool rmaxSdk::setMessageCallback(
    rmaxSdk::MessageCallback callback,
    void* userData = nullptr) noexcept;

void rmaxSdk::shutdown() noexcept;
```

`version()` returns a read-only view of an internal static build date/time string. Do not
free its data.

`appDataPath` is the directory in which the SDK stores global configuration, models,
templates, update caches, and related data. The SDK maintains configuration files and
subdirectories there. Deleting the entire directory rebuilds defaults at the next
initialization; do this only for an explicit “restore local factory data” operation.

`initialize()` return values:

| Value | Meaning |
| --- | --- |
| `InitResult::Success` | Initialization succeeded |
| `InitResult::AlreadyInitialized` | The SDK was already running; no duplicate runtime was created |
| `InitResult::ThreadStartFailed` | The SDK worker or output thread failed to start |
| `InitResult::CommunicationInitFailed` | Internal communication/business-object initialization failed |

`initialize()` is idempotent, and `shutdown()` can be called repeatedly. Reinitialization
is allowed after a complete shutdown.

Recommended exit sequence:

1. Stop the host from starting new SDK business calls.
2. Clear the message callback.
3. Wait for SDK-message work already queued by the host.
4. Release SDK payloads retained by the host.
5. Call `shutdown()`.
6. Only then unload the shared library.

---

## 4. Common Message Interface

### 4.1 Message structure

```cpp
struct SdkMessage {
    std::uint32_t msgId;
    std::shared_ptr<const void> payload;
};

using MessageCallback =
    void (*)(const SdkMessage& message, void* userData);
```

`msgId` determines the actual `payload` type. Casting to the wrong type is undefined
behavior. Check both `msgId` and `payload` before using `std::static_pointer_cast`.

Copy payload contents into host-owned value types before dispatching to another thread.
In particular, copy the `TelemetryDataUI` snapshot for `RC_MSGID_ELRS_TELEMETRY`
immediately; do not assume the underlying object remains unchanged.

### 4.2 Output message table

| Message ID | Payload type | Usage |
| --- | --- | --- |
| `RC_MSGID_LVTG_NOTIFY` | none | Low-voltage event |
| `RC_MSGID_PFC_NOTIFY` | `std::vector<std::int32_t>` | 36-channel preflight-check status |
| `RC_MSGID_TIM_REPORT` | `RcTimeReportUI` | Timer index, direction, start time, and current seconds |
| `RC_MSGID_CNT_REPORT` | `RcTimeReportUI` | Counter index, direction, initial value, and current count |
| `RC_MSGID_TIM_NOTIFY` | `RcTimeNotifyUI` | Timer index, running state, notification code, and current count |
| `RC_MSGID_TRIM_NOTIFY` | `std::uint8_t` | Trim-change event |
| `RC_MSGID_SRCINPUT_UPDATE` | undefined | Currently reserved message ID |
| `RC_MSGID_MIXOUT_UPDATE` | none | Inputs, outputs, trims, and radio state refreshed; call getters again |
| `RC_MSGID_CFGTODEV_END` | none | Current configuration written to the device |
| `RC_MSGID_GETDEVCFG_END` | none | Device configuration read completed |
| `RC_MSGID_RCTELEMETER_NOTIFY` | none | Telemetry notification queue is nonempty; drain with `RCgetTelemetryItemNotify()` |
| `RC_MSGID_APPINFO_UPDATED` | `AppUpdateInfoUI` | App update information refreshed |
| `RC_MSGID_APPDOWNLOAD_PROGRESS` | `AppUpdateInfoUI` | App download byte progress |
| `RC_MSGID_APPDOWNLOAD_END` | `AppUpdateInfoUI` | App download completed; read `cacheAppFile` |
| `RC_MSGID_FWVERSION_NOT_MATCH` | none | Device firmware is too old or incompatible |
| `RC_MSGID_FWINF_UPDATED` | none | Firmware information refreshed; call `rcGetFwInf()` |
| `RC_MSGID_FWUPDATE_PROGRESS` | `std::int32_t` | `0..100` progress, `-1` no firmware, `-2` update failed |
| `RC_MSGID_FWFILE_DOWNLOAD_END` | none | Firmware cache-file download completed |
| `RC_MSGID_ELRS_SET_ACK` | `ElrsSetEndAckUI` | ELRS menu-setting response |
| `RC_MSGID_ELRS_STATUS` | `ElrsStatusInfoUI` | ELRS status response |
| `RC_MSGID_ELRS_TELEMETRY` | `TelemetryDataUI` | ELRS position, attitude, or battery snapshot |
| `RC_MSGID_ELRS_DEVINF_UPDATE` | `std::string` | ELRS device name |
| `RC_MSGID_ELRS_MENUITEM_UPDATE` | `ElrsSetItemUI` | One ELRS menu item |
| `RC_MSGID_CRSF_DATA_RECEIVED` | `std::vector<std::uint8_t>` | Complete CRSF frame |
| `RC_MSGID_TELEMETRY_UPDATED` | none | Telemetry list changed; retrieve again as needed |
| `RC_MSGID_TELEMETRY_LOST` | none | Receiver telemetry lost |
| `RC_MSGID_TELEMETRY_RECOVER` | none | Receiver telemetry recovered |

Message parsing example:

```cpp
void onSdkMessage(const rmaxSdk::SdkMessage& message, void*)
{
    if (message.msgId == rmaxSdk::RC_MSGID_FWUPDATE_PROGRESS
        && message.payload) {
        const auto progress =
            std::static_pointer_cast<const std::int32_t>(
                message.payload);
        const std::int32_t value = *progress;
        // Dispatch value to the host thread.
    }

    if (message.msgId == rmaxSdk::RC_MSGID_ELRS_STATUS
        && message.payload) {
        const auto status =
            std::static_pointer_cast<
                const rmaxSdk::ElrsStatusInfoUI>(
                message.payload);
        const rmaxSdk::ElrsStatusInfoUI snapshot = *status;
        // Dispatch snapshot to the host thread.
    }

    if ((message.msgId == rmaxSdk::RC_MSGID_TIM_REPORT
         || message.msgId == rmaxSdk::RC_MSGID_CNT_REPORT)
        && message.payload) {
        const auto report =
            std::static_pointer_cast<
                const rmaxSdk::RcTimeReportUI>(
                message.payload);
        const rmaxSdk::RcTimeReportUI snapshot = *report;
        // Dispatch snapshot to the host thread.
    }
}
```

### 4.3 Timer message payloads

The public header defines two payload types independent of the internal device protocol:

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

`RcTimeReportUI`:

| Field | Meaning |
| --- | --- |
| `timeIdx` | Index within the timer or counter group |
| `direction` | `0` displays remaining amount; `1` displays elapsed/used amount |
| `startTime` | Start value; for timers, `0` means count up and greater than `0` means count down |
| `timeCount` | Current seconds in timer messages; current value in counter messages |

`RcTimeNotifyUI`:

| Field | Type | Meaning |
| --- | --- | --- |
| `timeIdx` | `std::uint8_t` | Timer index |
| `isRunning` | `std::uint8_t` | Running state; nonzero means running |
| `notifyCode` | `std::uint8_t` | Notification code reported by the device |
| `timeCount` | `std::int32_t` | Current count when the notification was triggered |

Consumers need only `rmAxSdkApi.h` or `rmAxSdkTypes.h`; do not include internal headers
under `rcSystem`. When retaining a payload, copy the public structure rather than saving
only the raw pointer returned by `payload.get()`.

### 4.4 Input message queue

```cpp
bool rmaxSdk::postMessage(
    rmaxSdk::SdkMessage message) noexcept;
```

This function asynchronously places a message on the SDK input queue. It returns `false`
if the SDK is uninitialized or shutting down, the queue is full, or posting fails. The
current public API defines no specific input-message business protocol. `true` means only
that the message entered the queue, not that a corresponding handler exists. Prefer a
specific public API for ordinary SDK functionality.

Both input and output queues currently have a limit of 4096. The host must not depend on
that exact value and must treat `false` as an explicit failure.

---

## 5. Reading, Modifying, and Saving Configuration

### 5.1 Three data locations

Configuration operations involve:

1. **Current SDK working configuration:** runtime configuration accessed by `rcGet*`/`rcSet*`.
2. **Local model file:** model JSON under `appDataPath`.
3. **Radio device configuration:** data actually used by the device.

`rcSet*` normally changes only the current SDK working configuration or sends a live
preview; it does not mean the target model or device has been saved.

```cpp
void rmaxSdk::taskReadConfigFromDevice() noexcept;
void rmaxSdk::taskSaveConfigToDevice() noexcept;
void rmaxSdk::rcLoadModelFile(std::string modelFile) noexcept;
void rmaxSdk::rcSaveCurrentCfgToModel(
    rmaxSdk::RcModelDataUI rcModel) noexcept;
```

- `taskReadConfigFromDevice()` asynchronously reads device configuration. On completion
  it publishes `RC_MSGID_GETDEVCFG_END` and writes the result to the current model file.
- `taskSaveConfigToDevice()` asynchronously writes the device. On completion it publishes
  `RC_MSGID_CFGTODEV_END` and saves the current model file.
- `rcLoadModelFile()` loads the specified model into the current SDK working configuration.
- `rcSaveCurrentCfgToModel()` saves the current working configuration to the model file
  specified by its argument, enabling “save as another model.”

Typical editing flow:

```cpp
auto mix = rmaxSdk::rcGetMixCfgData(0, 0);
mix.weightUp = 80;
mix.weightDown = 80;
rmaxSdk::rcSetMixCfgData(mix);       // Modify working configuration/preview.

rmaxSdk::taskSaveConfigToDevice();   // Write device; current model is persisted afterward.
```

To save the current configuration to another model:

```cpp
auto target = rmaxSdk::rcCreateModelCfg();
target.modelName = "My model";
rmaxSdk::rcSaveCurrentCfgToModel(target);
```

Local models are readable after initialization, but do not consider device data ready
until `RC_MSGID_GETDEVCFG_END`. Tasks may be delayed during the device handshake; never
replace the completion message with a fixed sleep.

---

## 6. Inputs, Outputs, Trims, and Basic Status

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

- `rcGetSrcCfgList()` returns input source names, types, raw/logical/output values, and weights.
- `rcGetChOutList()` returns output-channel configuration and live values.
- `rcGetTrimming()` returns the four trim values.
- `rcGetRadioStatus()` returns RF link state, radio/receiver voltages, top keys, and ADC calibration state.
- `rcSetChOutCfgData()` updates one output-channel configuration.

Do not poll live getters in a tight loop. After `RC_MSGID_MIXOUT_UPDATE`, retrieve
`rcGetSrcCfgList()`, `rcGetChOutList()`, `rcGetTrimming()`, and `rcGetRadioStatus()` at a
controlled cadence. Raw device data updates about every 40 ms; still coalesce UI updates
according to page visibility and rendering capacity.

Read an output channel, change only target fields, then write it back so uninitialized
fields do not overwrite other settings:

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

## 7. Mixes, Curves, DR, Timers, and System Configuration

### 7.1 Mixes

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

- `chIdx` is the output-channel index.
- `mixIdx` is the mix-item index within a channel.
- `srcIdx` is the input-source index.
- The deletion key is `(chIdx, srcIdx)`, not `(chIdx, mixIdx)`.
- `rcDelMixCfgData()` returns the remaining mix count on success and `-1` on call failure.

```cpp
auto added = rmaxSdk::rcAddMixCfgData(0, 1);
added.weightUp = 100;
added.weightDown = 100;
rmaxSdk::rcSetMixCfgData(added);

const int remaining =
    rmaxSdk::rcDelMixCfgData(0, added.srcIdx);
```

### 7.2 Curves

```cpp
std::vector<rmaxSdk::RcCurveCfgDataUI>
rmaxSdk::rcGetCurveList(int idx = -1) noexcept;

int rmaxSdk::rcSetCurveCfg(
    const rmaxSdk::RcCurveCfgDataUI& curveCfg) noexcept;

double rmaxSdk::rcCurveApply(
    int curveIdx,
    double rawVal) noexcept;
```

- `rcGetCurveList(-1)` returns all curves; a specific index returns only that curve.
- The current default model has 18 curves; always use the actual returned list.
- `rcSetCurveCfg()` returns `0` on success and `-1` for invalid configuration or call failure.
- `rcCurveApply()` returns `rawVal` unchanged on failure or an invalid index.
- `curveIdx == 0xff` means no curve is applied.
- `xVal` and `yVal` are fixed arrays of length 12; `ptNum` must not exceed capacity.

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

- Each channel currently has three DR slots.
- `rcGetDrData(chIdx, -1)` finds an unused slot.
- `rcSetDrData()` returns the channel’s used DR count on success and `-1` on failure.
- The current `rcDelDrData()` implementation only returns `0` and does not actually clear
  the DR slot. Do not interpret `0` as a persistently completed deletion.

Channel-lock fields are in `RcChOutCfgUI` and updated through `rcSetChOutCfgData()`.

### 7.4 Timers and counters

```cpp
rmaxSdk::RcTimeCfgDataUI rmaxSdk::rcGetTimerCfgData(
    int timerIdx,
    int isCounter) noexcept;

void rmaxSdk::rcSetTimerCfgData(
    const rmaxSdk::RcTimeCfgDataUI& timerCfg,
    int isCounter) noexcept;

void rmaxSdk::rcResetTimer(int timerIdx) noexcept;
```

- `isCounter == 0` selects the timer array; nonzero selects the counter array.
- Current indices in each group are `0..2`; validate indices first.
- `timerCfg.timeIdx` is the within-group index used when writing.
- In `timerCfg.direction`, `1` means count up and `0` means count down.
- `timerCfg.startTime == 0` means count up; greater than `0` is the countdown period.
  In counter mode it is the maximum counter value and cannot be `0`.
- Comparison values for `swCmpType`, `rstSwCmpType`, and `cmpType` are `0` disabled,
  `1` greater than, `2` equal to, and `3` less than.
- `outputMin` and `outputMax` range from `-100..100`.
- Timer settings send a live preview; counter settings update working configuration.
- `rcResetTimer()` sends a device reset command using the device timer number.
- `RC_MSGID_TIM_REPORT` and `RC_MSGID_CNT_REPORT` both carry `RcTimeReportUI`.
  `direction == 0` displays remaining amount and `direction == 1` elapsed/used amount;
  `timeCount` is current seconds for timers and the current value for counters.
- `RC_MSGID_TIM_NOTIFY` carries `RcTimeNotifyUI`: index, running state, notification code,
  and current count. Convert its first three `std::uint8_t` fields to integers before
  printing so stream output does not treat them as characters.

```cpp
auto timer = rmaxSdk::rcGetTimerCfgData(0, 0);
timer.timeIdx = 0;
timer.startTime = 60;
timer.beepTimeSec = 10;
rmaxSdk::rcSetTimerCfgData(timer, 0);
```

Message-handling example:

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

### 7.5 System configuration

```cpp
rmaxSdk::RcSysCfgDataUI
rmaxSdk::rcGetRcSysConfig() noexcept;

void rmaxSdk::rcSetRcSysConfig(
    const rmaxSdk::RcSysCfgDataUI& rcSysCfg) noexcept;
```

The current implementation actually reads/writes `alramVtg`, `rfModuleMode`, `stickMode`,
and `preFlightCheckChannel[36]`.

Other `RcSysCfgDataUI` fields are not fully mapped by the getter/setter. Do not assume
`fsMode`, `fsChVal`, `joytickBeep`, `modelMatchId`, or `crsfBaudRate` is persisted by
these APIs. Update model matching through `RcModelDataUI::modelMatch` and model APIs.

---

## 8. Device Control

```cpp
void rmaxSdk::rcResetDefault() noexcept;
void rmaxSdk::rcSkipPreFlightCheck() noexcept;
void rmaxSdk::rcSetAdcCalibration(int isStart) noexcept;
void rmaxSdk::rcSetUsbToVcp(std::uint8_t mode) noexcept;
void rmaxSdk::rcRebootRFModule() noexcept;
```

| API | Purpose |
| --- | --- |
| `rcResetDefault()` | Restore defaults and trigger another read |
| `rcSkipPreFlightCheck()` | Skip the current preflight check |
| `rcSetAdcCalibration(isStart)` | Start ADC calibration when nonzero; stop when `0` |
| `rcSetUsbToVcp(mode)` | Set USB virtual-serial passthrough mode; values are device-protocol-defined |
| `rcRebootRFModule()` | Request an RF-module reboot |

Read current ADC state from `rcGetRadioStatus().adcSetState`.

---

## 9. Models and Templates

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

`RcModelDataUI` contains `modelName`, `modelImg`, `filePath`, `modelIdx`, and `modelMatch`.

| API | Current behavior |
| --- | --- |
| `rcGetRcModelList()` | Scan and return the model directory |
| `rcCreateModelCfg()` | Generate new model metadata and destination path; **does not immediately create a file** |
| `rcSetModelCfg()` | Load the target, update metadata, and save; used to modify existing model metadata |
| `rcSaveCurrentCfgToModel()` | Write current working configuration and supplied metadata to the target model |
| `rcLoadModelFile()` | Load the target model as current working configuration |
| `rcExportModelCfg(source, destination)` | Copy a model file to the export path |
| `rcImportModelCfg(source, target)` | Import source configuration into a target model; return `0` on success |
| `rcDeleteModel()` | Delete the specified model file |
| `rcGetCurrentModelCfgIndex()` | Return the current model index; `-1` on failure |
| `rcGetTemplateList()` | Return the template list |
| `rcModelToTemplate()` | Save a model as a template with the specified name |

Create a model and save current configuration:

```cpp
auto model = rmaxSdk::rcCreateModelCfg();
model.modelName = "Plane";
model.modelImg = "qrc:/image/plane.png";
model.modelMatch = 1;

rmaxSdk::rcSaveCurrentCfgToModel(model);
```

Use `rcSetModelCfg()` only to change an existing model’s name or image. It is not a
replacement for “save current working configuration.”

---

## 10. Telemetry and Notifications

### 10.1 Telemetry list

```cpp
std::list<rmaxSdk::TelemetryNotifyUI>
rmaxSdk::rcGetTelemetryList(int uiOnly) noexcept;
```

- `uiOnly == 0`: return every telemetry item.
- `uiOnly != 0`: return only items whose `showUI != 0`.
- `RC_MSGID_TELEMETRY_UPDATED` can be frequent; do not rebuild the entire UI for each one.

In production, retain the latest state in the background; throttle UI list refreshes
(for example, 500 ms to 1.5 s); stop UI refreshes while the page is hidden; and allow an
explicit user request to bypass throttling.

### 10.2 Notification configuration

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

Current `rcSetNotifyCfgItem()` decision rules:

- `hashValue == 0`: add a notification; `index` is not used for location.
- `hashValue != 0`: modify an existing notification by `index`; the caller must ensure it is valid.

For an addition, `index == -1` may communicate caller intent, but the first argument
`hashValue == 0` is what actually selects insertion.

`hashValue` is `std::uint64_t`; business or scripting bindings must not store it in a
32-bit `int`, which would truncate it.

### 10.3 Triggered-notification queue

```cpp
int rmaxSdk::RCgetTelemetryItemNotify(
    rmaxSdk::TelemetryNotifyUI& notifyItem) noexcept;
```

`RC_MSGID_RCTELEMETER_NOTIFY` only indicates that the queue is nonempty; it carries no
notification payload. Drain it after receipt:

```cpp
rmaxSdk::TelemetryNotifyUI item{};
while (rmaxSdk::RCgetTelemetryItemNotify(item) != -1) {
    // Copy item and perform speech, popup, or logging.
}
```

Success returns the queue length before removal. An empty queue or call failure returns `-1`.

### 10.4 Choosing the public telemetry type

`rmAxSdkTypes.h` declares both `TelemetryNotifyUI` and `TelemetryItemUI`. The public APIs
`rcGetTelemetryList()`, `rcGetNotifyCfgItem()`, `rcSetNotifyCfgItem()`,
`rcGetNotifyCfgList()`, and `RCgetTelemetryItemNotify()` all use `TelemetryNotifyUI`.

No current public API parameter, return value, or message payload uses `TelemetryItemUI`.
Do not substitute it in these APIs; treat it only as a reserved public data structure
unless a future public API explicitly adopts it.

---

## 11. ELRS and CRSF

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

### 11.1 Menu

`rcTaskRequestElrsMenuList(reqSetDevID)` asynchronously requests the parameter menu of
the specified ELRS device. A common transmitter address is `0xEE`; use the actual CRSF
device address.

The usual response sequence is:

1. `RC_MSGID_ELRS_DEVINF_UPDATE`: device name.
2. Multiple `RC_MSGID_ELRS_MENUITEM_UPDATE` messages, one `ElrsSetItemUI` each.
3. Call `rcElrsSendMenuCmd(idx, val)` to make a change.
4. `RC_MSGID_ELRS_SET_ACK`: `ElrsSetEndAckUI`.

After leaving the ELRS page, stop the host’s repeated refreshes/requests. Requests made
while a menu request is active are ignored by internal state, and frequent repeated
retrieval can make the page unresponsive.

### 11.2 ELRS status

`rcRequestElrsStatus()` sends a status request:

- `0`: request sent.
- `-1`: an ELRS menu request is active, the SDK is unavailable, or the call failed.

A successful send does not mean a response has arrived. `RC_MSGID_ELRS_STATUS` carries
an `ElrsStatusInfoUI` response:

- `badPkt`: bad-packet count;
- `goodPkt`: good-packet count;
- `modelMismatch`: model-match state; nonzero means mismatch.

```cpp
if (rmaxSdk::rcRequestElrsStatus() != 0) {
    // Busy or SDK unavailable; retry later.
}
```

### 11.3 Raw CRSF data

`rcSendCrsfRawData()` sends caller-provided raw bytes. An empty array is not sent.

`RC_MSGID_CRSF_DATA_RECEIVED` returns a complete frame:

```text
ADDR LEN TYPE DATA CRC
```

The total byte count must be `LEN + 2`; CRC8-D5 covers `TYPE + DATA`.

---

## 12. Firmware and App Updates

### 12.1 Device firmware

```cpp
rmaxSdk::FirmwareInfUI
rmaxSdk::rcGetFwInf() noexcept;

void rmaxSdk::rcTaskRequestDeviceInf() noexcept;
void rmaxSdk::rcTaskStartUpdateFw() noexcept;
```

Procedure:

1. Call `rcTaskRequestDeviceInf()`.
2. Receive `RC_MSGID_FWINF_UPDATED`.
3. Call `rcGetFwInf()` for device and cached-firmware information.
4. Call `rcTaskStartUpdateFw()` when business conditions permit.
5. Handle `RC_MSGID_FWUPDATE_PROGRESS`.

`FirmwareInfUI` contains system ID, device ID, flight time, build time, firmware/hardware
versions, device description, and cached firmware version text.

Firmware progress values:

- `0..100`: progress;
- `-1`: no firmware available;
- `-2`: update failed.

### 12.2 App update

```cpp
rmaxSdk::AppUpdateInfoUI
rmaxSdk::rcGetAppUpdateInfo() noexcept;

void rmaxSdk::rcTaskGetAppInfoFromWeb() noexcept;

int rmaxSdk::rcTaskUpdateApp(
    std::string url) noexcept;

void rmaxSdk::rcAbortUpdateApp() noexcept;
```

Procedure:

1. Call `rcTaskGetAppInfoFromWeb()`.
2. Receive `RC_MSGID_APPINFO_UPDATED`; read its payload or call `rcGetAppUpdateInfo()`.
3. Call `rcTaskUpdateApp(info.apkUrl)`.
4. Handle `RC_MSGID_APPDOWNLOAD_PROGRESS`.
5. After `RC_MSGID_APPDOWNLOAD_END`, read `cacheAppFile` and let the host install it.

`rcTaskUpdateApp()` returns `0` when the download starts and `-1` if another download
exists, the SDK is unavailable, or the call fails. `rcAbortUpdateApp()` aborts the active download.

---

## 13. Return Values and Failure Conventions

All public APIs are `noexcept`; exceptions never cross the DLL/SO boundary.

| API kind | When the SDK is unavailable or the call fails |
| --- | --- |
| `std::vector`/`std::list` getter | Empty container |
| Structure getter | Value-initialized default structure |
| `rcGetCurrentModelCfgIndex()` | `-1` |
| `rcDelMixCfgData()` | `-1` |
| `rcSetDrData()`/`rcDelDrData()` | `-1` |
| `rcSetCurveCfg()` | `-1` |
| `rcImportModelCfg()` | `-1` |
| `rcTaskUpdateApp()` | `-1` |
| `rcRequestElrsStatus()` | `-1` |
| `RCgetTelemetryItemNotify()` | `-1` |
| `rcCurveApply()` | Return `rawVal` unchanged |
| `void` setter/task API | No operation; failure cannot be observed through a return value |

An empty container or default structure can mean either “there is genuinely no data” or
failure. Interpret it using lifecycle state, device completion messages, and host business
state.

The caller validates indices, paths, URLs, and protocol parameters. Some internal code
indexes arrays directly; an out-of-range argument is not a recoverable business error.

---

## 14. Best Practices and Frequently Asked Questions

### 14.1 Do not treat getters as device truth before the device is ready

Local models can be read immediately after `initialize()`, but wait for
`RC_MSGID_GETDEVCFG_END` before trusting device configuration. Tasks may queue or be
delayed during startup handshaking.

### 14.2 Do not manipulate UI directly in callbacks

The callback runs on the independent output thread:

1. Validate message ID and payload.
2. Immediately copy into a host-owned value.
3. Dispatch through a thread-safe queue, event loop, or task scheduler.
4. Return immediately.

Do not perform network access, disk I/O, lock waits, or long computations in a callback.

### 14.3 Coalesce or throttle high-frequency messages

- `RC_MSGID_MIXOUT_UPDATE` can arrive about every 40 ms.
- `RC_MSGID_TELEMETRY_UPDATED` may arrive continuously with telemetry frames.

Do not rebuild the entire UI model for each message. Overwrite with latest values, refresh
in timed batches, and pause UI updates when the page is hidden.

### 14.4 Distinguish save semantics

- `rcSet*`: modify current working configuration/live preview.
- `rcSaveCurrentCfgToModel(model)`: save to a specified local model.
- `taskSaveConfigToDevice()`: asynchronously write the device, then save the current model.
- `rcSetModelCfg(model)`: primarily modify metadata for an existing model.

### 14.5 Do not use a 32-bit int for large integers across binding layers

`TelemetryNotifyUI::hashValue` is 64-bit. Use a 64-bit unsigned integer or string in host
bindings, not a 32-bit `int`. If the target scripting language cannot exactly represent
all 64-bit integers, prefer a string.

### 14.6 Use only public payload types

Timer, counter, and timer-notification messages provide `RcTimeReportUI` and
`RcTimeNotifyUI`. Include only public headers and cast to these public types; do not refer
to or copy internal protocol structures under `rcSystem`.

### 14.7 Release cross-boundary objects before unloading

Before unloading the shared library, clear the callback, shut down the SDK, and destroy
host-retained `SdkMessage` objects, payloads, and SDK-returned containers and strings.

---

## 15. CMake Integration

### 15.1 As a source subdirectory

```cmake
cmake_minimum_required(VERSION 3.16)
project(SdkConsumer LANGUAGES CXX)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/rmAxSdk1)

add_executable(sdk_consumer main.cpp)
target_compile_features(sdk_consumer PRIVATE cxx_std_20)
target_link_libraries(sdk_consumer PRIVATE rmAxSdk1::rmAxSdk1)
```

### 15.2 Using the Windows binary library

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

The deployment directory needs at least `rmAxSdk1.dll`, runtime dependencies supplied
with the SDK, and the required MSVC runtime if absent on the target machine. Runtime
dependency versions and architecture must match `rmAxSdk1.dll`.

### 15.3 Android arm64 example

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

On Android/Linux, deploy the SDK and runtime dependencies for the target ABI. Current
CMake presets include Windows x64 and Android arm64-v8a Release. On the current
development machine, run the VS Code task `rmAxSdk1: Build Android arm64 Release`, or:

```powershell
D:\Tools\QT6.8\Tools\CMake_64\bin\cmake.exe `
    --preset Android-Arm64-Release

D:\Tools\QT6.8\Tools\CMake_64\bin\cmake.exe `
    --build --preset build-android-arm64-release --parallel
```

The shared library is generated at:

```text
build/android-arm64-v8a-release/librmAxSdk1.so
```

The following VS Code task strips the library and updates the distribution directory
after a successful build:

```text
rmAxSdk1: Build Android arm64 Release
```

`rmAxSdk1: Package Android arm64 Release` remains an alias for the same process. Both
tasks ultimately produce:

```text
build/dist/android-arm64-v8a/
├── SDK_API_USAGE.zh-CN.md
├── SDK_API_USAGE.en.md
├── tst_rmAxSdkApi.cpp
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

`liblog.so`, `libm.so`, `libz.so`, `libdl.so`, and `libc.so` are supplied by Android and
must not be copied into the distribution.

The Android preset’s SDK, NDK, JDK, and target-toolchain paths correspond to their
locations on the current development machine. Update the relevant paths in
`CMakePresets.json` when moving to another machine.
