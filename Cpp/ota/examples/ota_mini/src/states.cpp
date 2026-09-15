/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/states.cpp
 * @Brief: 各状态的具体实现
 *
 * 对照真实工程：
 *   Init               <-> state_machine/init.cpp                （断点续升级）
 *   Idle               <-> state_machine/idle.cpp                （Waiter 等任务）
 *   CheckPrerequisites <-> state_machine/check_prerequisites.cpp （电量/挂起/线程池）
 *   Download           <-> state_machine/download.cpp            （curl + 进度回调 + 解压）
 *   ParsingImg         <-> state_machine/parsing_img.cpp         （任务列表 + 文件过滤）
 *   CheckOtaTask       <-> state_machine/check_ota_task.cpp       （任务分发）
 *   SendImgData        <-> state_machine/send_img_data.cpp        （线程池并行升级）
 *   NxSelf             <-> state_machine/nx_self.cpp              （执行脚本）
 *   RkSendOtaCmd       <-> state_machine/rk_send_ota_cmd.cpp      （EtherCAT 电机）
 *   Success            <-> state_machine/success.cpp              （index++ / 下一任务）
 *   Failed             <-> state_machine/failed.cpp                （等 /ota_reset）
 *   Reboot             <-> state_machine/reboot.cpp                （写版本号 + 重启策略）
 *   PubHardReboot      <-> state_machine/pub_hard_reboot.cpp
 ***************************************************************/

#include "states.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>

#include "fs_utils.hpp"
#include "log.hpp"
#include "shell_command.hpp"

