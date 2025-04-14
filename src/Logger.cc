#include <iostream>

#include "Logger.h"
#include "Timestamp.h"


// 获取日志唯一的实例对象 单例
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}


// 设置日志级别
void Logger::setLogLevel(int level)
{
    loglevel_ = level;
}


// 写日志  
// 将日志级别、当前时间和日志信息格式化后输出到 标准输出（控制台）  
// 格式：[级别信息] time : msg    
// 例如 [INFO] 2025-04-01 12:34:56 : This is a log message.
void Logger::log(std::string msg) // msg是要输出的内容
{
    std::string pre = "";
    switch (loglevel_) // 根据loglevel_确定日志级别前缀
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

    // 打印时间和msg
    // 格式：[级别信息] time : msg
    std::cout << pre + Timestamp::now().toString() << " : " << msg << std::endl;

}