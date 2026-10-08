#include "rmAxSdkApi.h"
#include "rmAxSdkTestConsole.h"

#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {
const std::string appDataPath = "D:/TDDOWNLOAD/AppDataRadioMasterAX";

struct CallbackContext {
  std::atomic_uint64_t messageCount{0};
};


// 将初始化结果转换为可读文本。
const char *initResultText(rmaxSdk::InitResult result) {
  switch (result) {
  case rmaxSdk::InitResult::Success:
    return "success";
  case rmaxSdk::InitResult::AlreadyInitialized:
    return "already initialized";
  case rmaxSdk::InitResult::ThreadStartFailed:
    return "thread start failed";
  case rmaxSdk::InitResult::CommunicationInitFailed:
    return "communication initialization failed";
  }
  return "unknown";
}

// 输出控制台支持的命令列表。
void printHelp() {
  std::cout
      << "Commands:\n"
      << "  help      Show this help\n"
      << "  version   Show SDK version\n"
      << "  status    Show SDK state\n"
      << "  init      Initialize SDK\n"
      << "  shutdown  Shut down SDK\n"
      << "  getSrcCfg                 List input source configurations\n"
      << "  getChOutCfg               List output channel configurations\n"
      << "  getTelemetry [uiOnly]     List telemetry items (default: all)\n"
      << "  getMixCfg <ch> <mix>      Show one mixing configuration\n"
      << "  getTimerCfg <timer>       Show one timer configuration\n"
      << "  getRadioStatus            Show radio status\n"
      << "  getCurve [idx]            List all curves or one curve\n"
      << "  setCurve <idx> <type> <ptNum> <x0> <y0> ...\n"
      << "                            Update one curve configuration\n"
      << "  applyCurve <idx> <raw>    Apply a curve to an input value\n"
      << "  getFwInf                  Show cached device firmware information\n"
      << "  requestFwInf              Request firmware information from the device\n"
      << "  requestElrsParameters     Request parameters from the ELRS module\n"
      << "  requestElrsStatus         Request status from the ELRS module\n"
      << "  startUpdateFw             Start the firmware update\n"
      << "  getAppInfo                Request application update information\n"
      << "  updateApp <url>           Start downloading an application update\n"
      << "  abortAppUpdate            Abort the application update download\n"
      << "  getModels                 List local model files\n"
      << "  createModel               Create a model configuration\n"
      << "  setModel <file> <name> <image> <index> <match>\n"
      << "                            Update model metadata\n"
      << "  exportModel <source> <destination>\n"
      << "                            Export a model file\n"
      << "  importModel <source> <target>\n"
      << "                            Import configuration into a model\n"
      << "  deleteModel <file>        Delete a model file\n"
      << "  getTemplates              List local templates\n"
      << "  modelToTemplate <file> <name>\n"
      << "                            Save a model as a template\n"
      << "  quit      Shut down SDK and exit\n";
}

struct CommandContext {
  std::string_view libraryVersion;
  bool &initialized;
  std::string arguments;
};

using CommandHandler = bool (*)(CommandContext &);

// 显示命令帮助。
bool help(CommandContext &) {
  printHelp();
  return true;
}

// 向 SDK 投递一条测试消息。
bool postMsg(CommandContext &) {
  rmaxSdk::postMessage({1, std::make_shared<const std::vector<std::uint8_t>>(
                              std::vector<std::uint8_t>{0x10, 0x20})});
  return true;
}

// 将当前配置保存到设备。
bool saveConfig(CommandContext &) {
  rmaxSdk::taskSaveConfigToDevice();
  return true;
}

