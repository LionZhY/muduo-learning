#pragma once

#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp;

/**
 * 定义回调操作的类型
 */

 using TcpConnectionPtr      = std::shared_ptr<TcpConnection>; // 管理连接对象的生命周期，保证Tcp连接在使用期间不被销毁

 using ConnectionCallback    = std::function<void (const TcpConnectionPtr&)>; // 连接建立或断开时的回调类型
 using CloseCallback         = std::function<void (const TcpConnectionPtr&)>; // 连接关闭时的回调类型
 using WriteCompleteCallback = std::function<void (const TcpConnectionPtr&)>; // 数据写入完成时的回调类型
 
 using HighWaterMarkCallback = std::function<void (const TcpConnectionPtr&, size_t)>; // 高水位标记回调类型（流量控制，当输出缓冲区数据量超过设定的阈值时触发）

 using MessageCallback       = std::function<void (const TcpConnectionPtr&, Buffer*, Timestamp)>; // 消息到达时的回调类型