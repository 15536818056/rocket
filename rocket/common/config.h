#ifndef ROCKET_COMMON_CONFIG_H
#define ROCKET_COMMON_CONFIG_H

#include <string>

namespace rocket
{
    class Config
    {
    public:
        Config(const char * xmlfile);   //选择xml是因为简单、容易读取，可读性能接受,yaml和json也可以
    public:
        static Config * GetGlobalConfig();
        static void SetGlobalConfig(const char * xmlfile);

    public:
        std::string m_log_level;
    };


}


#endif