// 从设备读取当前配置。
bool readConfig(CommandContext &) {
  rmaxSdk::taskReadConfigFromDevice();
  return true;
}
void printSrcCfg() {
  const auto items = rmaxSdk::rcGetSrcCfgList();
  int16_t luaCh[4];
  luaCh[0]= items[3].rawVal*2;//副翼
  luaCh[1]= items[1].rawVal*2;//升降
  luaCh[2]= items[2].rawVal*2;//油门
  luaCh[3]= items[0].rawVal*2;//偏航
  // std::cout << "Lua channels: "
  //           << luaCh[0] << ' '
  //           << luaCh[1] << ' '
  //           << luaCh[2] << ' '
  //           << luaCh[3] << '\n';
  // std::cout << "Input sources: " << items.size() << '\n';
  // for (const auto &item : items) {
  //   std::cout << "  [" << item.srcIdx << "] " << item.srcName
  //             << " type=" << item.srcType
  //             << " enabled=" << item.isEnable
  //             << " raw=" << item.rawVal
  //             << " logic=" << item.rawLogicVal
  //             << " output=" << item.outputVal
  //             << " weight=" << item.weight << '\n';
  // }
}
// 输出全部输入源配置。
bool getSrcCfg(CommandContext &) {
  printSrcCfg();
  return true;
}

// 输出全部通道配置。
bool getChOutCfg(CommandContext &) {
  const auto items = rmaxSdk::rcGetChOutList();
  std::cout << "Output channels: " << items.size() << '\n';
  for (const auto &item : items) {
    std::cout << "  CH" << item.chIdx
              << " output=" << item.chOut
              << " logic=" << item.logicVal
              << " weight=" << item.weight
              << " range=[" << item.min << ',' << item.middle << ',' << item.max << ']'
              << " curve=" << item.curveIdx
              << " mixes=" << item.mixesNum << '\n';
  }
  return true;
}

// 输出回传列表。
void printTelemetryList(int uiOnly) {
  const auto items = rmaxSdk::rcGetTelemetryList(uiOnly);
  // std::cout << "Telemetry items: " << items.size() << '\n';
  for (const auto &item : items) {
    std::cout << "  [" << item.idx << "] " << item.notifyName
              << " value=" << item.currValue
              << " text=" << item.valTxt
              << " unit=" << item.valueUnit
              << " hash=" << item.hashValue
              << " showUI=" << item.showUI << '\n';
  }
}

// 输出全部回传项目，或仅输出需要在 UI 中显示的项目。
bool getTelemetry(CommandContext &context) {
  int uiOnly = 0;
  if (!context.arguments.empty()) {
    std::istringstream input(context.arguments);
    if (!(input >> uiOnly)) {
      std::cout << "Usage: getTelemetry [uiOnly]\n";
      return true;
    }
  }
  printTelemetryList(uiOnly);
  return true;
}

// 按通道和混控索引输出单个混控配置。
bool getMixCfg(CommandContext &context) {
  int chIdx = 0;
  int mixIdx = 0;
  std::istringstream input(context.arguments);
  if (!(input >> chIdx >> mixIdx)) {
    std::cout << "Usage: getMixCfg <ch> <mix>\n";
    return true;
  }

  const auto item = rmaxSdk::rcGetMixCfgData(chIdx, mixIdx);
  std::cout << "Mix ch=" << item.chIdx
            << " mix=" << item.mixIdx
            << " source=" << item.srcName
            << " srcIdx=" << item.srcIdx
            << " srcType=" << item.srcType
            << " offset=" << item.offset
            << " curve=" << item.curveIdx
            << " switch=[" << item.switchIdx << ',' << item.swCmpType << ',' << item.swCmpVal << ']'
            << " mode=" << item.mixesMode
            << " slow=[" << item.slowMsUp << ',' << item.slowMsDown << ']'
            << " weight=[" << item.weightUp << ',' << item.weightDown << ']'
            << " count=" << item.mixesCount << '\n';
  return true;
}

