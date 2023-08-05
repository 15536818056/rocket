#include "rocket/common/run_time.h"


namespace  rocket
{
    thread_local RunTime * t_run_time = NULL;   //多线程，每个线程独有的
    RunTime * RunTime::GetRunTime() {
       if (t_run_time) {
            return t_run_time;
       }
       t_run_time = new RunTime();
       return t_run_time;
    }    




}
