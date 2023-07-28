#include <sys/timerfd.h>
#include <string.h>
#include "rocket/net/timer.h"
#include "rocket/common/util.h"

namespace rocket {
    Timer::Timer() : FdEvent() {
        //该m_fd到达时间后会产生读事件
        m_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC); 
        DEBUGLOG("timer fd = %d", m_fd);
        //把fd的可读事件放到eventloop上监听
        listen(FdEvent::IN_EVENT, std::bind(&Timer::onTimer, this));
    }
    Timer::~Timer() {
        
    } 

    void Timer::onTimer() { //触发可读事件后，执行的回调函数
    //处理缓冲区数据，防止下一次继续触发可读事件
        char buf[8];
        while (1) {
            if ((read(m_fd, buf, 8) == -1) && errno == EAGAIN) {
                break;
            }
        }
        //执行定时任务
        int64_t now = getNowMs();
        std::vector<TimerEvent::s_ptr> tmps;
        std::vector<std::pair<int64_t, std::function<void()>>> tasks;   //需要执行的任务
        ScopeMutex<Mutex> lock(m_mutex);
        auto it = m_pending_events.begin();

        for (it = m_pending_events.begin(); it != m_pending_events.end(); ++it) {
            if ((*it).first <= now ) {   //该定时任务到期了
                if (!(*it).second->isCancel()) {
                    tmps.push_back((*it).second);
                    tasks.push_back(std::make_pair((*it).second->getArriveTime(), (*it).second->getCallBack()));
                }
            } else {
                break;      //第一个没到期，说明后面的都没到期  
            }        
        }
        m_pending_events.erase(m_pending_events.begin(), it);   //由于已经缓存了，就清空，防止重复执行
        lock.unlock();

        //把重复的Event事件再次添加进去
        for (auto i = tmps.begin(); i != tmps.end(); ++i) {
            if ((*i)->isRepeated()) {   //周期性的定时任务
                //调整arriveTime
                (*i)->resetArriveTime();
                addTimerEvent(*i);
            }
        }
        resetArriveTime();
        for (auto i : tasks) {
            if (i.second) {
                i.second();
            }
        }
    }
    void Timer::resetArriveTime() {
        ScopeMutex<Mutex> lock(m_mutex);
        auto tmp = m_pending_events;
        lock.unlock();

        if (tmp.size() == 0) {   //map为空，没有定时任务,退出就行了
            return;
        }
        int64_t now = getNowMs();
        //取第一个定时任务的时间
        auto it = tmp.begin();
        int64_t inteval = 0;
        if (it->second->getArriveTime() > now) {    //说明第一个定时任务时间还没到
            inteval = it->second->getArriveTime() - now;
        } else { //说明定时任务已经过期了
           inteval = 100;   //100ms后执行定时任务,拯救一下
        }

        timespec ts;
        memset(&ts, 0, sizeof(ts));
        ts.tv_sec = inteval / 1000;
        ts.tv_nsec = (inteval % 1000) * 1000000;    //获得纳秒

        itimerspec value;
        memset(&value, 0, sizeof(value));
        value.it_value = ts;
        int rt = timerfd_settime(m_fd, 0, &value, NULL);
        if (rt != 0) {
            ERRORLOG("timer fd_settime errno=%d, error=%s\n", errno, strerror(errno));
        }
        DEBUGLOG("timer reset to %lld", now + inteval);
    }

    void Timer::addTimerEvent(TimerEvent::s_ptr event) {
        bool is_reset_timerfd = false;  //默认不需要重新设置
        ScopeMutex<Mutex> lock(m_mutex);
        if (m_pending_events.empty())   //当前事件为空
        {
            is_reset_timerfd = true;    //设置fd时间，否则不会触发
        } else {
            auto it = m_pending_events.begin();
            if ((*it).second->getArriveTime() > event->getArriveTime()) {
                //需要插入的定时任务指定时间比任务队列中所有定时任务指定时间都早,需要重新设置
                is_reset_timerfd = true;
            }
        }
        m_pending_events.emplace(event->getArriveTime(), event);
        lock.unlock();
        if (is_reset_timerfd) {
            resetArriveTime();
        }
    }
    void Timer::deleteTimerEvent(TimerEvent::s_ptr event) {
        event->setCancel(true);    // 确保后面的定时器不会触发
        ScopeMutex<Mutex> lock(m_mutex);
        auto begin = m_pending_events.lower_bound(event->getArriveTime());
        auto end = m_pending_events.upper_bound(event->getArriveTime());

        auto it = begin;
        for (it = begin; it != end; it++) {
            if (it->second == event) {
                break;
            }
        }
        if (it != end) {
            m_pending_events.erase(it);
        }
        lock.unlock();
        DEBUGLOG("success delete TimerEvent at time %lld", event->getArriveTime());
    }

}