// 按索引输出定时器配置。
bool getTimerCfg(CommandContext &context) {
  int timerIdx = 0;
  std::istringstream input(context.arguments);
  if (!(input >> timerIdx)) {
    std::cout << "Usage: getTimerCfg <timer>\n";
    return true;
  }

  const auto item = rmaxSdk::rcGetTimerCfgData(timerIdx, 0);
  std::cout << "Timer index=" << timerIdx
            << " mode=" << item.mode
            << " start=" << item.startTime
            << " direction=" << item.direction
            << " countdown=" << item.countdown
            << " beep=" << item.beepTimeSec
            << " enableSwitch=[" << item.switchIdx << ',' << item.swCmpType << ',' << item.switchAct << ']'
            << " resetSwitch=[" << item.resetSwIdx << ',' << item.rstSwCmpType << ',' << item.resetSwAct << ']'
            << " compare=[" << item.cmpValue << ',' << item.cmpType << ']'
            << " loop=" << item.loopMode
            << " output=[" << item.outputMode << ',' << item.outputMin << ',' << item.outputMax << ']'
            << " threshold=" << item.threshold
            << " count=" << item.timeCount
            << " notify=" << item.notifyCode << '\n';
  return true;
}

// 输出遥控器基础状态。
bool getRadioStatus(CommandContext &) {
  const auto status = rmaxSdk::rcGetRadioStatus();
  std::cout << "Radio status:"
            << " rfLQ=" << status.rfLQ
            << " rfLDBM=" << status.rfLDBM
            << " rfLLostCnt=" << status.rfLLostCnt
            << " rcVtg=" << status.rcVtg
            << " rxVtg=" << status.rxVtg
            << " topKeyState=" << status.topKeyState
            << " adcSetState=" << status.adcSetState << '\n';
  return true;
}

// 输出全部曲线或指定索引的曲线配置。
bool getCurve(CommandContext &context) {
  int curveIdx = -1;
  if (!context.arguments.empty()) {
    std::istringstream input(context.arguments);
    if (!(input >> curveIdx)) {
      std::cout << "Usage: getCurve [idx]\n";
      return true;
    }
  }

  const auto curves = rmaxSdk::rcGetCurveList(curveIdx);
  std::cout << "Curves: " << curves.size() << '\n';
  for (const auto &curve : curves) {
    std::cout << "  [" << curve.idx << "] type=" << curve.cType
              << " points=" << curve.ptNum << " values=";
    const std::size_t pointCount = curve.ptNum > 0
        ? static_cast<std::size_t>(curve.ptNum)
        : 1;
    for (std::size_t index = 0; index < pointCount && index < 12; ++index) {
      std::cout << " (" << curve.xVal[index] << ',' << curve.yVal[index] << ')';
    }
    std::cout << '\n';
  }
  return true;
}

// 从成对的 x/y 参数更新一条曲线配置。
bool setCurve(CommandContext &context) {
  rmaxSdk::RcCurveCfgDataUI curve{};
  std::istringstream input(context.arguments);
  if (!(input >> curve.idx >> curve.cType >> curve.ptNum) ||
      curve.ptNum < 0 || curve.ptNum > 12) {
    std::cout << "Usage: setCurve <idx> <type> <ptNum> <x0> <y0> ...\n";
    return true;
  }

  const int pointCount = curve.ptNum > 0 ? curve.ptNum : 1;
  for (int index = 0; index < pointCount; ++index) {
    int x = 0;
    int y = 0;
    if (!(input >> x >> y)) {
      std::cout << "Expected " << pointCount << " x/y point pairs\n";
      return true;
    }
    curve.xVal[index] = x;
    curve.yVal[index] = y;
  }

  std::cout << "Set curve result: " << rmaxSdk::rcSetCurveCfg(curve) << '\n';
  return true;
}

// 将输入值应用到指定曲线并输出计算结果。
bool applyCurve(CommandContext &context) {
  int curveIdx = 0;
  double rawValue = 0;
  std::istringstream input(context.arguments);
  if (!(input >> curveIdx >> rawValue)) {
    std::cout << "Usage: applyCurve <idx> <raw>\n";
    return true;
  }

  std::cout << "Curve output: " << rmaxSdk::rcCurveApply(curveIdx, rawValue) << '\n';
  return true;
}

