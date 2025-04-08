#include <string.h>  // 包含 memset()、strlen()、strcpy() 等 C 风格字符串处理函数

#include "InetAddress.h"


// 构造  初始化 IPv4 地址对象
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    ::memset(&addr_, 0, sizeof(addr_));     // 清零结构体，避免脏数据
    addr_.sin_family = AF_INET;             // 设置地址族为 IPv4
    addr_.sin_port = ::htons(port);         // 本地主机字节序，转为网络字节序
    addr_.sin_addr.s_addr = ::inet_addr(ip.c_str()); // 字符串IP转为网络二进制格式

}


// 获取端口号port（主机字节序）
uint16_t InetAddress::toPort() const
{
    return ::ntohs(addr_.sin_port); // 网络序 -> 主机序
}


// 转换为 IP 地址字符串 （如 "192.168.1.1"）
std::string InetAddress::toIp() const
{
    // addr
    char buf[64] = {0}; // 缓冲区，用于存放IP字符串
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // 将网络地址转为点分十进制字符串
    return buf; // 转换为std::string 返回
}


// 转换为 "IP:Port" 组合字符串（如 "192.168.1.1:8080"）
std::string InetAddress::toIpPort() const
{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); // IP 转字符
    size_t end = ::strlen(buf); // 找到当前字符串结尾的位置

    uint16_t port = ::ntohs(addr_.sin_port); // 网络序 ->主机序，得到真实端口
    sprintf(buf+end, ":%u", port); // 拼接 ":端口号"
    return buf;
}




// 测试代码
// #if 0
#include <iostream>
int main()
{
    InetAddress addr(8080); // 使用默认IP 127.0.0.1 和端口8080
    std::cout << addr.toIpPort() << std::endl; // 输出格式为：127.0.0.1:8080
}
// #endif 