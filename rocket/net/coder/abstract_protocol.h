#ifndef ROCKET_NET_ABSTRACT_PROTOCOL_H
#define ROCKET_NET_ABSTRACT_PROTOCOL_H

#include <memory>
#include "rocket/net/tcp/tcp_buffer.h"

//抽象的协议基类
namespace rocket {

struct AbstractProtocol: public std::enable_shared_from_this<AbstractProtocol> {
public:
    //为了方便使用，定义一个智能指针
    typedef std::shared_ptr<AbstractProtocol> s_ptr;


    virtual ~AbstractProtocol() {}

public:
    //请求号，唯一标识一个请求,或者响应;一个rpc请求和响应的标识应该一致
    std::string m_msg_id;


};



}



#endif