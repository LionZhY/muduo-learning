#pragma once
/*
禁止拷贝：nuncopyable被继承后，派生类对象可正常构造和析构，但无法进行拷贝构造和赋值构造
*/

class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete; // 删除拷贝构造函数
    noncopyable& operator=(const noncopyable&) = delete; // 删除拷贝赋值运算符=

protected:
    // 允许派生类继承noncopyable，但不允许在外部直接实例化noncopyable本身
    noncopyable() = default; // 默认构造，允许子类正常构造对象
    ~noncopyable() = default;// 默认析构，允许子类正常析构
};