#include "rocket/common/util.h"
#include <sys/syscall.h>

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







}