// 输出单个模型或模板的元数据。
void printModelData(const rmaxSdk::RcModelDataUI &model) {
  std::cout << "  name=" << model.modelName
            << " image=" << model.modelImg
            << " file=" << model.filePath
            << " index=" << model.modelIdx
            << " match=" << model.modelMatch << '\n';
}

// 输出本地模型列表。
bool getModels(CommandContext &) {
  const auto models = rmaxSdk::rcGetRcModelList();
  std::cout << "Models: " << models.size() << '\n';
  for (const auto &model : models) {
    printModelData(model);
  }
  return true;
}

// 新建模型配置文件并输出生成的模型信息。
bool createModel(CommandContext &) {
  const auto model = rmaxSdk::rcCreateModelCfg();
  // std::cout << "Created model:\n";
  printModelData(model);
  return true;
}

// 修改指定模型文件的元数据。含空格的参数可以使用双引号包围。
bool setModel(CommandContext &context) {
  rmaxSdk::RcModelDataUI model{};
  std::istringstream input(context.arguments);
  if (!(input >> std::quoted(model.filePath)
              >> std::quoted(model.modelName)
              >> std::quoted(model.modelImg)
              >> model.modelIdx
              >> model.modelMatch)) {
    std::cout << "Usage: setModel <file> <name> <image> <index> <match>\n";
    return true;
  }
  printModelData(rmaxSdk::rcSetModelCfg(std::move(model)));
  return true;
}

// 将模型文件导出到目标路径。
bool exportModel(CommandContext &context) {
  std::string source;
  std::string destination;
  std::istringstream input(context.arguments);
  if (!(input >> std::quoted(source) >> std::quoted(destination))) {
    std::cout << "Usage: exportModel <source> <destination>\n";
    return true;
  }
  rmaxSdk::rcExportModelCfg(std::move(source), std::move(destination));
  std::cout << "Model export requested\n";
  return true;
}

// 将来源配置导入目标模型文件。
bool importModel(CommandContext &context) {
  std::string source;
  std::string target;
  std::istringstream input(context.arguments);
  if (!(input >> std::quoted(source) >> std::quoted(target))) {
    std::cout << "Usage: importModel <source> <target>\n";
    return true;
  }
  std::cout << "Import result: "
            << rmaxSdk::rcImportModelCfg(std::move(source), std::move(target)) << '\n';
  return true;
}

// 删除显式指定的模型文件。
bool deleteModelFile(CommandContext &context) {
  std::string filePath;
  std::istringstream input(context.arguments);
  if (!(input >> std::quoted(filePath))) {
    std::cout << "Usage: deleteModel <file>\n";
    return true;
  }
  rmaxSdk::rcDeleteModel(std::move(filePath));
  std::cout << "Model deletion requested\n";
  return true;
}

// 输出本地模板列表。
bool getTemplates(CommandContext &) {
  const auto templates = rmaxSdk::rcGetTemplateList();
  std::cout << "Templates: " << templates.size() << '\n';
  for (const auto &model : templates) {
    printModelData(model);
  }
  return true;
}

// 输出设备固件信息。
void printFirmwareInfo(const rmaxSdk::FirmwareInfUI& fwInf) {
  std::ostringstream output;
  output << "  SysId: 0x" << std::hex << std::setw(8) << std::setfill('0') << fwInf.SysId << '\n'
         << "  DevId: 0x" << std::setw(8) << fwInf.DevId << '\n'
         << "  FlyTime: " << std::dec << fwInf.FlyTime << " s\n"
         << "  BuildTime: " << fwInf.BuildTime << '\n'
         << "  Firmware version: " << fwInf.FwVerText
         << " (0x" << std::hex << std::setw(8) << fwInf.VER_FW << ")\n"
         << "  Hardware version: 0x" << std::setw(8) << fwInf.VER_HW << '\n'
         << "  Device: " << fwInf.DevText << '\n'
         << "  Build time text: " << fwInf.BulidTimeText << '\n'
         << "  Cache firmware file version text: " << fwInf.cacheFwFileVerTxt << '\n'
         ;
  std::cout << output.str();
}

