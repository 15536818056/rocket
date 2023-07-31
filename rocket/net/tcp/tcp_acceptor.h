#ifndef ROCKET_NET_TCP_ACCEPTOR_H
#define ROCKET_NET_TCP_ACCEPTOR_H 

#include <memory>
#include "rocket/net/tcp/net_addr.h"

namespace rocket {

class TcpAcceptor {
public:
    typedef std::shared_ptr<TcpAcceptor> s_ptr;//由于是多线程，使用智能指针可以保证线程安全    typedef std::shared_ptr<TcpAcceptor> s_ptr;
    TcpAcceptor(NetAddr::s_ptr local_addr);
    ~TcpAcceptor();
    std::pair<int, NetAddr::s_ptr> accept();
    int getListenFd();
private:
    //服务端监听的地址 addr -> ip:port
    NetAddr::s_ptr m_local_addr;
    int m_family {-1};

    //监听套接字
    int m_listenfd {-1};
};

}


#endif