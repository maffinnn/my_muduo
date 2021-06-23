#pragma once

#include <string>

#include "noncopyable.h"
//������־�ļ��� INFO����ӡ/������Ҫ��������Ϣ
//ERROR����Ӱ������������еĴ���
//FATAL: Ӱ��������еĴ��� exit()
//DEBUG��������Ϣ

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

//һ����Ե���Ϣ�϶��Ҳ��Ǻ���Ҫ �����I/O��������Ч�ʽ���
//���DEBUG ģʽһ��ر� 
//Ҳ����˵���������MUDEBUG�����DEBUG��Ϣ �������
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

//Ϊ�˷�ֹ�궨���������Ĵ���
    
enum LogLevel
{
    INFO,//��ͨ��Ϣ
    ERROR,//������Ϣ
    FATAL,//core dump��Ϣ
    DEBUG,//������Ϣ
};

//���һ����־�� 
//����ģʽ - Singleton Pattern ��һ��һ���࣬���ฺ�𴴽��Լ��Ķ���ͬʱȷ��ֻ�е������󱻴���
//�������Ա�����͸�ֵ -������noncopyable��Ϊ����
class Logger : noncopyable
{
public:
    //��ȡ��־Ψһ��ʵ������
    static Logger& instance();
    //������־����
    void setLogLevel(int level);
    //д��־
    void log(std::string msg);
private:
    //ϵͳ�������ǽ��»��߷���ǰ�� Ϊ������ ���ǰѳ�Ա�������»����ú�
    int logLevel_;
    Logger(){}
};