// 输出 APP 更新信息和当前下载进度。
void printAppUpdateInfo(const rmaxSdk::AppUpdateInfoUI& appUpdateInfo) {
  std::cout << "  Version: " << appUpdateInfo.version << '\n'
            << "  APK URL: " << appUpdateInfo.apkUrl << '\n'
            << "  Download: " << appUpdateInfo.downloadBytesReceived
            << '/' << appUpdateInfo.downloadBytesTotal << " bytes\n"
            << "  Chinese update content:\n" << appUpdateInfo.cnUpdateContent << '\n'
            << "  English update content:\n" << appUpdateInfo.enUpdateContent << '\n';
}

// 获取并打印 SDK 缓存的设备固件信息。
bool getFwInf(CommandContext &) {
  printFirmwareInfo(rmaxSdk::rcGetFwInf());
  return true;
}

// 请求设备固件信息。
bool requestFwInf(CommandContext &) {
  rmaxSdk::rcTaskRequestDeviceInf();
  return true;
}

// 请求 ELRS 模块参数。
bool requestElrsParameters(CommandContext &) {
  rmaxSdk::rcTaskRequestElrsMenuList(0xEE); // CRSF_ADDRESS_CRSF_TRANSMITTER
  // std::cout << "ELRS parameter request started\n";
  return true;
}

// 请求 ELRS 模块状态。
bool requestElrsStatus(CommandContext &) {
  std::cout << "ELRS status request result: "
            << rmaxSdk::rcRequestElrsStatus() << '\n';
  return true;
}

// 启动设备固件更新。
bool startUpdateFw(CommandContext &) {
  rmaxSdk::rcTaskStartUpdateFw();
  return true;
}

// 请求服务器返回最新的 APP 版本信息。
bool getAppInfo(CommandContext &) {
  rmaxSdk::rcTaskGetAppInfoFromWeb();
  return true;
}

// 使用指定 URL 启动 APP 更新下载。
bool updateApp(CommandContext &context) {
  std::string url;
  std::istringstream input(context.arguments);
  if (!(input >> std::quoted(url))) {
    std::cout << "Usage: updateApp <url>\n";
    return true;
  }
  std::cout << "Application update result: "
            << rmaxSdk::rcTaskUpdateApp(std::move(url)) << '\n';
  return true;
}

// 中止正在进行的 APP 更新下载。
bool abortAppUpdate(CommandContext &) {
  rmaxSdk::rcAbortUpdateApp();
  std::cout << "Application update abort requested\n";
  return true;
}

// 将模型保存为指定名称的模板。
bool saveModelAsTemplate(CommandContext &context) {
  std::string modelFile;
  std::string templateName;
  std::istringstream input(context.arguments);
  if (!(input >> std::quoted(modelFile) >> std::quoted(templateName))) {
    std::cout << "Usage: modelToTemplate <file> <name>\n";
    return true;
  }
  rmaxSdk::rcModelToTemplate(std::move(modelFile), std::move(templateName));
  // std::cout << "Template save requested\n";
  return true;
}

// 显示 SDK 版本。
bool version(CommandContext &context) {
  std::cout << context.libraryVersion << '\n';
  return true;
}

// 显示 SDK 初始化状态。
bool status(CommandContext &context) {
  std::cout << (context.initialized ? "initialized" : "stopped") << '\n';
  return true;
}

// 初始化 SDK 并更新运行状态。
bool init(CommandContext &context) {
  const rmaxSdk::InitResult result = rmaxSdk::initialize(appDataPath);
  context.initialized = result == rmaxSdk::InitResult::Success || result == rmaxSdk::InitResult::AlreadyInitialized;
  std::cout << "Initialization result: " << initResultText(result) << '\n';
  return true;
}

// 关闭 SDK 并更新运行状态。
bool shutdown(CommandContext &context) {
  rmaxSdk::shutdown();
  context.initialized = false;
  std::cout << "SDK stopped\n";
  return true;
}

// 退出命令循环。
bool quit(CommandContext &) {
  return false;
}

