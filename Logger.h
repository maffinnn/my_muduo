#pragma once

#include <string>

#include "noncopyable.h"
//定义日志的级别 INFO：打印/跟踪重要的流程信息
//ERROR：不影响软件正常运行的错误
//FATAL: 影响程序运行的错误 exit()
//DEBUG：调试信息

//LOG_INFO("%S %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
      Logger& logger = Logger::instance(); \
      logger.setLogLevel(INFO); \
      char buf[1024] = {0}; \
      snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
      logger.log(buf); \
    } while(0)

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
      Logger& logger = Logger::instance(); \
      logger.setLogLevel(ERROR); \
      char buf[1024] = {0}; \
      snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
      logger.log(buf); \
    } while(0)

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
      Logger& logger = Logger::instance(); \
      logger.setLogLevel(FATAL); \
      char buf[1024] = {0}; \
      snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
      logger.log(buf); \
      exit(-1); \
    } while(0)

//一般调试的信息较多且不是很重要 过多的I/O操作导致效率降低
//因此DEBUG 模式一般关闭 
//也就是说如果定义了MUDEBUG则输出DEBUG信息 否则不输出
#ifndef MUDEBUG
    #define LOG_DEBUG(logmsgFormat, ...) \
        do \
        { \
        Logger& logger = Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        } while(0)
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif

//为了防止宏定义造成意外的错误
    
enum LogLevel
{
    INFO,//普通信息
    ERROR,//错误信息
    FATAL,//core dump信息
    DEBUG,//调试信息
};

//输出一个日志类 
//单例模式 - Singleton Pattern 单一的一个类，该类负责创建自己的对象，同时确保只有单个对象被创建
//即不可以被构造和赋值 -》利用noncopyable作为基类
class Logger : noncopyable
{
public:
    //获取日志唯一的实例对象
    static Logger& instance();
    //设置日志级别
    void setLogLevel(int level);
    //写日志
    void log(std::string msg);
private:
    //系统变量都是将下划线放在前面 为了区分 我们把成员变量的下划线置后
    int logLevel_;
    Logger(){}
};
