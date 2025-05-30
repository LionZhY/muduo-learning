#pragma once 

#include <iostream>
#include <string>

// 事件戳类，表示微秒级的时间点
class Timestamp
{
public:
    // 默认构造
    Timestamp();

    // 带参数的显示构造
    explicit Timestamp(int64_t microSecondSinceEpoch); // explicit禁止隐式转换

    // 获取当前时间点的时间戳，返回一个Timestamp实例
    static Timestamp now();

    // 时间戳转换为字符串格式
    std::string toString() const; // const表示不会修改成员变量，保证toString仅用于读取数据

private:
    int64_t microSecondSinceEpoch_; // 以微秒为单位的时间戳值
};