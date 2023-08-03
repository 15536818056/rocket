#ifndef ROCKET_NET_TCP_TCP_CONNECTION_H
#define ROCKET_NET_TCP_TCP_CONNECTION_H

#include <memory>
#include <map>
#include <queue>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_buffer.h"
#include "rocket/net/io_thread.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/coder/abstract_coder.h"
#include "rocket/net/rpc/rpc_dispatcher.h"

namespace rocket
{
    class RpcDispatcher;
    enum TcpState
    {
        // 当前连接的状态
        NotConnected = 1,
        Connected = 2,
        HalfClosing = 3, // 半连接
        Closed = 4,
    };

    enum TcpConnectionType {    //调用connectin可能是服务器端，也可能是客户端
        TcpConnectionByServer = 1,    //作为服务端使用，代表跟对端客户端连接
        TcpConnectionByClient = 2,    //作为客户端使用，代表跟对端服务端的连接
    };

    class TcpConnection
    {
    public:
        typedef std::shared_ptr<TcpConnection> s_ptr;
        TcpConnection(EventLoop * event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr, NetAddr::s_ptr local_addr,TcpConnectionType type = TcpConnectionByServer);
        // 参数的含义分别为，代表哪一个IO线程，哪一个客户端,缓冲区大小以及对端地址
        ~TcpConnection();
        void onRead();
        void excute();
        void onWrite();
        void setState(const TcpState state);
        TcpState getState();
        void clear();   // 清除连接
        void shutdown();    //服务器主动关闭连接,处理无效的TCP连接,服务端主动close
        void setConnectionType(TcpConnectionType type);
        //启动监听可写事件
        void listenWrite();
        //启动监听可读事件
        void listenRead();
        void pushSendMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done);
        void pushReadMessage(const std::string & req_id, std::function<void(AbstractProtocol::s_ptr)> done);
        NetAddr::s_ptr getLocalAddr();
        NetAddr::s_ptr getPeerAddr();

    private:
        EventLoop *m_event_loop {NULL};   // 代表持有该连接的IO线程
        NetAddr::s_ptr m_peer_addr;
        NetAddr::s_ptr m_local_addr;
        TcpBuffer::s_ptr m_in_buffer;  // 接收缓冲区
        TcpBuffer::s_ptr m_out_buffer; // 发送缓冲区
        FdEvent *m_fd_event{NULL};
        AbstractCoder * m_coder {NULL};
        TcpState m_state;
        int m_fd{0};
        TcpConnectionType m_connection_type {TcpConnectionByServer};
        //写回调
        std::vector<std::pair<AbstractProtocol::s_ptr, std::function<void(AbstractProtocol::s_ptr)>>> m_write_dones;
        //读回调,key 为req_id
        std::map<std::string, std::function<void(AbstractProtocol::s_ptr)>> m_read_dones;

    };

}

#endif