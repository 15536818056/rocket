#ifndef ROCKET_NET_TCP_TCP_CLIENT_H
#define ROCKET_NET_TCP_TCP_CLIENT_H

#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/tcp/abstract_protocol.h"

namespace rocket {

class TcpClient {
public:
    TcpClient(NetAddr::s_ptr peer_addr);   //对端地址 
    ~TcpClient();
    //异步的进行connect,因此需要一个回调来获取结果
    //如果connect成功，done会被执行
    //注意eventloop下，所有的读、写、connect都是异步的
    void connect(std::function<void()> done);

    //异步的发送Message,Message是什么都行
    //如果发送message成功，会调用done函数,函数的入参就是message对象
    void writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)>done);

    //异步的读取Message
    //如果读取message成功，会调用done函数，函数的入参就是message对象 
    void readMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)>done);

private:
    NetAddr::s_ptr m_peer_addr;     //对端地址
    EventLoop * m_event_loop {NULL}; //成员变量如果是指针，要给初始值NULL，否则会引起段错误
    int m_fd {-1};  //对端fd
    FdEvent * m_fd_event {NULL};
    TcpConnection::s_ptr m_connection;  //利用connection对象实现数据收发
};

    
}
#endif