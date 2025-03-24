#pragma once // 预处理指令，防止头文件被重复包含

#include <iostream>
#include <string>

// 时间戳类，表示微秒级的时间点
class Timestamp 
{
public:
    // 默认构造函数，创建一个无效的时间戳（即时间值为0）
    Timestamp();

    // 显式构造函数，使用微秒级时间值创建时间戳
    explicit Timestamp(int64_t microSecondSinceEpoch); // explicit 禁止隐式转换

    // 获取当前时间点的时间戳，返回一个Timestamp实例
    static Timestamp now();

    // 将时间戳转换为字符串格式
    std::string toString() const;// const表示不会修改成员变量，保证toString仅用于读取数据

private:
    int64_t microSecondSinceEpoch_;// 以微秒为单位的时间戳值

};