namespace ota_mini::state_machine {
namespace {

// 模拟"升级包"的内容：为演示，在临时目录里造出一棵 img 树
// 注意 devices 里的设备是串口设备，这里再加一层模拟：升级包内固件文件数 = 设备数
void createFakePackage(const Paths& paths) {
  fs::ensureDir(paths.imgDir() / "nx_mcu");
  fs::ensureDir(paths.imgDir() / "rk_motor");
  fs::ensureDir(paths.imgDir() / "rk_self");
  fs::ensureDir(paths.imgDir() / "nx_self");

  const auto write = [](const std::filesystem::path& p, const std::string& content) {
    std::ofstream ofs(p);
    ofs << content;
  };
  write(paths.imgDir() / "nx_mcu" / "mcu_v1.0.3.bin", "fake mcu firmware");
  write(paths.imgDir() / "rk_motor" / "motor_v2.1.0.bin", "fake motor firmware");
  write(paths.imgDir() / "rk_motor" / "readme.txt", "not a firmware");
  write(paths.imgDir() / "rk_self" / "rk_app_v3.0.0.deb", "fake rk deb");
  write(paths.imgDir() / "nx_self" / "nx_app_v3.0.1.deb", "fake nx deb");
}

}  // namespace

// ===========================================================================
// 状态工厂：对应 ota.cpp 里的 state_machine_list
// ===========================================================================
std::unique_ptr<StateMachineFunction> createState(StateMachineType type) {
  switch (type) {
    case StateMachineType::INIT: return std::make_unique<Init>();
    case StateMachineType::IDLE: return std::make_unique<Idle>();
    case StateMachineType::CHECK_PREREQUISITES: return std::make_unique<CheckPrerequisites>();
    case StateMachineType::DOWNLOAD: return std::make_unique<Download>();
    case StateMachineType::PARSING_IMG: return std::make_unique<ParsingImg>();
    case StateMachineType::CHECK_OTA_TASK: return std::make_unique<CheckOtaTask>();
    case StateMachineType::SEND_IMG_DATA: return std::make_unique<SendImgData>();
    case StateMachineType::NX_SELF: return std::make_unique<NxSelf>();
    case StateMachineType::RK_SEND_OTA_CMD: return std::make_unique<RkSendOtaCmd>();
    case StateMachineType::SUCCESS: return std::make_unique<Success>();
    case StateMachineType::FAILED: return std::make_unique<Failed>();
    case StateMachineType::REBOOT: return std::make_unique<Reboot>();
    case StateMachineType::PUB_HARD_REBOOT: return std::make_unique<PubHardReboot>();
  }
  return nullptr;   // 调用方必须判空（工厂可能失败）
}

// ===========================================================================
// Init：读机型/配置，从磁盘恢复上次中断的状态（断点续升级）
// ===========================================================================
void Init::run() {
  auto& d = data();
  d.paths.work_dir = d.configures.work_dir;

  OTA_LOG_INFO("配置文件内容: {}", config::describe(d.configures));
  OTA_LOG_INFO("工作目录: {}", d.paths.workDir().string());

  auto last = readStateMachineTypeFromFile();
  OTA_LOG_INFO("从文件恢复的状态: {}", toString(last, true));

  StateMachineType next = StateMachineType::IDLE;

  switch (last) {
    case StateMachineType::INIT:
    case StateMachineType::IDLE: {
      next = StateMachineType::IDLE;
      break;
    }
    case StateMachineType::CHECK_PREREQUISITES: {
      const auto upgrade_type = readUpgradeTypeFromFile();
      if (upgrade_type == UpgradeType::UNKNOWN) {
        OTA_LOG_WARN("升级类型文件已失效，回到 IDLE");
        next = StateMachineType::IDLE;
      } else {
        d.setUpgradeType(upgrade_type);
        OTA_LOG_INFO("恢复升级类型: {}，继续前置检查", toString(upgrade_type));
        next = StateMachineType::CHECK_PREREQUISITES;
      }
      break;
    }
    case StateMachineType::PARSING_IMG:
    case StateMachineType::DOWNLOAD: {
      d.setUpgradeType(readUpgradeTypeFromFile());
      next = last == StateMachineType::DOWNLOAD ? StateMachineType::DOWNLOAD : StateMachineType::PARSING_IMG;
      break;
    }
    case StateMachineType::CHECK_OTA_TASK:
    case StateMachineType::SEND_IMG_DATA:
    case StateMachineType::NX_SELF:
    case StateMachineType::RK_SEND_OTA_CMD: {
      auto task_list = readTaskTypeListFromFile();
      const int index = readTaskIndexFromFile();
      if (task_list.empty() || index >= static_cast<int>(task_list.size())) {
        OTA_LOG_WARN("磁盘上的任务列表已失效，回到 IDLE");
        next = StateMachineType::IDLE;
      } else {
        d.task_type_list = std::move(task_list);
        d.current_task_index.store(index);
        OTA_LOG_INFO("恢复断点: 第 {}/{} 个任务，继续从 CHECK_OTA_TASK 开始", index, d.task_type_list.size());
        next = StateMachineType::CHECK_OTA_TASK;
      }
      break;
    }
    case StateMachineType::SUCCESS:
    case StateMachineType::REBOOT:
    case StateMachineType::PUB_HARD_REBOOT: {
      // 升级其实已经完成，只是没来得及收尾 -> 直接进 REBOOT 收尾更合理
      next = StateMachineType::REBOOT;
      break;
    }
    case StateMachineType::FAILED: {
      OTA_LOG_WARN("上次是失败状态，回到 IDLE 重新等待任务");
      next = StateMachineType::IDLE;
      break;
    }
  }

  setNextStateMachineType(next, ota_state::IDLE, "初始化完成");
}

// ===========================================================================
// Idle：阻塞等待任务（不是轮询！）
// ===========================================================================
void Idle::run() {
  auto& d = data();
  d.thread_pool.reset();   // 释放上次升级的线程池

  auto condition = [&d]() -> bool {
    if (!d.running.load()) {
      return true;                     // 进程退出
    }
    if (d.getIsReceiveTask()) {
      return true;                     // 收到任务
    }
    return false;
  };

  OTA_LOG_INFO("进入 IDLE，等待升级任务（可以输入 local/cloud 命令触发）");

  while (d.running.load()) {
    const auto ret = d.idle_task_waiter.wait(condition, std::chrono::seconds(5));
    if (ret == ErrorCode::OK) {
      break;
    }
    OTA_LOG_DEBUG("等待任务超时（5s），继续等待……");
  }

  if (!d.running.load()) {
    OTA_LOG_INFO("进程终止");
    return;                            // 基类析构会跳过状态推进
  }

  // 把升级类型 / URL / 版本号落盘，掉电后可恢复
  writeUpgradeType(d.getUpgradeType());
  const auto url = d.getPackageUrl().empty() ? std::string("UNKNOWN") : d.getPackageUrl();
  fs::atomicWriteFile(d.paths.packageUrlFile(), url);
  const auto version = d.getOtaVersion().empty() ? std::string("UNKNOWN") : d.getOtaVersion();
  fs::atomicWriteFile(d.paths.versionFile(), version);

  OTA_LOG_INFO("收到任务: 升级类型 = {}, 版本 = {}", toString(d.getUpgradeType()), version);

  setNextStateMachineType(StateMachineType::CHECK_PREREQUISITES, ota_state::PREPARE, "前置检查");
}

// ===========================================================================
// CheckPrerequisites：电量 / 挂起状态 / 创建线程池
// ===========================================================================
void CheckPrerequisites::run() {
  auto& d = data();
  const int battery = d.battery_percentage.load();

  OTA_LOG_INFO("开始前置检查: 电量 = {}%, 阈值 = {}%", battery, d.configures.battery_limit);
  if (battery < d.configures.battery_limit) {
    OTA_LOG_ERROR("电量不足，无法升级");
    say("电量不足，请先充电");
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "电量不足");
    return;
  }

