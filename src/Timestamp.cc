#include <time.h> // 引入时间相关库
#include "Timestamp.h"

// 默认构造
Timestamp::Timestamp() : microSecondSinceEpoch_(0) // 初始化时间戳为0
{
}


// 带参数的显示构造 
Timestamp::Timestamp(int64_t microSecondSinceEpoch) 
    : microSecondSinceEpoch_(microSecondSinceEpoch) // 接受一个微秒时间戳为参数，并初始化成员变量
{
}


// 获取当前时间点的时间戳
Timestamp Timestamp::now()
{
    return Timestamp(time(nullptr)); // time(nullptr)返回以秒为单位的当前时间，存入microSecondSinceEpoch_
}


// 时间戳转换为字符串格式
std::string Timestamp::toString() const
{
    char buf[128] = {0}; // 存储格式化时间字符串
    tm* tm_time = localtime(&microSecondSinceEpoch_); // 将时间戳转换为本地时间结构体tm

    // 格式化时间为 "YYYY/MM/DD HH:MM:SS" 形式，并存放到buf数组中
    snprintf(buf, 128, "%4d%02d%02d %02d:%02d:%02d", 
             tm_time->tm_year + 1900,   // 年
             tm_time->tm_mon + 1,       // 月
             tm_time->tm_mday,          // 日
             tm_time->tm_hour,          // 时
             tm_time->tm_min,           // 分
             tm_time->tm_sec);          // 秒
    
    return buf; // 返回格式化后的事件字符串
}

/* 测试代码 （需要时可取消注释）*/
// #include <iostream>
// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0; 
// }