// 退出命令循环。
bool exit(CommandContext &) {
  return false;
}
// ----------接收并打印 SDK 输出线程发布的消息---------------
void onSdkMessage(const rmaxSdk::SdkMessage &message, void *userData) {
  auto *context = static_cast<CallbackContext *>(userData);
  const std::uint64_t sequence = context
      ? context->messageCount.fetch_add(1, std::memory_order_relaxed) + 1 : 0;
  /**/
  if(message.msgId == rmaxSdk::RC_MSGID_MIXOUT_UPDATE){//混空更新
    //std::cout << "Mixout update received, sequence=" << sequence << '\n';
    printSrcCfg();

  }else if(message.msgId == rmaxSdk::RC_MSGID_TIM_REPORT
           || message.msgId == rmaxSdk::RC_MSGID_CNT_REPORT) {
    if (message.payload) {
      const auto report =
          std::static_pointer_cast<const rmaxSdk::RcTimeReportUI>(
              message.payload);
      std::cout << (message.msgId == rmaxSdk::RC_MSGID_TIM_REPORT
                        ? "Timer" : "Counter")
                << " report: index=" << report->timeIdx
                << ", direction=" << report->direction
                << ", start=" << report->startTime
                << ", count=" << report->timeCount << '\n';
    }
  }else if(message.msgId == rmaxSdk::RC_MSGID_TIM_NOTIFY) {
    if (message.payload) {
      const auto notify =
          std::static_pointer_cast<const rmaxSdk::RcTimeNotifyUI>(
              message.payload);
      std::cout << "Timer notification: index=" << notify->timeIdx
                << ", running=" << notify->isRunning
                << ", code=" << notify->notifyCode
                << ", count=" << notify->timeCount << '\n';
    }
  } else if(message.msgId == rmaxSdk::RC_MSGID_APPDOWNLOAD_PROGRESS) {
    const rmaxSdk::AppUpdateInfoUI appUpdateInfo = rmaxSdk::rcGetAppUpdateInfo();
    std::cout << "App download progress received, sequence=" << sequence
              << ", bytes=" << appUpdateInfo.downloadBytesReceived
              << '/' << appUpdateInfo.downloadBytesTotal << '\n';
  }else if(message.msgId == rmaxSdk::RC_MSGID_FWINF_UPDATED) {
    // 处理设备信息响应
    std::cout << "Firmware info received, sequence=" << sequence << '\n';
    printFirmwareInfo(rmaxSdk::rcGetFwInf());
  }else if(message.msgId == rmaxSdk::RC_MSGID_APPINFO_UPDATED) {
    // 处理 APP 信息更新
    std::cout << "Application update info received, sequence=" << sequence << '\n';
    printAppUpdateInfo(rmaxSdk::rcGetAppUpdateInfo());
  }else if(message.msgId == rmaxSdk::RC_MSGID_FWUPDATE_PROGRESS) {
    if (message.payload) {
      const auto updateResult = std::static_pointer_cast<const std::int32_t>(message.payload);
      std::cout << "RC_MSGID_FWUPDATE_PROGRESS =" << *updateResult << '\n';
    }
  }else if(message.msgId == rmaxSdk::RC_MSGID_CRSF_DATA_RECEIVED) {
    if (message.payload) {
      const auto crsfData = std::static_pointer_cast<const std::vector<std::uint8_t>>(message.payload);
      // std::ostringstream output;
      // output << "CRSF data received, sequence=" << sequence
      //        << ", length=" << crsfData->size() << ": ";
      // for (const std::uint8_t byte : *crsfData) {
      //   output << std::uppercase << std::hex << std::setw(2)
      //          << std::setfill('0') << static_cast<unsigned int>(byte) << ' ';
      // }
      // std::cout << output.str() << '\n';
      // uint8_t crsfPayloadLen = crsfData->at(1);
      // pushCrossfireDataToQueues(crsfData->data() + 1, crsfPayloadLen);
    }
  }else if(message.msgId == rmaxSdk::RC_MSGID_ELRS_MENUITEM_UPDATE){
    if (message.payload) {
      const auto elrsItemUI =std::static_pointer_cast<const rmaxSdk::ElrsSetItemUI>(message.payload);
      std::ostringstream output;
      output << "ElrsMenu:\n"
             << "  idx: " << elrsItemUI->idx << '\n'
             << "  val: " << elrsItemUI->val << '\n'
             << "  type: " << elrsItemUI->type << '\n'
             << "  parentIdx: " << elrsItemUI->parentIdx << '\n'
             << "  itemEnabled: " << std::boolalpha << elrsItemUI->itemEnabled << '\n'
             << "  itemVisible: " << elrsItemUI->itemVisible << '\n'
             << "  name: " << elrsItemUI->name << '\n'
             << "  deviceName: " << elrsItemUI->deviceName << '\n'
             << "  units: " << elrsItemUI->units << '\n'
             << "  options (" << elrsItemUI->options.size() << "):\n";
      for (std::size_t index = 0; index < elrsItemUI->options.size(); ++index) {
        output << "    [" << index << "] " << elrsItemUI->options[index] << '\n';
      }
      std::cout << output.str();
    }
  }else if(message.msgId == rmaxSdk::RC_MSGID_RCTELEMETER_NOTIFY){//遥测通知事件发生
    rmaxSdk::TelemetryNotifyUI telemetryNotifyUI;
    while (RCgetTelemetryItemNotify(telemetryNotifyUI) > 0) {//循环读取每一个遥测通知项
      std::ostringstream output;
      output << "Telemetry Notify:\n"
             << "  hashValue: " << telemetryNotifyUI.hashValue << '\n'
             << "  popupWindow: " << telemetryNotifyUI.popupWindow << '\n'
             << "  triggerType: " << telemetryNotifyUI.triggerType << '\n'
             << "  intervalSec: " << telemetryNotifyUI.intervalSec << '\n'
             << "  triggerValue: " << telemetryNotifyUI.triggerValue << '\n'
             << "  valueUnit: " << telemetryNotifyUI.valueUnit << '\n'
             << "  notifyName: " << telemetryNotifyUI.notifyName << '\n'
             << "  notifyText: " << telemetryNotifyUI.notifyText << '\n'
             << "  audioFile: " << telemetryNotifyUI.audioFile << '\n'
             << "  valTxt: " << telemetryNotifyUI.valTxt << '\n'
             << "  nameTxt: " << telemetryNotifyUI.nameTxt << '\n'
             << "  currValue: " << telemetryNotifyUI.currValue << '\n'
             << "  idx: " << telemetryNotifyUI.idx << '\n'
             << "  showUI: " << telemetryNotifyUI.showUI << '\n';
      std::cout << output.str();
    }
  }else if(message.msgId == rmaxSdk::RC_MSGID_TELEMETRY_LOST){//回传丢失
    std::cout << "Telemetry lost\n";
  }else if(message.msgId == rmaxSdk::RC_MSGID_TELEMETRY_RECOVER){//回传恢复
    std::cout << "Telemetry recover\n";
  }else if(message.msgId == rmaxSdk::RC_MSGID_TELEMETRY_UPDATED){
    printTelemetryList(1);
  }
}

} // namespace

