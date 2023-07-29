#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <string.h>
#include "rocket/net/eventloop.h"
#include "rocket/common/util.h"

//首先判断之前是否添加过fd，如果之前添加过，就变成修改
#define ADD_TO_EPOLL() \
    auto it = m_listen_fds.find(event->getFd());    \ 
    int op = EPOLL_CTL_ADD; \
    if (it != m_listen_fds.end()) {  \
        op = EPOLL_CTL_MOD;  }  \ 
    epoll_event tmp = event->getEpollEvent(); \
    int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &tmp); \
    if (rt == -1) { \
        ERRORLOG("failed epoll_ctl when add fd %d, errno=%d, error=%s", event->getFd(), errno, strerror(errno)); \
    } \
    m_listen_fds.insert(event->getFd()); \
    DEBUGLOG("add event success, fd[%d]", event->getFd()); \

#define DELETE_TO_EPOLL() \
    auto it = m_listen_fds.find(event->getFd()); \
    if (it == m_listen_fds.end()) { \
        return; \
    } \
    int op = EPOLL_CTL_DEL; \
    epoll_event tmp = event->getEpollEvent(); \
    int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &tmp); \
    if (rt == -1) { \
        ERRORLOG("failed epoll_ctl when add fd, errno=%d, error=%s", errno, strerror(errno)); \
    } \
    m_listen_fds.erase(event->getFd()); \
    DEBUGLOG("delete event success, fd[%d]", event->getFd()); \



namespace rocket {

static thread_local EventLoop * t_current_eventloop = NULL;     //利用线程局部变量判断当前线程有没有创建过EventLoop
static int g_epoll_max_timeout = 10000; //10s
static int g_epoll_max_events = 10;     //单次最大的event监听事件

EventLoop::EventLoop() {
    if (t_current_eventloop != NULL) {
        ERRORLOG("failed to create event loop, this thread has created event loop");
        exit(0);    //属于异常，必须退出程序
    }

    m_thread_id = getThreadId();
    m_epoll_fd = epoll_create(1024);    //参数随便给一个正数就行，Linux 2.6.8以后这个参数就没什么用了
    if (m_epoll_fd == -1) {
        ERRORLOG("failed to create event loop, epoll_create error, error info[%d]\n", errno);
        exit(0);    //属于异常，必须退出程序
    }
    m_wakeup_fd = eventfd(0, EFD_NONBLOCK); //设置为非阻塞
    if (m_wakeup_fd < 0) {
        ERRORLOG("failed to create event loop, epoll_create error, error info[%d]\n", errno);
        exit(0);    //属于异常，必须退出程序
    }

    initWakeUpFdEvent();    
    initTimer();
    INFOLOG("succ create event loop in thread %d\n", m_thread_id);
    t_current_eventloop = this;
}
EventLoop::~EventLoop() {   //一般来说调用不到
    close(m_epoll_fd);
    if (m_wakeup_fd_event) {
        delete m_wakeup_fd_event;
        m_wakeup_fd_event = NULL;
    }
    if (m_timer) {
        delete m_timer;
        m_timer = NULL;
    }
}

void EventLoop::initTimer() {
    m_timer = new Timer();
    addEpollEvent(m_timer);
}

void EventLoop::addTimerEvent(TimerEvent::s_ptr event) {
    m_timer->addTimerEvent(event);
}

void EventLoop::initWakeUpFdEvent() {
    m_wakeup_fd_event = new WakeUpFdEvent(m_wakeup_fd);
    m_wakeup_fd_event->listen(FdEvent::IN_EVENT, [=](){
        char buf[8];
        while (read(m_wakeup_fd, buf, 8) != -1 && errno != EAGAIN) {
        }
        DEBUGLOG("read full bytes from wakeup fd[%d]", m_wakeup_fd);
    });
    addEpollEvent(m_wakeup_fd_event);
}

void EventLoop::loop() {
    while (!m_stop_flag) {
        ScopeMutex<Mutex> lock(m_mutex);
        std::queue<std::function<void()>> tmp_tasks;
        m_pending_tasks.swap(tmp_tasks);
        lock.unlock();
        while (!tmp_tasks.empty()) {
            std::function<void()> cb = tmp_tasks.front();
            tmp_tasks.pop();
            if (cb) {
                cb();
            }
        }
        
        //如果有定时任务需要执行，那么执行 
        //1.如何判断定时任务是否需要执行？ now()>TimerEvent.arrtive_time
        //2.如何在arrtive_time让epoll_wait返回,也就是如何让event_loop监听arrive_time

        int timeout = g_epoll_max_timeout;   
        epoll_event result_events [g_epoll_max_events];
        //DEBUGLOG("now begin to epoll_wait");
        int rt = epoll_wait(m_epoll_fd, result_events, g_epoll_max_events, timeout);
        DEBUGLOG("now end epoll_wait, rt = %d", rt);
        if (rt < 0) {
            ERRORLOG("epoll_wait error, errno = ", errno);
        } else {
            for (int i = 0; i < rt; ++i) {
                epoll_event trigger_event = result_events[i];
                FdEvent * fd_event = static_cast<FdEvent *>(trigger_event.data.ptr);
                if (fd_event == NULL) {
                    continue;
                }
                if (trigger_event.events & EPOLLIN) {
                    DEBUGLOG("fd %d trigger EPOLLIN event", fd_event->getFd());
                    addTask(fd_event->handler(FdEvent::IN_EVENT));  //取读回调函数，再把回调函数放到执行队列里面
                }
                if (trigger_event.events & EPOLLOUT) {
                    DEBUGLOG("fd %d trigger EPOLLOUT event", fd_event->getFd());
                    addTask(fd_event->handler(FdEvent::OUT_EVENT));
                }
            }
            
        }
    }
}
void EventLoop::wakeup() {
    INFOLOG("wake up fd = %d", m_wakeup_fd);
    m_wakeup_fd_event->wakeup();
}
void EventLoop::stop() {

}

void EventLoop::dealWakeup(){

}
void EventLoop::addEpollEvent(FdEvent * event) {
    if (isInLoopThread()) {
        ADD_TO_EPOLL();
    } else {
        //如果不是当前线程，要用回调函数
        auto cb = [=]() {
            ADD_TO_EPOLL();
        };
        addTask(cb, true);
    }
}
void EventLoop::delEpollEvent(FdEvent * event) {
    if (isInLoopThread()) {
       DELETE_TO_EPOLL();
    } else {
        auto cb = [=] () {
            DELETE_TO_EPOLL();
        };
        addTask(cb, true);
    }
}
void EventLoop::addTask(std::function<void()> cb, bool is_wake_up) {
    ScopeMutex<Mutex> lock(m_mutex);
    m_pending_tasks.push(cb);
    lock.unlock();
    if (is_wake_up) {
        wakeup();
    }    
}

bool EventLoop::isInLoopThread() {
    return getThreadId() == m_thread_id;
}
}


