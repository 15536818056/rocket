#ifndef ROCKET_NET_EVENTLOOP_H
#define ROCKET_NET_EVENTLOOP_H

#include <pthread.h>
#include <set>
#include <functional>
#include <queue>
#include "rocket/net/fd_event.h"
#include "rocket/net/wakeup_fd_event.h"
#include "rocket/common/log.h"
#include "rocket/net/timer.h"

namespace rocket {
class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    void loop();    //核心逻辑
    void wakeup();  //唤醒函数
    void stop();    //正常情况下不会调用，因为服务器会一直运行下去
    void addEpollEvent(FdEvent * event);
    void delEpollEvent(FdEvent * event);
    bool isInLoopThread();  //判断当前是否是EventLoop的IO线程
    void addTask(std::function<void()> cb, bool is_wake_up = false); 
    //将任务添加到pending队列中,当此线程从epoll_wait返回后，自己去执行这些任务,而不是由其他线程执行，将任务封装到回调函数中
    void addTimerEvent(TimerEvent::s_ptr event);
public:
    static EventLoop * GetCurrentEventLoop();    //获得当前线程的EventLoop对象， 如果当前线程没有会去构建一个
    
private:
    void dealWakeup();              //处理wake的函数
    void initWakeUpFdEvent();
    void initTimer();
private:
    pid_t m_thread_id {0};    //该对象每个线程只能有一个
    int m_epoll_fd {0}; //epoll句柄
    int m_wakeup_fd {0};    //唤醒epoll_wait
    WakeUpFdEvent * m_wakeup_fd_event {NULL};
    bool m_stop_flag {false};
    std::set<int> m_listen_fds; //表示当前监听的所有套接字
    std::queue<std::function<void()>> m_pending_tasks;  //所有待执行的任务队列
    Mutex m_mutex;
    Timer * m_timer {NULL};
};

}

#endif