// 验证 SDK 基本行为并运行交互式命令行。
int runRmAxSdkTestConsole(int argc, char *argv[]) {
  CallbackContext callbackContext;

  const std::string_view libraryVersion = rmaxSdk::version();
  if (libraryVersion.empty()) {
    std::cerr << "Failed to read rmAxSdk1 version\n";
    return 1;
  }

  if (rmaxSdk::postMessage({1, std::make_shared<const std::uint8_t>(0x01)})) {
    std::cerr << "Message was accepted before SDK initialization\n";
    return 1;
  }
  if (rmaxSdk::setMessageCallback(onSdkMessage, &callbackContext)) {
    std::cerr << "Callback was set before SDK initialization\n";
    return 1;
  }

  std::cout << "rmAxSdk1 version: " << libraryVersion << '\n';
  std::cout << "Initializing SDK...\n";

  const rmaxSdk::InitResult initialResult = rmaxSdk::initialize(appDataPath);
  bool initialized = initialResult == rmaxSdk::InitResult::Success || initialResult == rmaxSdk::InitResult::AlreadyInitialized;
  std::cout << "Initialization result: " << initResultText(initialResult) << '\n';

  if (initialized && !rmaxSdk::setMessageCallback(onSdkMessage, &callbackContext)) {
    std::cerr << "Callback setup failed after SDK initialization\n";
    rmaxSdk::shutdown();
    return 1;
  }
  if (initialized && !rmaxSdk::setMessageCallback(nullptr)) {
    std::cerr << "Callback clearing failed\n";
    rmaxSdk::shutdown();
    return 1;
  }
  if (initialized && !rmaxSdk::setMessageCallback(onSdkMessage, &callbackContext)) {
    std::cerr << "Callback re-registration failed\n";
    rmaxSdk::shutdown();
    return 1;
  }

  if (initialized && !rmaxSdk::postMessage({2, std::make_shared<const std::vector<std::uint8_t>>(
                                                  std::vector<std::uint8_t>{0x10, 0x20})})) {
    std::cerr << "Message was rejected after SDK initialization\n";
    rmaxSdk::shutdown();
    return 1;
  }

  // CTest uses this mode so automated tests do not wait for console input.
  if (argc > 1 && std::string_view{argv[1]} == "--smoke-test") {
    if (!initialized) {
      return 1;
    }
    rmaxSdk::shutdown();
    std::cout << "SDK smoke test completed successfully\n";
    return 0;
  }

  printHelp();

  CommandContext commandContext{libraryVersion, initialized, {}};
  const std::unordered_map<std::string, CommandHandler> commands{
      {"help", help},
      {"postMsg", postMsg},
      {"saveConfig", saveConfig},
      {"readConfig", readConfig},
      {"getSrcCfg", getSrcCfg},
      {"getChOutCfg", getChOutCfg},
      {"getTelemetry", getTelemetry},
      {"getMixCfg", getMixCfg},
      {"getTimerCfg", getTimerCfg},
      {"getRadioStatus", getRadioStatus},
      {"getCurve", getCurve},
      {"setCurve", setCurve},
      {"applyCurve", applyCurve},
      {"getFwInf", getFwInf},
      {"requestFwInf", requestFwInf},
      {"requestElrsParameters", requestElrsParameters},
      {"requestElrsStatus", requestElrsStatus},
      {"startUpdateFw", startUpdateFw},
      {"getAppInfo", getAppInfo},
      {"updateApp", updateApp},
      {"abortAppUpdate", abortAppUpdate},
      {"getModels", getModels},
      {"createModel", createModel},
      {"setModel", setModel},
      {"exportModel", exportModel},
      {"importModel", importModel},
      {"deleteModel", deleteModelFile},
      {"getTemplates", getTemplates},
      {"modelToTemplate", saveModelAsTemplate},
      {"version", version},
      {"status", status},
      {"init", init},
      {"shutdown", shutdown},
      {"quit", quit},
      {"exit", exit},
  };
  rmaxSdk::taskReadConfigFromDevice();
  std::string commandLine;
  while (std::cout << "rmsdk> " && std::getline(std::cin, commandLine)) {
    std::istringstream input(commandLine);
    std::string command;
    input >> command;
    std::getline(input >> std::ws, commandContext.arguments);
    const auto commandIt = commands.find(command);
    if (commandIt != commands.end()) {
      if (!commandIt->second(commandContext)) {
        break;
      }
    } else if (!command.empty()) {
      std::cout << "Unknown command: " << command << "\nType 'help' for commands.\n";
    }
  }

  if (initialized) {
    std::cout << "Shutting down SDK...\n";
    rmaxSdk::shutdown();
  }

  std::cout << "Goodbye\n";
  return 0;
}
