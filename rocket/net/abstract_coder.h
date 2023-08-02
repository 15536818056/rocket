#ifndef ROCKET_NET_ABSTRACT_CODER_H
#define ROCKET_NET_ABSTRACT_CODER_H

#include <vector>
#include "rocket/net/tcp/tcp_buffer.h"
#include "rocket/net/abstract_protocol.h"

namespace rocket {
class AbstractCoder {
public:
    //将message对象转换为字节流写入到buffer
    virtual void encode(std::vector<AbstractProtocol::s_ptr> & messages, TcpBuffer::s_ptr out_buffer) = 0;
    //将buffer里面的字节流转换为message对象
    virtual void decode(std::vector<AbstractProtocol::s_ptr> & out_messages, TcpBuffer::s_ptr buffer) = 0;

    virtual ~AbstractCoder() {}

private:

};



}


#endif