  // 挂起状态检查：真实工程用 static 局部变量做重试计数（每次进入状态不会重置）
  if (d.configures.prepare_befor_ota) {
    static int count = 0;
    if (!d.is_hang_up.load()) {
      if (++count >= d.configures.prepare_befor_ota_time) {
        count = 0;
        OTA_LOG_ERROR("等待机器人挂起超时");
        setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "等待挂起超时");
        return;
      }
      OTA_LOG_WARN("机器人尚未挂起，1 秒后重试（第 {}/{} 次）", count, d.configures.prepare_befor_ota_time);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      setNextStateMachineType(StateMachineType::CHECK_PREREQUISITES, ota_state::PREPARE, "等待挂起");
      return;
    }
    count = 0;
  }

  // 创建线程池（真实工程在 Idle/CheckPrerequisites 里创建）
  d.thread_pool = std::make_shared<ThreadPool>(static_cast<std::size_t>(d.configures.thread_pool_size));

  say("请勿切断设备电源或关机");

  if (d.getUpgradeType() == UpgradeType::CLOUD) {
    setNextStateMachineType(StateMachineType::DOWNLOAD, ota_state::DOWNLOAD_START, "开始下载升级包");
  } else {
    setNextStateMachineType(StateMachineType::PARSING_IMG, ota_state::UPDATING, "开始升级");
  }
}

// ===========================================================================
// Download：模拟 curl 下载（带进度回调）+ 解压
// ===========================================================================
bool Download::simulateDownload(const std::string& url, const std::string& save_path) {
  OTA_LOG_INFO("开始下载: {} -> {}", url, save_path);

  constexpr int kTotalChunks = 10;
  std::string content;
  for (int i = 1; i <= kTotalChunks; ++i) {
    content += "chunk" + std::to_string(i) + ";";
    std::this_thread::sleep_for(std::chrono::milliseconds(40));   // 模拟网络耗时

    const int progress = i * 100 / kTotalChunks;
    auto& d = data();
    d.ota_state_msg.download_progress = progress;
    publishStatus();                                              // 对应进度回调里发 topic
    OTA_LOG_INFO("下载进度: {}%", progress);
  }

  if (auto ret = fs::atomicWriteFile(save_path, content); !ret.has_value()) {
    return false;
  }
  OTA_LOG_INFO("下载完成，大小 {} 字节", content.size());
  return true;
}

