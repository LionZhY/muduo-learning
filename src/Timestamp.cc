#include <time.h>       // 引入时间相关的标准库
#include "Timestamp.h"  // 引入对应头文件

// 默认构造函数：初始化时间戳为0
Timestamp::Timestamp() : microSecondSinceEpoch_(0)
{

}

// 带参数的构造函数：接受一个64位整数（微秒时间戳）作为参数，并初始化成员变量
Timestamp::Timestamp(int64_t microSecondSinceEpoch) : microSecondSinceEpoch_(microSecondSinceEpoch)
{

}

// 获取当前时间的时间戳
Timestamp Timestamp::now()
{
    return Timestamp(time(nullptr));// time(nullptr)返回当前时间的秒数，这里存入Timestamp
}

// 将时间戳转换为可读字符串格式
std::string Timestamp::toString() const // 加const限定该成员函数不能修改类的成员变量
{
    char buf[128] = {0};// 定义字符数组来存储格式化时间字符串
    tm *tm_time = localtime(&microSecondSinceEpoch_);// 将时间戳转换为本地时间结构体tm

    // 格式化时间为 "YYYY/MM/DD HH:MM:SS" 形式，并存到buf数组中
    snprintf(buf, 128, "%4d%02d%02d %02d:%02d:%02d",
            tm_time->tm_year + 1900,
            tm_time->tm_mon + 1,
            tm_time->tm_mday,
            tm_time->tm_hour,
            tm_time->tm_min,
            tm_time->tm_sec);
    
    return buf;// 返回格式化后的字符串
}

// 测试代码（需要测试可以取消注释）
// #include <iostream>
// int main() {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }

