#pragma once

#define OTA_STATE_MACHINE_LIST                     \
  X(INIT, "init", "初始化", -1)                    \
  X(IDLE, "idle", "空闲状态", 0)                   \
  X(CHECK_PREREQUISITES, "check_prerequisites", "检查前置条件", 1) \
  X(DOWNLOAD, "download", "下载升级文件", 2)       \
  X(PARSING_IMG, "parsing_img", "解析镜像文件", 3) \
  X(CHECK_OTA_TASK, "check_ota_task", "检查OTA任务", 4) \
  X(SEND_IMG_DATA, "send_img_data", "发送镜像数据", 5)  \
  X(SUCCESS, "success", "升级成功", 6)             \
  X(FAILED, "failed", "失败", 7)                   \
  X(REBOOT, "reboot", "重启", 8)


#define OTA_ERROR_CODE_LIST          \
  X(OK, "成功", 0)                   \
  X(FAILED, "通用失败", 1)           \
  X(TIMEOUT, "超时", 2)              \
  X(NOT_FOUND, "文件不存在", 3)      \
  X(READ_FAILED, "读取失败", 4)      \
  X(WRITE_FAILED, "写入失败", 5)     \
  X(FILE_NOT_OPENED, "文件未打开", 6)

#define OTA_TASK_TYPE_LIST                                                              \
  X(UNKNOWN, "unknown", "未知", 0)                                                      \
  X(NX_SERIAL, "nx_serial", "NX 串口相关设备", 1)                                        \
  X(RK_SERIAL, "rk_serial", "RK 串口相关设备", 2)                                        \
  X(RK_ETHERCAT, "rk_ethercat", "RK ethercat 相关设备", 3)                               \
  X(NX_SELF, "nx_self", "NX 自升级", 4)                                                  \
  X(RK_SELF, "rk_self", "RK 自升级", 5)