void Download::run() {
  auto& d = data();
  const std::string url = d.getPackageUrl();
  const auto save_path = d.paths.packageFile();

  fs::ensureDir(d.paths.downloadDir());
  if (std::filesystem::exists(save_path)) {
    OTA_LOG_INFO("旧升级包已存在，先删除: {}", save_path.string());
    std::filesystem::remove(save_path);
  }

  if (!simulateDownload(url, save_path.string())) {
    OTA_LOG_ERROR("下载失败");
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "下载失败");
    return;
  }

  // "解压"：真实工程执行 unzip -q -o img.zip -d <dir>
  fs::removeDirIfExists(d.paths.imgDir());
  createFakePackage(d.paths);
  OTA_LOG_INFO("解压完成: {}", d.paths.imgDir().string());

  d.ota_state_msg.download_progress = 100;
  setNextStateMachineType(StateMachineType::PARSING_IMG, ota_state::DOWNLOAD_SUCCESS, "下载完成，开始解析镜像");
}

// ===========================================================================
// ParsingImg：按升级类型展开任务列表，并过滤掉升级包里没有的模块
// ===========================================================================
std::string ParsingImg::artifactDir(TaskType type) const {
  const auto& d = data();
  switch (type) {
    case TaskType::NX_SERIAL:   return (d.paths.imgDir() / "nx_mcu").string();
    case TaskType::RK_ETHERCAT: return (d.paths.imgDir() / "rk_motor").string();
    case TaskType::RK_SELF:     return (d.paths.imgDir() / "rk_self").string();
    case TaskType::TASK_NX_SELF:     return (d.paths.imgDir() / "nx_self").string();
    // RK 串口设备固件在本例程的升级包里不存在 -> 返回空目录，
    // 于是 hasArtifacts() 为 false，ParsingImg 会“跳过该流程”（正好演示这个分支）
    case TaskType::RK_SERIAL:   return "";
    case TaskType::UNKNOWN:     return "";
  }
  return "";
}

std::string ParsingImg::artifactExtension(TaskType type) const {
  switch (type) {
    case TaskType::NX_SERIAL:
    case TaskType::RK_ETHERCAT:
    case TaskType::RK_SERIAL:
      return ".bin";
    case TaskType::RK_SELF:
    case TaskType::TASK_NX_SELF:
      return ".deb";
    case TaskType::UNKNOWN:
      return "";
  }
  return "";
}

// 对应 parsing_img.cpp 的 hasUpgradeArtifacts
bool ParsingImg::hasArtifacts(TaskType type) const {
  const std::string dir = artifactDir(type);
  const std::string ext = artifactExtension(type);
  if (dir.empty() || ext.empty()) {
    return false;
  }
  return !fs::findFilesWithExtension(dir, ext).empty();
}

std::vector<TaskType> ParsingImg::makeTaskList() {
  auto& d = data();
  std::vector<TaskType> list;

  switch (d.getUpgradeType()) {
    case UpgradeType::LOCAL_RK_MOTOR:
      list = {TaskType::RK_ETHERCAT};
      break;
    case UpgradeType::LOCAL_RK_MCU:
      list = {TaskType::NX_SERIAL};
      break;
    case UpgradeType::LOCAL_RK_SELF:
      list = {TaskType::RK_SELF};
      break;
    case UpgradeType::LOCAL_NX_SELF:
      list = {TaskType::TASK_NX_SELF};
      break;
    case UpgradeType::LOCAL_ALL:
    case UpgradeType::CLOUD: {
      // 按 yaml 里的 upgrade_sequence 顺序展开，并跳过包内不存在的模块
      for (const auto& name : d.configures.upgrade_sequence) {
        const auto type = stringToTaskType(name);
        if (type == TaskType::UNKNOWN) {
          OTA_LOG_WARN("配置里存在无法识别的任务类型: {}", name);
          continue;
        }
        if (!hasArtifacts(type)) {
          OTA_LOG_WARN("升级包里没有 {} 对应的固件，跳过该流程", name);
          continue;
        }
        list.push_back(type);
      }
      break;
    }
    case UpgradeType::UNKNOWN:
      break;
  }
  return list;
}

