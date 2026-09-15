#include <iostream>
#include <string>
#include <string_view>
#include "mini_log.hpp"


namespace humanoid_ota::utils::ros2 {
const std::string &getOtaWorkDir() {
    static const std::string dir = "/home/eame/cust_para/ota_file/ota_work";
    return dir;
}

const std::string & getCurrentOtaStateFile() {
    static const std::string file = getOtaWorkDir() + "/current_ota_state";
    return file;
}

std::string describe();


}
namespace humanoid_ota::utils::ros2 {
std::string describe() {
  return "ros2 utils namespace reopened";
}
}  // namespace humanoid_ota::utils::ros2

namespace {
int g_internalCount = 0;
std::string helperMakeName(std::string_view base){
    return std::string(base) + "_" + std::to_string(++g_internalCount);
}
}

namespace ros2 = humanoid_ota::utils::ros2;

int main(){
    printTitle("01 namespace");
    printSourceHint("src/utiles/ros2/ros2.hpp | .cpp,src/ota/state_machine/parsing_img.cpp");

    printStep("1) 嵌套命名空间");

    const std::string & work_dir = humanoid_ota::utils::ros2::getOtaWorkDir();
    MINI_INFO("工作目录： {}",work_dir);
    MINI_INFO("状态文件路径： {}",humanoid_ota::utils::ros2::getCurrentOtaStateFile());
    MINI_INFO("两次调用是同意对象吗{}",static_cast<const void *>(&work_dir) ==
        static_cast<const void *>(&humanoid_ota::utils::ros2::getOtaWorkDir()));
    printStep("2) namespace 别名");
    MINI_INFO("namespace 别名： {}",ros2::describe());

    printStep("3) 匿名命名空间");
    MINI_INFO("匿名命名空间： {}",helperMakeName("mcu"));
    MINI_INFO("内部函数生成： {}",helperMakeName("mcu"));
    MINI_INFO("内部计数器： {}",g_internalCount);


 
}