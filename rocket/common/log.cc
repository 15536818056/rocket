#include <sys/time.h>
#include <sstream>
#include <stdio.h>
#include <assert.h>
#include "rocket/common/log.h"
#include "rocket/common/util.h"
#include "rocket/common/run_time.h"
#include "rocket/net/eventloop.h"



namespace rocket
{

    static Logger * g_logger = NULL;
    Logger * Logger::GetGlobalLogger()
    {
        return g_logger;
    }

    Logger::Logger(LogLevel level, int type /*=1*/) : m_set_level(level), m_type(type) {
        if (m_type == 0)
        {
            return;
        }
        m_asnyc_logger = std::make_shared<AsyncLogger>(
            Config::GetGlobalConfig()->m_log_file_name + "_rpc", 
            Config::GetGlobalConfig()->m_log_file_path, 
            Config::GetGlobalConfig()->m_log_max_file_size);

        m_asnyc_app_logger = std::make_shared<AsyncLogger>(
            Config::GetGlobalConfig()->m_log_file_name + "_app", 
            Config::GetGlobalConfig()->m_log_file_path, 
            Config::GetGlobalConfig()->m_log_max_file_size);
    }

    void Logger::init() {
        if (m_type == 0)
        {
            return;
        }
        m_timer_event = std::make_shared<TimerEvent>(Config::GetGlobalConfig()->m_log_sync_interval, true, std::bind(&Logger::syncLoop, this));  
        //成员函数，不是静态的，当作为回调函数的时候，需要指名一个this指针,因为普通的成员函数必须有对象才能调用
        //如果是静态成员函数，就不需要绑定，直接传入就行了
        EventLoop::GetCurrentEventLoop()->addTimerEvent(m_timer_event);
    }

    void Logger::syncLoop()
    {
        //同步m_buffer 到 async_logger 的buffer队尾
        // printf("sync to sync logger\n");
        std::vector<std::string> tmp_vec;
        ScopeMutex<Mutex> lock(m_mutex);
        tmp_vec.swap(m_buffer); //指针交换动作
        lock.unlock();
        if (!tmp_vec.empty())
        {
            m_asnyc_logger->pushLogBuffer(tmp_vec);
        }
        tmp_vec.clear();

        //同步m_app_buffer 到 app_async_logger 的buffer队尾
        std::vector<std::string> tmp_vec2;
        ScopeMutex<Mutex> lock2(m_app_mutex);
        tmp_vec2.swap(m_app_buffer); //指针交换动作
        lock2.unlock();
        if (!tmp_vec2.empty())
        {
            m_asnyc_app_logger->pushLogBuffer(tmp_vec2);
        }
        tmp_vec2.clear();
    }


    void Logger::InitGlobalLogger(int type /* = 1*/)
    {
        LogLevel global_log_level = StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
        printf("Init log level [%s]\n", LogLevelToString(global_log_level).c_str());
        g_logger = new Logger(global_log_level, type);
        g_logger->init();
    }