void ParsingImg::run() {
  auto& d = data();

  // 本地升级时，认为用户已经把升级文件放到了约定目录（真实工程靠手动拷贝）
  if (!fs::dirHasAnyFile(d.paths.imgDir())) {
    OTA_LOG_INFO("本地升级：准备模拟升级包目录 {}", d.paths.imgDir().string());
    createFakePackage(d.paths);
  }

  d.task_type_list = makeTaskList();
  if (d.task_type_list.empty()) {
    OTA_LOG_ERROR("没有任何可执行的升级任务，请检查升级包内容");
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "没有可执行的升级任务");
    return;
  }

  OTA_LOG_INFO("解析完成，共 {} 个升级任务:", d.task_type_list.size());
  for (std::size_t i = 0; i < d.task_type_list.size(); ++i) {
    OTA_LOG_INFO("  [{}] {}", i, toString(d.task_type_list[i], true));
  }

  d.current_task_index.store(0);
  d.ota_state_msg.pkg_current = 0;
  d.ota_state_msg.pkg_total = static_cast<int>(d.task_type_list.size());
  writeTaskTypeList(d.task_type_list);
  writeTaskIndex(0);

  setNextStateMachineType(StateMachineType::CHECK_OTA_TASK, ota_state::UPDATING, "解析镜像完成");
}

// ===========================================================================
// CheckOtaTask：任务分发
// ===========================================================================
bool CheckOtaTask::sendRkImageToRk3588() {
  auto& d = data();
  if (d.is_had_send_rk_img.load()) {
    return true;   // 幂等：同一个升级流程里只推一次
  }
  OTA_LOG_INFO("把 NX 上的升级包推送到 RK3588（对应 scripts 里的 scp/rsync 脚本）");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  d.is_had_send_rk_img.store(true);
  return true;
}

void CheckOtaTask::run() {
  auto& d = data();

  if (d.current_task_index.load() >= static_cast<int>(d.task_type_list.size())) {
    OTA_LOG_ERROR("任务索引越界: {} >= {}", d.current_task_index.load(), d.task_type_list.size());
    setNextStateMachineType(StateMachineType::IDLE, ota_state::IDLE, "任务索引越界");
    return;
  }

  const auto task = d.task_type_list[static_cast<std::size_t>(d.current_task_index.load())];
  OTA_LOG_INFO("执行第 {}/{} 个任务: {}", d.current_task_index.load() + 1, d.task_type_list.size(),
               toString(task, true));

  switch (task) {
    case TaskType::NX_SERIAL: {
      // 真实工程：先 STOP_RELATED_NODE，再 SEND_IMG_DATA
      setNextStateMachineType(StateMachineType::SEND_IMG_DATA, ota_state::UPDATING, "开始串口升级");
      break;
    }
    case TaskType::RK_SERIAL: {
      // 正常流程里 RK_SERIAL 会因为升级包内没有固件而被 ParsingImg 过滤掉，
      // 真走到这里说明升级包内容异常。
      OTA_LOG_ERROR("升级包里没有 RK 串口固件，任务不应出现");
      setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "RK 串口固件缺失");
      break;
    }
    case TaskType::RK_ETHERCAT:
    case TaskType::RK_SELF: {
      if (!sendRkImageToRk3588()) {
        setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "推送镜像到 RK3588 失败");
      } else {
        setNextStateMachineType(StateMachineType::RK_SEND_OTA_CMD, ota_state::UPDATING, "开始 RK 升级");
      }
      break;
    }
    case TaskType::TASK_NX_SELF: {
      setNextStateMachineType(StateMachineType::NX_SELF, ota_state::UPDATING, "开始 NX 自升级");
      break;
    }
    case TaskType::UNKNOWN: {
      OTA_LOG_ERROR("不支持的任务类型");
      setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "不支持的任务类型");
      break;
    }
  }
}

