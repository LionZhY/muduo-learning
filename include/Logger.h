#pragma once

#include <string>
#include "noncopyable.h"

// 不需要用户自己去获取实例、设置级别、写日志，可以定义成宏


// LOG_INFO("%s %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##_VA_ARGS__); \
        logger.log(buf); \
    } while(0)


// LOG_ERROR
#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##_VA_ARGS__); \
        logger.log(buf); \
    } while(0)


// LOG_FATAL
#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.seLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##_VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    } while(0)


// LOG_DEBUG
// debug 信息比较多，打印出来会影响查看，所以调试信息一般运行起来默认是关闭的
// 通过宏包起来
#ifdef MUDEBUG // 如果定义了MUDEBUG，就正常输出debug信息
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##_VA_ARGS__); \
        logger.log(buf); \
    } while(0)
#else // 如果没定义MUDEBUG，LOG_DEBUG是空的，不输出
#define LOG_DEBUG(logmsgFormat, ...)
#endif




// 定义日志级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core dump信息
    DEBUG, // 调试信息
};

// 输出一个日志类
class Logger : noncopyable
{
public:
    // 获取日志唯一的实例对象 单例
    static Logger& instance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(std::string msg);

private:
    int loglevel_;


};
