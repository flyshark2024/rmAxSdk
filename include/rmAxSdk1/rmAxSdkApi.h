/**/
#pragma once
/**/
#include <list>
#include <string_view>
#include <vector>
#include <cstdint>
#include "rmAxSdkTypes.h"

#if defined(_WIN32)
#  if defined(RMAXSDK_BUILDING_LIBRARY)
#    define RMAXSDK_API __declspec(dllexport)
#  else
#    define RMAXSDK_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define RMAXSDK_API __attribute__((visibility("default")))
#else
#  define RMAXSDK_API
#endif
/* */

/* */
namespace rmaxSdk {


/** Unified callback used for SDK output messages. */
using MessageCallback = void (*)(const SdkMessage& message, void* userData);

/** SDK initialization result. */
enum class InitResult {
  Success,
  AlreadyInitialized,
  ThreadStartFailed,
  CommunicationInitFailed
};

RMAXSDK_API std::string_view version() noexcept;
RMAXSDK_API InitResult initialize(std::string appDataPath) noexcept;
// Asynchronously submits a message to the SDK input queue.
RMAXSDK_API bool postMessage(SdkMessage message) noexcept;
// Sets the output callback. Pass nullptr to clear it.
RMAXSDK_API bool setMessageCallback(MessageCallback callback, void* userData = nullptr) noexcept;
RMAXSDK_API void shutdown() noexcept;
RMAXSDK_API void taskReadConfigFromDevice() noexcept; // 从设备读取配置
RMAXSDK_API void taskSaveConfigToDevice() noexcept; // 保存配置到设备
RMAXSDK_API void rcLoadModelFile(std::string modelFile) noexcept; // 从文件加载一个模型配置数据
//回传相关
RMAXSDK_API std::list<rmaxSdk::TelemetryNotifyUI> rcGetTelemetryList(int uiOnly) noexcept; /* 返回回传列表 */
RMAXSDK_API rmaxSdk::TelemetryNotifyUI rcGetNotifyCfgItem(uint64_t hashValue, int index) noexcept;/* 返回一个通知配置数据 */
RMAXSDK_API void rcSetNotifyCfgItem(uint64_t hashValue, const rmaxSdk::TelemetryNotifyUI &notifyCfg, int index) noexcept; /* 更新一个通知配置项目 */
RMAXSDK_API void rcDelNotifyItem(int index) noexcept;// 删除通知配置项
RMAXSDK_API std::list<rmaxSdk::TelemetryNotifyUI> rcGetNotifyCfgList() noexcept; /*返回通知配置列表*/
RMAXSDK_API int RCgetTelemetryItemNotify(rmaxSdk::TelemetryNotifyUI &notifyItem) noexcept;// 获取遥测通知列队中的单个通知项
// 遥控器功能相关接口
RMAXSDK_API void rcResetDefault() noexcept;
RMAXSDK_API void rcSkipPreFlightCheck() noexcept;
RMAXSDK_API void rcSetAdcCalibration(int isStart) noexcept;// 启动或停止 ADC 校准
RMAXSDK_API void rcResetTimer(int timerIdx) noexcept;
RMAXSDK_API void rcSetUsbToVcp(std::uint8_t mode) noexcept;
RMAXSDK_API void rcRebootRFModule() noexcept;
RMAXSDK_API void rcSetTimerCfgData(const RcTimeCfgDataUI& timerCfg, int isCounter) noexcept;// 设置定时器配置数据
RMAXSDK_API RcTimeCfgDataUI rcGetTimerCfgData(int timerIdx, int isCounter) noexcept;// 获取指定定时器索引的配置数据 0-2是timer  3-5是计数器
RMAXSDK_API rmaxSdk::RcSysCfgDataUI rcGetRcSysConfig() noexcept; // 获取遥控器系统配置数据
RMAXSDK_API void rcSetRcSysConfig(const rmaxSdk::RcSysCfgDataUI& rcSysCfg) noexcept; // 设置遥控器系统配置数据
RMAXSDK_API rmaxSdk::RadioStatusUI rcGetRadioStatus() noexcept; // 获取遥控器的基础状态
//ELRS模块相关接口
RMAXSDK_API void rcTaskRequestElrsMenuList(int reqSetDevID) noexcept;
RMAXSDK_API void rcSendCrsfRawData(std::vector<std::uint8_t> data) noexcept;
RMAXSDK_API void rcElrsSendMenuCmd(int idx,int val) noexcept;
RMAXSDK_API int rcRequestElrsStatus() noexcept; // 请求ELRS状态
//固件和APP升级相关接口
RMAXSDK_API rmaxSdk::FirmwareInfUI rcGetFwInf() noexcept; // 返回当前固件信息
RMAXSDK_API void rcTaskRequestDeviceInf(void) noexcept; // 请求设备信息
RMAXSDK_API void rcTaskStartUpdateFw() noexcept; // 开始固件更新
RMAXSDK_API rmaxSdk::AppUpdateInfoUI rcGetAppUpdateInfo() noexcept; //获取APP更新信息
RMAXSDK_API void rcTaskGetAppInfoFromWeb() noexcept; //从服务器查询APP版本信息
RMAXSDK_API int  rcTaskUpdateApp(std::string url) noexcept; //开始更新APP 返回 非0 表示已经启动
RMAXSDK_API void rcAbortUpdateApp() noexcept;//停止下载
//RC输入和输出通道设置接口
RMAXSDK_API rmaxSdk::RcTrimmingTopKeyUI rcGetTrimming() noexcept; // 获取微调和顶部按键状态
RMAXSDK_API std::vector<rmaxSdk::RcSrcCfgUI>   rcGetSrcCfgList() noexcept;// 获取RC输入通道配置列表
RMAXSDK_API std::vector<rmaxSdk::RcChOutCfgUI> rcGetChOutList() noexcept;// 获取RC输出通道配置列表
RMAXSDK_API RcMixCfgDataUI rcGetMixCfgData(int chIdx, int mixIdx) noexcept;// 获取输出通道的一条混控配置数据
RMAXSDK_API void rcSetMixCfgData(RcMixCfgDataUI mixData) noexcept;// 设置混控配置数据
RMAXSDK_API int rcDelMixCfgData(int chIdx, int srcIdx) noexcept;// 删除输出通道中指定的混控配置
RMAXSDK_API RcMixCfgDataUI rcAddMixCfgData(int chIdx, int srcIdx) noexcept;// 为指定输出通道添加一条混控配置数据
RMAXSDK_API void rcSetChOutCfgData(RcChOutCfgUI chOutCfg) noexcept;// 设置RC输出通道配置数据
RMAXSDK_API int rcDelDrData(const RcChCfgDrUI& drCfg) noexcept; // 删除指定的 DR 配置数据
RMAXSDK_API int rcSetDrData(const RcChCfgDrUI& drCfg) noexcept; // 设置指定的 DR 配置数据
RMAXSDK_API RcChCfgDrUI rcGetDrData(int chIdx, int idx) noexcept; // 获取指定通道和索引的 DR 配置数据
RMAXSDK_API std::vector<rmaxSdk::RcCurveCfgDataUI> rcGetCurveList(int idx=-1) noexcept;//返回曲线列表 idx=-1 表示返回所有列表 否则只返回指定的曲线设置
RMAXSDK_API int rcSetCurveCfg(const rmaxSdk::RcCurveCfgDataUI& curveCfg) noexcept;// 设置曲线配置数据
RMAXSDK_API double rcCurveApply(int curveIdx,double rawVal) noexcept;
//模型管理相关
RMAXSDK_API std::list<rmaxSdk::RcModelDataUI> rcGetRcModelList() noexcept;// 获取本地存储的RC模型列表
RMAXSDK_API rmaxSdk::RcModelDataUI rcSetModelCfg(rmaxSdk::RcModelDataUI rcModel) noexcept; // 修改一个模型数据
RMAXSDK_API rmaxSdk::RcModelDataUI rcCreateModelCfg() noexcept;  // 新建一个模型配置文件
RMAXSDK_API void rcSaveCurrentCfgToModel(rmaxSdk::RcModelDataUI rcModel) noexcept; // 保存当前配置到模型
RMAXSDK_API void rcExportModelCfg(std::string modelName, std::string modelFile) noexcept; // 导出配置
RMAXSDK_API int rcImportModelCfg(std::string importFile, std::string modelFile) noexcept; // 导入配置
RMAXSDK_API void rcDeleteModel(std::string filePath) noexcept; // 删除模型配置文件 
RMAXSDK_API int rcGetCurrentModelCfgIndex() noexcept; // 获取当前模型配置索引
RMAXSDK_API std::list<rmaxSdk::RcModelDataUI> rcGetTemplateList() noexcept; // 获取模板列表
RMAXSDK_API void rcModelToTemplate(std::string modelFile, std::string tempName) noexcept; // 模型保存为模板

} // namespace rmaxSdk