// ===========================================================================
// SendImgData：线程池并行给多台串口设备发固件
// ===========================================================================
void SendImgData::run() {
  auto& d = data();

  if (!d.thread_pool) {
    OTA_LOG_ERROR("线程池未初始化");
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "线程池未初始化");
    return;
  }

  // 模拟"设备"：如果 config 没配设备，就按升级包里的固件数量造几个
  auto devices = d.configures.devices;
  if (devices.empty()) {
    const auto bins = fs::findFilesWithExtension(d.paths.imgDir() / "nx_mcu", ".bin");
    int index = 0;
    for (std::size_t i = 0; i < bins.size(); ++i) {
      config::Device dev;
      dev.name = "nx_mcu_" + std::to_string(index);
      dev.port = "/dev/ttyTHS" + std::to_string(index);
      dev.baud_rate = 921600;
      dev.should_fail = false;
      devices.push_back(dev);
      ++index;
    }
    if (devices.empty()) {
      OTA_LOG_WARN("没有可升级的串口设备，直接视为成功");
      setNextStateMachineType(StateMachineType::SUCCESS, ota_state::UPDATING, "无串口设备");
      return;
    }
  }

  std::vector<std::future<bool>> futures;
  futures.reserve(devices.size());
  for (const auto& device : devices) {
    // 关键：按值捕获 device，按引用会全部指向同一个悬空对象
    futures.push_back(d.thread_pool->enqueue([device, work_dir = d.paths.workDir().string()]() -> bool {
      OTA_LOG_INFO("  [{}] 开始升级 {} @ {}bps", device.port, device.name, device.baud_rate);
      std::this_thread::sleep_for(std::chrono::milliseconds(60));
      if (device.should_fail) {
        OTA_LOG_ERROR("  [{}] 固件写入校验失败", device.port);
        return false;
      }
      // 每个设备写自己的文件：多线程写同一个文件会因为 tmp 名字冲突而失败
      fs::atomicWriteFile(std::filesystem::path(work_dir) / ("upgraded_" + device.name + ".txt"),
                          device.name + " ok\n");
      OTA_LOG_INFO("  [{}] 升级成功", device.port);
      return true;
    }));
  }

  bool all_success = true;
  for (auto& future : futures) {
    if (!future.get()) {
      all_success = false;      // 不提前 return，保证等到所有任务
    }
  }

  if (all_success) {
    setNextStateMachineType(StateMachineType::SUCCESS, ota_state::UPDATING, "串口升级成功");
  } else {
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "串口升级失败");
  }
}

// ===========================================================================
// NxSelf：执行 NX 自升级脚本
// ===========================================================================
void NxSelf::run() {
  auto& d = data();
  const auto script = d.paths.workDir() / "upgrade_nx_self.sh";
  shell_command::createFakeScript(script.string(), 0, "install nx deb packages done");

  std::string output;
  const auto ret = shell_command::execute(script.string(), "", &output);
  if (ret != ErrorCode::OK) {
    OTA_LOG_ERROR("NX 自升级脚本执行失败");
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "NX 自升级失败");
    return;
  }
  setNextStateMachineType(StateMachineType::SUCCESS, ota_state::UPDATING, "NX 自升级完成");
}