    std::string LogLevelToString(LogLevel level)
    {
        switch (level)
        {
            case Debug:
                return "DEBUG";
            case Info:
                return "INFO";
            case Error:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }

    LogLevel StringToLogLevel(const std::string & log_level)
    {
        if (log_level == "DEBUG")
        {
            return Debug;
        }
        else if (log_level == "INFO")
        {
            return Info;
        }
        else if (log_level == "ERROR")
        {
            return Error;
        }
        else
        {
            return Unknown;
        }
    }

    //格式化字符串
    std::string LogEvent::toString()
    {
        struct timeval now_time;
        gettimeofday(&now_time, nullptr);   //获取到当前时间
        struct tm now_time_t;
        localtime_r(&(now_time.tv_sec), &now_time_t);

        char buf[128]; 
        strftime(&buf[0], 128, "%y-%m-%d %H:%M.%S", &now_time_t);
        std::string time_str(buf);
        
        int ms = now_time.tv_usec / 1000;   //单位是微秒
        time_str = time_str + "." + std::to_string(ms);

        m_pid = getPid();
        m_thread_id = getThreadId();


        std::stringstream ss;
        ss << "[" << LogLevelToString(m_level) << "]\t"
            << "[" << time_str << "]\t"
            << "[" << m_pid << ":" << m_thread_id << "]\t";

        //获取当前线程处理的请求的msgid
        std::string msgid = RunTime::GetRunTime()->m_msgid;
        std::string method_name = RunTime::GetRunTime()->m_method_name;
        if (!msgid.empty()) {
            ss << "[" << msgid << "]\t";
        }
        if (!method_name.empty()) {
            ss << "[" << method_name << "]\t";
        }
        return ss.str();
    }

    void Logger::pushLog(const std::string & msg)
    {
        if (m_type == 0)    //同步日志
        {
            printf((msg + "\n").c_str());
            return;
        }        
        ScopeMutex<Mutex> lock(m_mutex);
        m_buffer.push_back(msg);
        lock.unlock(); 
    }
    void Logger::pushAppLog(const std::string & msg)
    {
        ScopeMutex<Mutex> lock(m_app_mutex);
        m_app_buffer.push_back(msg);
        lock.unlock(); 
    }

    //多个线程可能会同时调用log方法
    void Logger::log()
    {
        // ScopeMutex<Mutex> lock(m_mutex);
        // std::queue<std::string> tmp;
        // m_buffer.swap(tmp);
        // lock.unlock();

        // while (!tmp.empty())
        // {
        //     std::string msg = tmp.front();    //m_buffer是全局变量，没有加锁，多个线程访问会有线程安全问题
        //     tmp.pop();
        //     printf("%s\n", msg.c_str());
        // }
    }

    AsyncLogger::AsyncLogger(const std::string & file_name, const std::string & file_path, int max_size) 
    : m_file_name(file_name), m_file_path(file_path), m_max_file_size(max_size)
    {
        sem_init(&m_semaphore, 0, 0);
        assert(pthread_create(&m_thread, NULL, &AsyncLogger::Loop, this) == 0);
        sem_wait(&m_semaphore);
    }
    void * AsyncLogger::Loop(void * arg) {
        //将buffer中的所有数据打印到文件中，然后线程睡眠，直到有新的数据将再重复这个过程
        AsyncLogger * logger = reinterpret_cast<AsyncLogger*>(arg);
        assert(pthread_cond_init(&logger->m_condition, NULL) == 0);
        sem_post(&logger->m_semaphore);
        while (1)
        {
            //获取buffer数据
            ScopeMutex<Mutex> lock(logger->m_mutex);
            while (logger->m_buffer.empty())
            {
                // printf("begin pthread_cond_wait back\n");
                pthread_cond_wait(&(logger->m_condition), logger->m_mutex.getMutex());
            }
            // printf("pthread_cond_wait back\n");
            std::vector<std::string> tmp;
            tmp.swap(logger->m_buffer.front());
            logger->m_buffer.pop();
            lock.unlock();
            //获取当前时间
            timeval now;
            gettimeofday(&now, NULL);
            struct tm now_time;
            localtime_r(&(now.tv_sec), &now_time);
            const char * format = "%Y%m%d";
            char date[32];
            strftime(date, sizeof(date), format, &now_time);
            if (std::string(date) != logger->m_date) {  //当前日期不等于上次打印的日期，说明需要重新打开文件
                logger->m_no = 0;   //隔天需要重新计数
                logger->m_reopen_flag = true;
                logger->m_date = std::string(date);
            }
            if (logger->m_file_handler == NULL) { 
                logger->m_reopen_flag = true;
            }
            std::stringstream ss;   //构造文件名
            ss << logger->m_file_path << logger->m_file_name << "_"  << std::string(date) << "_log.";
            std::string log_file_name = ss.str() + std::to_string(logger->m_no);
            if (logger->m_reopen_flag) {    //如果当前我们已经打开了一个日志文件
                if (logger->m_file_handler)
                {
                    fclose(logger->m_file_handler); //先关闭
                }
                logger->m_file_handler = fopen(log_file_name.c_str(), "a"); //重新打开,追加写入
                logger->m_reopen_flag = false;
            }
            if (ftell(logger->m_file_handler) > logger->m_max_file_size) {  //判断当前文件大小是否过大
                fclose(logger->m_file_handler);  //把之前的文件关闭
                log_file_name = ss.str() + std::to_string(logger->m_no++);
                logger->m_file_handler = fopen(log_file_name.c_str(), "a"); //重新打开,追加写入
                logger->m_reopen_flag = false;
            }
            for (auto & i : tmp) {
                if (!i.empty())
                {
                    fwrite(i.c_str(), 1, i.length(), logger->m_file_handler);
                }
            }
            fflush(logger->m_file_handler);   //刷新，让日志写到磁盘当中
            if (logger->m_stop_flag) {
                return NULL;
            }
        }
        return NULL;
    }

    void AsyncLogger::stop() {
        m_stop_flag = true;
    }
    void AsyncLogger::flush() {
        if (m_file_handler) {
            fflush(m_file_handler);
        }
    }

    void AsyncLogger::pushLogBuffer(std::vector<std::string> & vec)
    {
        ScopeMutex<Mutex> lock(m_mutex);
        m_buffer.push(vec);

        //这时候需要唤醒异步日志线程
        pthread_cond_signal(&m_condition);  //返回到152行
        lock.unlock();
        // printf("pthread_cond_signal");

    }
}






