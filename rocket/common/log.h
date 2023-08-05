#ifndef ROCKET_COMMON_LOG_H
#define ROCKET_COMMON_LOG_H
// 防止头文件重复包含，命名规则为项目名 + 目录名 + 文件名

#include <string>
#include <queue>
#include <memory>
#include <semaphore.h>
#include "rocket/common/config.h"
#include "rocket/common/mutex.h"
#include "rocket/net/timer_event.h"

namespace rocket 
//隔离库代码，防止命名空间污染
{
template<typename... Args>  //可变参数模板，允许传入任意数量和类型的参数
std::string formatString(const char * str, Args&&... args)  //接受一个C风格字符串和可变参数,比如("%s","11") 或者 ("%s, %d", "11", 11)都行
//实现自定义消息
{
    int size  = snprintf(nullptr, 0, str, args...); //获取格式化后字符串的长度，不会将结果输出到缓冲区，仅计算长度
    std::string result; //定义一个字符串变量用于存储格式化后的结果
    if (size > 0)
    {
        result.resize(size);    // 调整字符串大小以适应格式化后的结果
        /*
        &result[0] 表示返回字符串 result 的第一个字符的地址（指针），这里使用指针是为了在调用 snprintf 函数时直接将格式化后的结果写入到 result 中，从而避免了通过循环逐个字符复制的开销。
        在 C++11 之前，std::string 没有提供获取字符数组指针的方法，因此我们需要使用 &result[0] 来获取字符串的起始地址。而在 C++11 及以后的版本中，标准已经规定了可以使用 std::string 的成员函数 data() 来获取字符串的起始地址，因此也可以使用 result.data() 来替代 &result[0]。不过需要注意的是，在使用 data() 函数时，要确保字符串不为空并且已经以 null 结尾，否则可能会导致程序崩溃或者未定义行为。
        总的来说，这种使用指针的方式可以更高效地操作字符串，并且能够避免无谓的内存分配和拷贝，从而提高代码的性能。
        */
        snprintf(&result[0], size + 1, str, args...);   // 将格式化后的结果写入字符串变量中
    }
    return result;
}


//##__VA_ARGS__代表可变长参数
    /*
    `__VA_ARGS__` 是一个预处理器宏，在 C/C++ 中用于表示可变参数的部分。
    在使用宏定义时，`__VA_ARGS__` 表示宏的可变参数部分，即宏在被调用时可以接受任意数量和类型的参数。通常，`__VA_ARGS__` 出现在宏定义中的位置，用于将可变参数传递给宏展开的代码。
    下面是一个简单的例子来说明 `__VA_ARGS__` 的使用方式：
    ```cpp
    #define LOG(format, ...) printf(format, __VA_ARGS__)

    int main() {
        LOG("Hello %s!\n", "world");
        LOG("The answer is %d.\n", 42);
        return 0;
    }
    ```
    在上述代码中，`LOG` 宏定义接受一个格式字符串作为第一个参数，后面的 `...` 表示可变参数部分。通过使用 `printf` 函数，我们可以将 `format` 字符串和后面的可变参数一起输出到控制台。
    在调用 `LOG` 宏时，我们可以像调用函数一样传入多个参数，这些参数会被替换为 `__VA_ARGS__`，然后传递给 `printf` 函数进行格式化输出。
    注意：`__VA_ARGS__` 必须始终与省略号 `...` 成对出现，并且在宏定义中必须放在最后。
    */
#define DEBUGLOG(str, ...) \
    if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Debug) \
    { \
        rocket::Logger::GetGlobalLogger()->pushLog(rocket::LogEvent(rocket::LogLevel::Debug).toString()\
         + "[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" + rocket::formatString(str, ##__VA_ARGS__) + "\n"); \
    } \

#define INFOLOG(str, ...) \
    if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Info) \
    { \
        rocket::Logger::GetGlobalLogger()->pushLog(rocket::LogEvent(rocket::LogLevel::Info).toString()\
         + "[" + std::string(__FILE__) + ":" +  std::to_string(__LINE__) + "]\t" + rocket::formatString(str, ##__VA_ARGS__) + "\n"); \
    } \

#define ERRORLOG(str, ...) \
    if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Error) \
    { \
        rocket::Logger::GetGlobalLogger()->pushLog(rocket::LogEvent(rocket::LogLevel::Error).toString()\
         + "[" + std::string(__FILE__) + ":" +  std::to_string(__LINE__) + "]\t" +  rocket::formatString(str, ##__VA_ARGS__) + "\n"); \
    } \


#define APPDEBUGLOG(str, ...) \
    if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Debug) \
    { \
        rocket::Logger::GetGlobalLogger()->pushAppLog(rocket::LogEvent(rocket::LogLevel::Debug).toString()\
         + "[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" + rocket::formatString(str, ##__VA_ARGS__) + "\n"); \
    } \

#define APPINFOLOG(str, ...) \
    if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Info) \
    { \
        rocket::Logger::GetGlobalLogger()->pushAppLog(rocket::LogEvent(rocket::LogLevel::Info).toString()\
         + "[" + std::string(__FILE__) + ":" +  std::to_string(__LINE__) + "]\t" + rocket::formatString(str, ##__VA_ARGS__) + "\n"); \
    } \

#define APPERRORLOG(str, ...) \
    if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Error) \
    { \
        rocket::Logger::GetGlobalLogger()->pushAppLog(rocket::LogEvent(rocket::LogLevel::Error).toString()\
         + "[" + std::string(__FILE__) + ":" +  std::to_string(__LINE__) + "]\t" +  rocket::formatString(str, ##__VA_ARGS__) + "\n"); \
    } \



enum LogLevel
//枚举体
{
    Unknown = 0,
    Debug = 1,
    Info = 2,
    Error = 3
};


class AsyncLogger {
public:
    typedef std::shared_ptr<AsyncLogger> s_ptr;
    AsyncLogger(const std::string & file_name, const std::string & file_path, int max_size);
    void stop();
    //刷新到磁盘,一开始是写到缓冲区的，不刷新到磁盘会丢失数据
    void flush();
    void pushLogBuffer(std::vector<std::string> & vec);
public:
    static void * Loop(void *); //异步的loop


private:
    // m_file_path/m_file_name_yyyymmdd.1
    std::queue<std::vector<std::string>> m_buffer;

    std::string m_file_name;    //日志输出文件名字
    std::string m_file_path;    //日志输出路径
    int m_max_file_size {0};    //日志单个文件最大大小，单位为字节
    
    sem_t m_semaphore;      //信号量
    pthread_t m_thread;     //线程
    pthread_cond_t m_condition;  //消费者要等待生产者通知，通知就用条件变量，需要和互斥锁一起用
    Mutex m_mutex;

    std::string m_date;     //当前打印日志的文件日期
    FILE * m_file_handler {NULL};  //当前打开的日志文件句柄,频繁打开会造成性能损耗
    bool m_reopen_flag {false}; //是否要重新打开文件，跨天或者一个文件满了才会为true

    int m_no {0};   //日志文件序号
    bool m_stop_flag {false};   //防止loop死循环
};



//日志器，打印日志
class Logger
{
public:
    typedef std::shared_ptr<Logger> s_ptr;
    Logger(LogLevel level, int type = 1);
    void pushLog(const std::string & msg);  //将msg对象塞到buffer中
    void pushAppLog(const std::string & msg);  //将msg对象塞到buffer中
    void init();
    void log(); //将buffer中的日志输出，后续优化 ① 异步 ② 输出到文件
    LogLevel getLogLevel() const 
    {
        return m_set_level;
    }
    void syncLoop();
public:
    static Logger * GetGlobalLogger();
    static void InitGlobalLogger(int type = 1);
private:
    LogLevel m_set_level;   //日志级别
    std::vector<std::string> m_buffer;  //底层rpc日志队列
    std::vector<std::string> m_app_buffer;  //应用层日志
    Mutex m_mutex;
    Mutex m_app_mutex;

    std::string m_file_name;    //日志输出文件名字
    std::string m_file_path;    //日志输出路径
    int m_max_file_size {0};  

    AsyncLogger::s_ptr m_asnyc_logger;
    AsyncLogger::s_ptr m_asnyc_app_logger;
    
    TimerEvent::s_ptr m_timer_event;

    int m_type {0}; 
};

//为LogLevel提供两个方法,打印日志的时候需要用
std::string LogLevelToString(LogLevel level);
LogLevel StringToLogLevel(const std::string & log_level);


class LogEvent
{
public:
    LogEvent(LogLevel level): m_level(level) {};
    std::string getFileName() const
    {
        return m_file_name;
    }
    LogLevel getLogLevel() const
    {
        return m_level;
    }
    std::string toString();


private:
    std::string m_file_name;    //文件名
    int32_t m_file_line;        //行号
    int32_t m_pid;              //进程号
    int32_t m_thread_id;        //线程号
    LogLevel m_level;           //日志级别
};

}

#endif