#ifndef ROCKET_NET_CODER_TINYPB_CODER_H
#define RCOEKT_NET_CODER_TINYPB_CODER_H

#include "rocket/net/coder/abstract_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"

namespace rocket  
{

class TinyPBCoder : public AbstractCoder 
{
public:
    TinyPBCoder() {}
    ~TinyPBCoder() {}
public:
    void encode(std::vector<AbstractProtocol::s_ptr> & messages, TcpBuffer::s_ptr out_buffer);
    void decode(std::vector<AbstractProtocol::s_ptr> & out_messages, TcpBuffer::s_ptr buffer);

private:
    //传入PB对象返回字节流及长度
    const char * encodeTinyPB(std::shared_ptr<TinyPBProtocol> message, int &len);
};



















}



#endif