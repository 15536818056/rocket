#ifndef ROCKET_COMMON_RUN_TIME_H
#define ROCKET_COMMON_RUN_TIME_H

#include <string>
/*
    run time 存储运行过程中的一些信息
*/
namespace rocket
{
class RunTime {
public:

public:
    static RunTime * GetRunTime();


public:
    std::string m_msgid;
    std::string m_method_name;  //当前线程处理的rpc请求方法名


};



}








#endif