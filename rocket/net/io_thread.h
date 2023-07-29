#ifndef ROCKET_NET_IO_THREAD_H
#define ROCKET_NET_IO_THREAD_H

#include <pthread.h>
#include <semaphore.h>
#include "rocket/net/eventloop.h"

namespace rocket {
class IOThread {
public:
    IOThread();
    ~IOThread();
    EventLoop * getEventLoop();
    void start();
    void join();
public:
    static void *  Main(void * arg);    //静态成员函数、构造函数、析构函数开头都用大写，其他的成员函数都用小写

private:
    pid_t m_thread_id {-1};  //线程id
    pthread_t m_thread {0};    //线程句柄,unsinged int不能是-1
    EventLoop * m_event_loop {NULL};    //当前io线程的loop对象
    sem_t m_init_semaphore; //信号量
    sem_t m_start_semaphore;
    
};
}


#endif