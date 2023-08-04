#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "rocket/common/log.h"
#include "rocket/common/error_code.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/tcp/net_addr.h"

namespace rocket
{

    TcpClient::TcpClient(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr)
    {
        m_event_loop = EventLoop::GetCurrentEventLoop();
        m_fd = socket(peer_addr->getFamily(), SOCK_STREAM, 0);
        if (m_fd < 0)
        {
            ERRORLOG("TcpClient::TcpClient() error, failed to create fd");
            return;
        }
        m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);
        m_fd_event->setNonBlock();
        m_connection = std::make_shared<TcpConnection>(m_event_loop, m_fd, 128, peer_addr, nullptr, TcpConnectionByClient);
        m_connection->setConnectionType(TcpConnectionByClient);
    }

    TcpClient::~TcpClient()
    {
        // 关闭套接字
        DEBUGLOG("TcpClient::~TcpClient()");
        if (m_fd > 0)
        {
            close(m_fd);
        }
    }
    // 异步的进行connect
    // 如果connect成功，done会被执行
    void TcpClient::connect(std::function<void()> done)
    {
        int rt = ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
        if (rt == 0)
        {
            DEBUGLOG("connect [%s] success", m_peer_addr->toString().c_str());
            m_connection->setState(Connected);
            initLocalAddr();
            if (done)
            {
                done();
            }
        }
        else if (rt == -1)
        {
            if (errno == EINPROGRESS)
            {
                // epoll监听可写事件，监听错误码
                m_fd_event->listen(
                    FdEvent::OUT_EVENT,
                    [=]()
                    {
                        /*
                            判断connect能否成功有两种方法
                            1. getsockopt 获得error
                            2. 另一种方式是连接成功后再调用一次connect，如果返回值=0，或者<0 errno==EISCONN
                        */
                        int rt = ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
                        if ((rt < 0 && errno == EISCONN) || (rt == 0))
                        {
                            DEBUGLOG("connect [%s] sussess", m_peer_addr->toString().c_str());
                            initLocalAddr();
                            m_connection->setState(Connected);
                        }
                        else
                        {
                            if (errno == ECONNREFUSED)
                            {
                                m_connect_error_code = ERROR_PEER_CLOSE;
                                m_connect_error_info = "connect refused, sys error = " + std::string(strerror(errno));
                            }
                            else
                            {
                                m_connect_error_code = ERROR_FAILED_CONNECT;
                                m_connect_error_info = "connect unkonwn error, sys error = " + std::string(strerror(errno));
                            }
                            ERRORLOG("connect errror, errno=%d, error=%s", errno, strerror(errno));
                            // 连接失败，关闭套接字，重新申请一个
                            close(m_fd);
                            m_fd = socket(m_peer_addr->getFamily(), SOCK_STREAM, 0);
                        }
                        // int error = 0;
                        // socklen_t error_len = sizeof(error);
                        // getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &error_len); //捕获错误码信息及其长度
                        // if (error == 0) {
                        //     //错误码是0代表连接创建成功
                        //     DEBUGLOG("connect [%s] success", m_peer_addr->toString().c_str());
                        //     initLocalAddr();
                        //     m_connection->setState(Connected);
                        // } else {
                        //     m_connect_error_code = ERROR_FAILED_CONNECT;
                        //     m_connect_error_info = "connect error, sys error = " + std::string(strerror(errno));
                        //     ERRORLOG("connect error, errno = %d, error = %s", errno, strerror(errno));
                        // }
                        // 连接完成后需要去掉可写事件的监听，不然会一直触发
                        m_event_loop->delEpollEvent(m_fd_event);
                        DEBUGLOG("now begin to done");
                        // 如果连接成功，才会执行回调函数
                        if (done)
                        {
                            done();
                        }
                    });

                m_event_loop->addEpollEvent(m_fd_event);
                if (!m_event_loop->isLooping())
                {
                    m_event_loop->loop();
                }
            }
            else
            {
                ERRORLOG("connect error, errno = %d, error = %s", errno, strerror(errno));
                m_connect_error_code = ERROR_FAILED_CONNECT;
                m_connect_error_info = "connect error, sys error = " + std::string(strerror(errno));

                if (done)
                {
                    done();
                }
            }
        }
    }
    void TcpClient::stop()
    {
        if (m_event_loop->isLooping())
        {
            m_event_loop->stop();
        }
    }

    void TcpClient::writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done)
    {
        // 1.把message对象写入到Connection的buffer中,done也写入
        // 2.启动connection可写事件监听
        m_connection->pushSendMessage(message, done);
        m_connection->listenWrite();
    }

    void TcpClient::readMessage(const std::string &req_id, std::function<void(AbstractProtocol::s_ptr)> done)
    {
        // 1.监听可读事件
        // 2.从buffer中decode得到message对象,判断是否req_id相等，相等则成功,执行其回调
        m_connection->pushReadMessage(req_id, done);
        m_connection->listenRead();
    }
    int TcpClient::getConnectErrorCode()
    {
        return m_connect_error_code;
    }
    std::string TcpClient::getConnectErrorInfo()
    {
        return m_connect_error_info;
    }
    NetAddr::s_ptr TcpClient::getPeerAddr()
    {
        return m_peer_addr;
    }
    NetAddr::s_ptr TcpClient::getLocalAddr()
    {
        return m_local_addr;
    }

    void TcpClient::initLocalAddr()
    {
        sockaddr_in local_addr;
        socklen_t len = sizeof(local_addr);
        int ret = getsockname(m_fd, reinterpret_cast<sockaddr *>(&local_addr), &len);
        if (ret != 0)
        {
            ERRORLOG("initLocalAddr error, getsockname error, errno = %d, error = %s", errno, strerror(errno));
            return;
        }
        m_local_addr = std::make_shared<IPNetAddr>(local_addr);
    }
}