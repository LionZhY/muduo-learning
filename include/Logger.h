#pragma once

#include <string>
#include "noncopyable.h"

/*使用宏 提供日志打印
* 简化日志记录，避免重复代码
* 不需要用户自己去手动格式化字符串、获取 Logger 实例、调用 log() 方法。
*/



// LOG_INFO("%s %d", arg1, arg2) 
// 定义宏 LOG_INFO，用于记录 INFO 级别的日志
#define LOG_INFO(logmsgFormat, ...)                                    \
    do                                                                 \
    {                                                                  \
        Logger &logger = Logger::instance(); /*获取单例Logger实例*/     \
        logger.setLogLevel(INFO);            /*设置当前日志级别*/        \
        char buf[1024] = {0};                /*存储格式化后的日志信息*/  \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); /*将格式化的日志信息写入buf*/  \
        logger.log(buf);                     /*将格式化后的日志内容写入日志系统*/         \
    } while(0) // do {}while(0) 是常见的宏封装技巧，保证宏的代码块在调用时，不会因为分号或换行导致语法错误


// LOG_ERROR
#define LOG_ERROR(logmsgFormat, ...)                        \
    do                                                      \
    {                                                       \
        Logger &logger = Logger::instance();                \
        logger.setLogLevel(ERROR);                          \
        char buf[1024] = {0};                               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
    } while(0)


// LOG_FATAL
#define LOG_FATAL(logmsgFormat, ...)                        \
    do                                                      \
    {                                                       \
        Logger &logger = Logger::instance();                \
        logger.setLogLevel(FATAL);                           \
        char buf[1024] = {0};                               \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);   \
        logger.log(buf);                                    \
        exit(-1);                                           \
    } while(0)


// LOG_DEBUG
// debug 信息比较多，打印出来会影响查看，所以调试信息一般运行起来默认是关闭的
// 通过宏包起来
#ifdef MUDEBUG // 如果定义了MUDEBUG，就正常输出debug信息 (就是在使用LOG_DEBUG之前，有#define MUDEBUG 这一句)
    #define LOG_DEBUG(logmsgFormat, ...)                      \
        do                                                    \
        {                                                     \
            Logger &logger = Logger::instance();              \
            logger.setLogLevel(DEBUG);                        \
            char buf[1024] = {0};                             \
            snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
            logger.log(buf);                                  \
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
