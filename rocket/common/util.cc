#include <arpa/inet.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <string.h>
#include "rocket/common/util.h"

namespace rocket
{
    static int g_pid = 0;   //进程ID的全局变量
    static thread_local int t_thread_id = 0;    //线程的局部变量,thread_local 表示该变量是线程本地存储的私有变量，即每个线程都有一份独立的拷贝，不同线程之间互不干扰

    pid_t getPid()
    {
        if (g_pid != 0)
        {
            return g_pid;
        }
        return getpid();
    }
    pid_t getThreadId()
    {
        if (t_thread_id != 0)
        {
            return t_thread_id;
        }
        return syscall(SYS_gettid);
    }

    int64_t getNowMs() {
        timeval val;
        gettimeofday(&val, NULL);
        return val.tv_sec * 1000 +  val.tv_usec / 1000;
    }

    //这个函数的作用是将网络字节序转换为主机字节序
    int32_t getInt32FromNetByte(const char * buf)
    {
        int32_t re;
        memcpy(&re, buf, sizeof(re));
        return ntohl(re);
    }




}