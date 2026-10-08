/**/
#pragma once
/**/
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace rmaxSdk {
//输入通道配置
struct RcSrcCfgUI {
  std::string srcName;
  int srcIdx; // 输入通道索引
  int srcType;// 输入通道类型
  int isEnable; // 是否使能显示

  int rawVal;
  int rawLogicVal;
  int outputVal;
  int weight;
};
struct RcTrimmingTopKeyUI {
  int trimLV; // 左垂直微调
  int trimLH; // 作水平微调
  int trimRV;
  int trimRH;
  // int topKeyState;
  //AppRadioControl.getSrcTrimData
  //AppRadioControl.modelCfgQ.counterCfg1
};
//输出通道配置
struct RcChOutCfgUI {
  int chIdx;    // 通道索引
  int middle;   // 中点  +-100%
  int min;      // 最小值 +-100%
  int max;      // 最大值 +-100%
  int curveIdx; // 曲线类型
  int mixesNum; // 混控数量

  int weight;   // 权重
  int chOut;    // 通道输出值
  float logicVal; // 逻辑值
  

  int lockChSwIdx;    // 锁定通道开关索引
  int lockChSwVal;    // 锁定通道开关值
  int lockChSwType;   // 锁定通道开关类型
  int lockChVal;      // 锁定通道值

  int dr1SwIdx;       // 双比例1开关索引
  int dr1SwAct;       // 双比例1开关动作
  int dr1Weight;      // 双比例1权重

  int dr2SwIdx;       // 双比例2开关索引
  int dr2SwAct;       // 双比例2开关动作
  int dr2Weight;      // 双比例2权重

  int dr3SwIdx;       // 双比例3开关索引
  int dr3SwAct;       // 双比例3开关动作
  int dr3Weight;      // 双比例3权重
};
// typedef enum _MIXED_MODE: unsigned char{
//   MIXES_REPLACE,      //替换
//   MIXES_ADD,          //叠加
//   MIXES_MULTIP        //相乘
// }MIXED_MODE;
/** Public mixing configuration without Qt dependencies. */
struct RcMixCfgDataUI {
  std::string srcName;
  int srcType = 0;
  int srcIdx = 0;
  int offset = 0;       // 偏移 +-100%
  int curveIdx = 0;     // 曲线
  int swCmpType = 0;    // 使能开关比较条件 0:禁用 1:大于 2:等于 3:小于
  int switchIdx = 0;    // 使能开关
  int swCmpVal = 0;     // 使能开关比较值
  int mixesMode = 0;    // 混控类型  MIXED_MODE
  int chIdx = 0;        // Channel index
  int mixIdx = 0;       // Mix index
  int slowMsUp = 0;     // 上慢放，单位毫秒；0 表示不慢放
  int slowMsDown = 0;   // 下慢放，单位毫秒；0 表示不慢放
  int weightUp = 0;     // 上比例
  int weightDown = 0;   // 下比例
  int mixesCount = 0;   // Number of mixes for this channel
};


// typedef enum _SRC_IDX_DEF: unsigned char{
//     SI_RUD,               //"Rud"  航向
//     SI_ELE,               //"Ele"  升降-俯仰
//     SI_THR,               //"Thr"  油门
//     SI_AIL,               //"Ail"  副翼-横滚
//     SI_POT1,              //"P1"       4: 会回中的旋钮
//     SI_POT2,              //"P2"
//     SI_POT3,              //"P3"
//     SI_POT4,              //"P4"
//     SI_POT5,              //"P5"
//     SI_POT6,              //"P6"
//     SI_POT7,              //"P7"
//     SI_POT8,              //"P8"
    
//     SI_SLIDER1,           //"S1"        12: 不会回中的旋钮
//     SI_SLIDER2,           //"S2"
//     SI_SLIDER3,           //"S3"
//     SI_SLIDER4,           //"S4"
//     SI_SLIDER5,           //"S5"
//     SI_SLIDER6,           //"S6"
//     SI_SLIDER7,           //"S7"
//     SI_SLIDER8,           //"S8"

//     SI_SWA,               //"SA"        20:
//     SI_SWB,               //"SB"
//     SI_SWC,               //"SC"
//     SI_SWD,               //"SD"
//     SI_SWE,               //"SE"
//     SI_SWF,               //"SF"
//     SI_SWG,               //"SG"
//     SI_SWH,               //"SH"
//     SI_SWI,               //"SI"
//     SI_SWJ,               //"SJ"
//     SI_SWK,               //"SK"
//     SI_SWL,               //"SL"
//     SI_SWM,               //"SM"
//     SI_SWN,               //"SN"
//     SI_SWO,               //"SO"
//     SI_SWP,               //"SP"

//     SI_CNT1,              //COUNTER 1     36:
//     SI_CNT2,              //COUNTER 2
//     SI_CNT3,              //COUNTER 3
//     SI_CNT4,              //COUNTER 4
//     SI_CNT5,              //COUNTER 5
//     SI_CNT6,              //COUNTER 6
    
//     SI_END
// }SRC_IDX_DEF;
struct RcSysCfgDataUI {
  uint8_t  fsMode;     //失控保护模式
  // 0 没有失控保护
  // 1 Hold – The receiver keeps channel values at their last received state from the transmitter.
  // 2 No pulses – No PWM pulses are output.
  // 3 Custom – The receiver changes the channel values to the custom set values.
  int8_t  fsChVal[16];//自定义失控时的通道输出值 范围 +- 100%
  uint8_t stickMode;  //摇杆模式 1 或   2
  //预飞行检查 通道值预设值  127=不检查  +-100 为预设检查值 如果通道与预设值相差超过一定值那么认定通道不在预设位置，将发出警报
  int8_t   preFlightCheckChannel[36];//长度SI_SWP+1      类型: SRC_IDX_DEF 
  //int8_t   srcInWeight[SRC_LEGNTH];  //输入权重 可用于反向 默认为100 范围 +-100
  uint8_t  rfModuleMode; /* 0:内部ELRS 1:外部ELRS 2: 0xff 关闭*/
  uint8_t  alramVtg;     /* 报警电压 单位 0.1v*/
  uint8_t  joytickBeep;  /* 摇杆蜂鸣器 */
  uint8_t  modelMatchId; /* ELRS 的MODELMATCH编号 */
  uint8_t  crsfBaudRate;      /* 1:400K 2:921600 3:1870000 */
};
// /* 计数时提示音 */
// typedef enum _RCTIME_COUNTDOWN: unsigned char{
//     TCD_SILENT,         //安静
//     TCD_BEEPS,          //蜂鸣器
//     TCD_VOICE,          //语言
//     TCD_HAPTIC,         //震动
//     TCD_BEEPS_HAPTIC,   //蜂鸣器和震动
//     TCD_VOICE_HAPTIC,   //语言和震动
//     TCD_NOTFY_RESET = 0x81      //定时器被复位
// }RCTIME_COUNTDOWN;
/** 定时器配置结构体 */
struct RcTimeCfgDataUI {
  int mode = 0;
  int swCmpType = 0;    // 使能开关比较条件 0:禁用 1:大于 2:等于 3:小于 
  int switchIdx = 0;    // 开关索引
  int switchAct = 0;    // 开关动作
  int startTime = 0;     // 0 为正计时，大于 0 为倒计时周期
  int direction = 0;    // 计时方向 1: 正计时 0: 倒计时
  int countdown = 0;    //计数时提示模式  RCTIME_COUNTDOWN
  int beepTimeSec = 0;   // 提示间隔，例如 5、10、20、30 秒
  int rstSwCmpType = 0;    // 重置开关比较条件 0:禁用 1:大于 2:等于 3:小于 
  int resetSwIdx = 0;     // 重置开关索引
  int resetSwAct = 0;     // 重置开关动作
  int cmpValue = 0;      // 通道激活模式下的输入比较值
  int cmpType = 0;       // 0 禁用，1 大于，2 等于，3 小于

  int loopMode = 0;       // 循环模式
  int outputMode = 0;    // 输出模式，根据阈值在 outputMin 和 outputMax 间映射
  int outputMin = 0;     // 百分比，范围 +-100
  int outputMax = 0;      // 百分比，范围 +-100
  int threshold = 0;      // 比较器阈值

  int timeIdx = 0;
  int trDirection = 0;
  int trStartTime = 0;   // 0 为正计时，大于 0 为倒计时
  int timeCount = 0;     // 当前计数器值
  int notifyCode = 0;    // 提示信息
};

// 定时器或计数器运行状态报告。
struct RcTimeReportUI {
  int timeIdx = 0;    // 定时器或计数器的组内索引
  int direction = 0;  // 0: 显示剩余时间，1: 显示已用时间
  std::int32_t startTime = 0; // 启动时的时间 0就是正计时   大于0 是倒计时
  std::int32_t timeCount = 0; // 定时器为秒数，计数器为当前计数值
};

// 定时器提示事件。
struct RcTimeNotifyUI {
  uint8_t  timeIdx;     // 定时器索引
  uint8_t  isRunning;    // 是否正在运行
  uint8_t  notifyCode;   // 提示代码
  std::int32_t timeCount = 0;    // 当前计数器值
};

struct RcChCfgDrUI{
    RcChCfgDrUI() : chIdx(0), idx(0), drCount(0), swCmpType(0), swSrcIdx(0), swCmpVal(0), drWeight(0) {}
    int chIdx;       //通道编号
    int idx;         //DR编号
    int drCount;     //DR数量
    int swCmpType;   //使能开关比较条件      0:禁用 1:大于 2:等于 3:小于
    int swSrcIdx;    //使能开关
    int swCmpVal;    //使能开关比较值
    int drWeight;    // DR权重
};
// typedef enum _IN_CURVE_TYPE: unsigned char{
//     ICT_NONE=0,
//     ICT_DIFF,   //大于0的时候控制0到-100的比例  大于0的时候控制0到+100的比例
//     ICT_EXPO,   //曲线
//     ICT_FUNC,   //函数
//     ICT_CSTM,   //带点数的曲线
// }IN_CURVE_TYPE;
struct RcCurveCfgDataUI{
  int idx;    // 曲线索引
  int cType;  // 曲线类型 IN_CURVE_TYPE
  int ptNum;  // 曲线点数量
  int xVal[12];
  int yVal[12];
};
// 遥控器模型数据结构
struct RcModelDataUI{
  std::string modelName;
  std::string modelImg;
  std::string filePath;
  int64_t    modelIdx;
  uint32_t    modelMatch;
};

/*  固件信息数据结构 */
struct FirmwareInfUI{
    uint32_t SysId;       //32位系统ID编号
    uint32_t DevId;       //32位设备唯一ID
    uint32_t FlyTime;     //总的飞行时间，单位秒
    uint32_t BuildTime;   //固件构建日期,格式 YYMMDD
    uint32_t VER_FW;      //32位固件版本号，格式为 XX.YY.ZZZZ
        //例如:0x01020003 对应于 01.02.0003
    uint32_t  VER_HW;     //32位硬件版本号，格式为 XX.YY
                      //例如:0x00000102 对应于 01.02
    std::string  DevText;  //32字节的设备描述字符串
    std::string  FwVerText; // 固件版本字符串
    std::string  BulidTimeText; // 固件构建时间文本
    std::string  cacheFwFileVerTxt; // 下载的缓存固件版本字符串
};
// APP 更新信息数据结构
struct AppUpdateInfoUI{//APP 更新状态
  uint32_t downloadBytesReceived; // 已下载的字节数
  uint32_t downloadBytesTotal;    // 总的字节数
  std::string apkUrl;
  std::string version;
  std::string cnUpdateContent;
  std::string enUpdateContent;
  std::string cacheAppFile; 
};
// ELRS 设置项数据结构
struct ElrsSetItemUI{
    int idx;    // index of the item
    int val;    // value of the item
    int type;   // type of the item
    int parentIdx; // index of the parent item
    bool itemEnabled; // whether the item is enabled
    bool itemVisible; // whether the item is visible

    std::string name; // name of the item
    std::vector<std::string> options; // selection options for the item
    std::string deviceName; // name of the device
    std::string units; // units of the item
};
// ELRS 设置结束应答数据结构
struct ElrsSetEndAckUI{
  int type;
  int status;
  int lastSetIdx;
  std::string options; 
};
// ELRS 状态信息数据结构
struct ElrsStatusInfoUI{
    int badPkt;
    int goodPkt;
    int modelMismatch;
};
// 回传数据结构
struct TelemetryDataUI {
  double lat, lng;         /* 回传位置 */
  double pitch, roll, yaw; /* 回传姿态 */
  double voltage, current; /* 回传电池信息 */
};
//遥控器的一些基础状态
struct RadioStatusUI {
  int rfLQ = 0;        // 接收机的信号质量  由接收机传回
  int rfLDBM = 0;      // 接收机的信号质量  由接收机传回
  int rfLLostCnt = 0;  // 接收机数据丢失计数器=0 表示接收机丢失
  float rcVtg = 0;     // 遥控器电压
  float rxVtg = 0;     // 飞机或者接收机回传电压
  int topKeyState = 0; // 顶部按键状态
  int adcSetState = 0; // ADC 设置校准状态
};
// 回传通知数据结构
struct TelemetryNotifyUI {
  uint64_t hashValue;     // 关联的回传项目哈希值
  int popupWindow;        // 是否太弹窗
  int triggerType;        // 触发类型  1大于，2等于，3小于
  int intervalSec;        // 通知间隔时间单位为秒
  double triggerValue;    // 触发值
  int valueUnit;          // 值的单位
  std::string notifyName; // 回传项目的名称
  std::string notifyText; // 通知文本
  std::string audioFile;  // 音频文件
  std::string valTxt;
  std::string nameTxt;

  double currValue; // 值
  int idx;
  int showUI; // 是否显示UI
};
// 回传项目数据结构
struct TelemetryItemUI{
    uint64_t      hashValue;      //关联的回传项目哈希值
    std::string   notifyName;
    std::string   notifyText;  //通知文本
    int       popupWindow;  //是否太弹窗
    int       triggerType;  //触发类型  1大于，2等于，3小于
    int       intervalSec;  //通知间隔时间单位为秒
    double    triggerValue;  //触发值
    double    currValue;  //值
    std::string   audioFile;  //音频文件
    int       idx;
    std::string   valTxt;
    int       showUI;
    std::string   nameTxt;
    int       valueUnit;
};

/** SDK output message identifiers. */
enum RcMessageId : std::uint32_t {
  RC_MSGID_LVTG_NOTIFY        = 0x08000001,
  RC_MSGID_PFC_NOTIFY,  // PFC通知  std::vector<int32_t>[32]
  RC_MSGID_TIM_REPORT,  // 定时器报告 RcTimeReportUI
  RC_MSGID_CNT_REPORT,  // 计数器报告 RcTimeReportUI
  RC_MSGID_TIM_NOTIFY,  // 定时器通知 RcTimeNotifyUI
  RC_MSGID_TRIM_NOTIFY,  // 修剪通知
  RC_MSGID_SRCINPUT_UPDATE,  // 输入源更新
  RC_MSGID_MIXOUT_UPDATE,  // 混合输出更新
  RC_MSGID_CFGTODEV_END,  // 配置发送到设备结束
  RC_MSGID_GETDEVCFG_END,  // 获取设备配置结束
  RC_MSGID_RCTELEMETER_NOTIFY,  // 遥测通知
  RC_MSGID_APPINFO_UPDATED,  // APP固件信息被更新
  RC_MSGID_APPDOWNLOAD_PROGRESS,  // APP下载进度
  RC_MSGID_APPDOWNLOAD_END,  // APP下载完成
  RC_MSGID_FWVERSION_NOT_MATCH,  // 固件版本不匹配
  RC_MSGID_FWINF_UPDATED,  // 固件信息更新
  RC_MSGID_FWUPDATE_PROGRESS,  // 固件更新进度通知 int32_t: -1:下载固件失败  -2:固件更新失败 0-100:更新进度
  RC_MSGID_FWFILE_DOWNLOAD_END,  // 固件下载进度
  RC_MSGID_ELRS_SET_ACK,  // ELRS设置结束   ElrsSetEndAckUI
  RC_MSGID_ELRS_STATUS,  // ELRS状态信息  ElrsStatusInfoUI
  RC_MSGID_ELRS_TELEMETRY,  // ELRS遥测数据
  RC_MSGID_ELRS_DEVINF_UPDATE,  // ELRS设备信息更新    char deviceName[]
  RC_MSGID_ELRS_MENUITEM_UPDATE,  // ELRS菜单项更新
  RC_MSGID_CRSF_DATA_RECEIVED,  // CRSF数据接收   std::vector<std::uint8_t>
  RC_MSGID_TELEMETRY_UPDATED,  //回传被更新
  RC_MSGID_TELEMETRY_LOST,  //回传丢失
  RC_MSGID_TELEMETRY_RECOVER,  //回传恢复
};

/** Message exchanged across the SDK boundary. */
struct SdkMessage {
  std::uint32_t msgId;
  std::shared_ptr<const void> payload;
};

} // namespace rmaxSdk
