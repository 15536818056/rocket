#ifndef ROCKET_NET_TIMER_H
#define ROCKET_NET_TIMER_H

#include <map>
#include "rocket/net/fd_event.h"
#include "rocket/net/timer_event.h"
#include "rocket/common/log.h"

namespace rocket {
class Timer : public FdEvent {
public:
    Timer();
    ~Timer(); 
    void addTimerEvent(TimerEvent::s_ptr event);
    void deleteTimerEvent(TimerEvent::s_ptr event);
    void onTimer(); //当发生了IO事件后，eventloop会执行这个回调函数,绑定到read call_back上面就可以
private:
    void resetArriveTime();
private:
    std::multimap<int64_t, TimerEvent::s_ptr> m_pending_events; //到达事件和定时时间的map
    Mutex m_mutex;  //可以替换成读写锁，性能更好
};

}


#endif