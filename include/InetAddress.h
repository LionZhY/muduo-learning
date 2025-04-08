#pragma once

#include <arpa/inet.h>  // 提供inet_pton和inet_ntop等函数，用于ip地址在字符串和二进制之间转换
#include <netinet/in.h> // 定义sockaddr_in结构体 是IPv4 地址在 socket 编程中的表示方式
#include <string>

/**
 * 封装IPv4套接字地址sockaddr_in
 */


// 封装socket地址类型  将底层的 sockaddr_in 封装成 C++ 类，隐藏底层细节，提供易用的API
class InetAddress
{
public:
    // 构造
    // 允许通过 IP 和端口字符串形式来创建一个 socket 地址
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");

    // 重载构造
    // 允许直接使用一个已有的 sockaddr_in 对象来构造 InetAddress 类实例
    explicit InetAddress(const sockaddr_in &addr) 
        : addr_(addr)
    {

    }

    // 返回当前对象所表示的 IP 地址字符串 （如 "192.168.1.1"）
    std::string toIp() const;

    // 返回 IP 和端口的组合字符串（如 "192.168.1.1:8080"）
    std::string toIpPort() const;

    // 返回端口号（主机字节序）
    uint16_t toPort() const;

    // 获取底层的sockaddr_in指针
    const sockaddr_in *getSockAddr() const      { return &addr_; }

    // 设置底层地址结构
    void setSockAddr(const sockaddr_in &addr)   { addr_ = addr; }


private:
    sockaddr_in addr_; // 封装的数据成员，存储底层的IPv4套接字地址

};