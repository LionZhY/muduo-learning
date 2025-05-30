#include <iostream>

#include "Logger.h"
#include "Timestamp.h"


// 获取日志唯一的实例对象 单例
Logger& Logger::instance()
{
    static Logger logger; // static保证 logger 只会被创建一次，并在整个程序生命周期内存在
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int level)
{
    loglevel_ = level;
}


// 写日志 (将日志输出到控制台)
// 格式：[级别信息] time : msg  例如 [INFO] 2025-04-01 12:34:56 : This is a log message.
void Logger::log(std::string msg)
{
    std::string pre = ""; // 日志级别前缀
    switch(loglevel_) // 根据日志级别确定前缀
    {
    case INFO:
        pre = "[INFO]";
        break;
    case ERROR:
        pre = "[ERROR]";
        break;
    case FATAL:
        pre = "[FATAL]";
        break;
    case DEBUG:
        pre = "[DEBUG]";
        break;
    default:
        break;
    }

    // 打印 前缀pre + 时间 + 日志msg
    // 格式：[级别信息] time : msg  例如 [INFO] 2025-04-01 12:34:56 : This is a log message.
    std::cout << pre + Timestamp::now().toString() << " : " << msg << std::endl; 
}