// ===========================================================================
// RkSendOtaCmd：RK3588 侧升级（EtherCAT 电机 / 自升级）
// ===========================================================================
bool RkSendOtaCmd::upgradeEthercatMotors() {
  auto& d = data();

  const auto motors = fs::findFilesWithExtension(d.paths.imgDir() / "rk_motor", ".bin");
  if (motors.empty()) {
    OTA_LOG_ERROR("没有找到电机固件");
    return false;
  }

  // 配置里的 should_upgrade_motor_size 语义：0 表示全部升级
  const int expected = static_cast<int>(motors.size());
  OTA_LOG_INFO("在线电机数量 = {}, 待升级固件数 = {}", expected, motors.size());

  if (!d.thread_pool) {
    OTA_LOG_ERROR("线程池未初始化");
    return false;
  }

  std::vector<std::future<bool>> futures;
  for (std::size_t i = 0; i < motors.size(); ++i) {
    futures.push_back(d.thread_pool->enqueue([i]() -> bool {
      OTA_LOG_INFO("  电机 {} 升级中……", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      OTA_LOG_INFO("  电机 {} 升级完成", i);
      return true;
    }));
  }
  bool all_ok = true;
  for (auto& f : futures) {
    if (!f.get()) {
      all_ok = false;
    }
  }
  return all_ok;
}

void RkSendOtaCmd::run() {
  auto& d = data();
  const auto task = d.task_type_list[static_cast<std::size_t>(d.current_task_index.load())];

  bool ok = false;
  if (task == TaskType::RK_ETHERCAT) {
    OTA_LOG_INFO("升级 RK EtherCAT 电机固件");
    ok = upgradeEthercatMotors();
  } else if (task == TaskType::RK_SELF) {
    OTA_LOG_INFO("升级 RK3588 自身（deb 安装）");
    const auto script = d.paths.workDir() / "upgrade_rk_self.sh";
    shell_command::createFakeScript(script.string(), 0, "install rk deb packages done");
    ok = shell_command::execute(script.string()) == ErrorCode::OK;
    if (ok) {
      // 对应真实工程 rk_wait_ota_finish.cpp：校验安装结果 + 恢复控制服务
      const auto check_script = d.paths.workDir() / "check_rk_install.sh";
      shell_command::createFakeScript(check_script.string(), 0, "check rk deb install ok");
      ok = shell_command::execute(check_script.string()) == ErrorCode::OK;
      if (!ok) {
        OTA_LOG_ERROR("RK 自升级安装校验失败");
      }
      OTA_LOG_INFO("RK 控制服务已恢复");
    }
  } else {
    OTA_LOG_ERROR("RkSendOtaCmd 收到不支持的任务类型");
  }

  if (ok) {
    setNextStateMachineType(StateMachineType::SUCCESS, ota_state::UPDATING, "RK 升级完成");
  } else {
    setNextStateMachineType(StateMachineType::FAILED, ota_state::FAIL, "RK 升级失败");
  }
}

// ===========================================================================
// Success：任务计数 + 决定下一个任务还是收尾
// ===========================================================================
void Success::run() {
  auto& d = data();

  const int next_index = d.current_task_index.load() + 1;
  d.current_task_index.store(next_index);
  writeTaskIndex(next_index);

  d.ota_state_msg.pkg_current = next_index;
  d.ota_state_msg.pkg_total = static_cast<int>(d.task_type_list.size());

  OTA_LOG_INFO("任务完成，进度 {}/{}", next_index, d.task_type_list.size());

  if (next_index >= static_cast<int>(d.task_type_list.size())) {
    OTA_LOG_INFO("全部任务完成，进入收尾");
    say("升级成功");
    setNextStateMachineType(StateMachineType::REBOOT, ota_state::SUCCEED, "升级完成");
  } else {
    setNextStateMachineType(StateMachineType::CHECK_OTA_TASK, ota_state::UPDATING, "升级下一个任务");
  }
}

// ===========================================================================
// Failed：等 /ota_reset（对应真实工程 failed.cpp）
// ===========================================================================
void Failed::run() {
  auto& d = data();

  say("升级失败");
  OTA_LOG_ERROR("升级失败: {}（输入 reset 命令可复位）", d.ota_state_msg.state_details);

  auto condition = [&d]() -> bool {
    return !d.running.load() || d.getIsFailedReset();
  };

  while (d.running.load()) {
    const auto ret = d.failed_task_waiter.wait(condition, std::chrono::seconds(5));
    if (ret == ErrorCode::OK) {
      break;
    }
    OTA_LOG_WARN("仍在失败状态，等待复位……（输入 reset）");
  }

  if (!d.running.load()) {
    OTA_LOG_INFO("进程终止");
    return;   // 基类析构跳过推进
  }

  // 清干净本次升级留下的运行时状态
  d.resetIsReceiveTask();
  d.resetFailed();
  d.is_had_send_rk_img.store(false);
  d.is_had_stop_controller.store(false);
  d.is_had_reboot.store(false);
  d.current_task_index.store(0);
  d.task_type_list.clear();
  d.last_state_machine_type = StateMachineType::IDLE;

  writeTaskIndex(0);
  writeTaskTypeList({});

  OTA_LOG_INFO("失败状态已复位，回到 IDLE");
  setNextStateMachineType(StateMachineType::IDLE, ota_state::IDLE, "已复位");
}

// ===========================================================================
// Reboot：写版本号 + 按配置决定重启策略
// ===========================================================================
void Reboot::writeVersionJson() {
  auto& d = data();
  const std::string version = d.getOtaVersion();
  if (version.empty() || version == "UNKNOWN") {
    OTA_LOG_INFO("没有新版本号，跳过写版本文件");
    return;
  }
  // 真实工程用 nlohmann::json 生成 {"current_version": "x.y.z"}
  const std::string json = "{\"current_version\": \"" + version + "\"}";
  if (auto ret = fs::atomicWriteFile(d.paths.versionJsonFile(), json); !ret.has_value()) {
    OTA_LOG_ERROR("写版本号文件失败");
  } else {
    OTA_LOG_INFO("版本号已写入: {} -> {}", d.paths.versionJsonFile().string(), json);
  }
}

void Reboot::run() {
  auto& d = data();

  if (!d.is_had_reboot.load()) {
    writeVersionJson();
    d.is_had_reboot.store(true);
  }

  if (d.configures.hard_reboot) {
    OTA_LOG_INFO("配置为硬重启：转 PUB_HARD_REBOOT，由外部下电");
    setNextStateMachineType(StateMachineType::PUB_HARD_REBOOT, ota_state::SUCCEED, "等待硬重启");
    return;
  }

  if (d.configures.reboot_after_ota) {
    OTA_LOG_WARN("配置为软重启：真实工程此处会执行 sudo reboot（演示里跳过）");
  } else {
    OTA_LOG_INFO("配置为不重启：清理运行时状态后回到 IDLE");
  }

  // 收尾：索引和任务列表必须一起清空，否则磁盘上会留下自相矛盾的状态
  d.resetIsReceiveTask();
  d.is_had_send_rk_img.store(false);
  d.is_had_stop_controller.store(false);
  d.is_had_reboot.store(false);
  d.current_task_index.store(0);
  d.task_type_list.clear();
  d.last_state_machine_type = StateMachineType::IDLE;

  writeTaskIndex(0);
  writeTaskTypeList({});

  d.ota_state_msg.download_progress = 0;
  setNextStateMachineType(StateMachineType::IDLE, ota_state::SUCCEED, "升级成功，可以再次发起升级");
}

// ===========================================================================
// PubHardReboot：广播硬重启请求
// ===========================================================================
void PubHardReboot::run() {
  OTA_LOG_WARN("广播硬重启消息，等待外部断电重启");
  auto& d = data();
  d.resetIsReceiveTask();
  setNextStateMachineType(StateMachineType::IDLE, ota_state::SUCCEED, "等待硬重启");
}

}  // namespace ota_mini::state_machine
