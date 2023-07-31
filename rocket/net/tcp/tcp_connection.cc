#include <unistd.h>
#include <string.h>
#include "rocket/net/tcp/tcp_connection.h"

namespace rocket
{
    TcpConnection::TcpConnection(EventLoop * event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr)
        : m_event_loop(event_loop), m_peer_addr(peer_addr), m_state(NotConnected), m_fd(fd)
    {
        m_in_buffer = std::make_shared<TcpBuffer>(buffer_size);
        m_out_buffer = std::make_shared<TcpBuffer>(buffer_size);
        m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(fd);
        m_fd_event->setNonBlock();
        // 发生可读事件后，会调用read函数
        m_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpConnection::onRead, this));
        m_event_loop->addEpollEvent(m_fd_event);
    }
    TcpConnection::~TcpConnection()
    {
        DEBUGLOG("~TcpConnection");
    }
    void TcpConnection::onRead()
    {
        // 1.从socket缓冲区调用系统的read函数读取字节流到in_buffer里面
        if (m_state != Connected)
        {
            ERRORLOG("onRead error, client has already disconnected, addr[%s], clienfd[%d]", m_peer_addr->toString().c_str(), m_fd);
            return;
        }
        // 一次性读完,LT模式
        bool is_read_all = false;
        bool is_close = false;
        while (!is_read_all)
        {
            if (m_in_buffer->writeAble() == 0)
            {
                m_in_buffer->resizeBuffer(2 * m_in_buffer->m_buffer.size());
            }
            int read_count = m_in_buffer->writeAble();   // 获取当前能够写入的字节数
            int write_index = m_in_buffer->writeIndex(); // 写入的起始位置
            int rt = ::read(m_fd, &(m_in_buffer->m_buffer[write_index]), read_count);
            DEBUGLOG("success read %d bytes form add[%s], client fd [%d]", rt, m_peer_addr->toString().c_str(), m_fd);
            if (rt > 0)
            { // 读取成功
                m_in_buffer->moveWriteIndex(rt);
                if (rt == read_count)
                {
                    // 没读完
                    continue;
                }
                else if (rt < read_count)
                {
                    // 说明缓冲区已经读完
                    is_read_all = true;
                    break;
                }
            }
            else if (rt == 0)
            {
                // 说明对端已经关掉
                is_close = true;
                break;
            }
            else if (rt == -1 && errno == EAGAIN)
            {
                is_read_all = true;
                break;
            }
        }
        if (is_close)
        {
            // TODO: 如果对端关闭，处理关闭连接
            INFOLOG("peer closed, peer addr [%s], clientfd [%d]", m_peer_addr->toString().c_str(), m_fd);
            clear();
            return; //不去继续执行ececute()
        }
        if (!is_read_all)
        {
            ERRORLOG("not read all data");
        }
        // TODO: 简单的echo,后面补充rpc协议的解析
        excute();
    }
    void TcpConnection::excute()
    {
        // 将RPC请求执行业务逻辑，获取RPC响应，再把RPC响应发送回去
        std::vector<char> tmp;
        int size = m_in_buffer->readAble();
        tmp.resize(size);
        m_in_buffer->readFromBuffer(tmp, size);
        // 先原样返回
        std::string msg;
        memset(&msg, 0, sizeof(msg));
        for (size_t i = 0; i < tmp.size(); i++)
        {
            msg += tmp[i];
        }
        INFOLOG("success get request [%s] from client[%s]", msg.c_str(), m_peer_addr->toString().c_str());
        m_out_buffer->writeToBuffer(msg.c_str(), msg.length());
        m_fd_event->listen(FdEvent::OUT_EVENT, std::bind(&TcpConnection::onWrite, this));
        m_event_loop->addEpollEvent(m_fd_event);
    }
    void TcpConnection::onWrite()
    {
        // 将当前out_buffer里面的数据全部分送给client
        if (m_state != Connected)
        {
            ERRORLOG("onWrite error, client has already disconnected, addr[%s], clienfd[%d]", m_peer_addr->toString().c_str(), m_fd);
            return;
        }
        bool is_write_all = false;
        while (true)
        {
            if (m_out_buffer->readAble() == 0) // 说明没有数据可以发送了
            {
                DEBUGLOG("no data need to send to client [%s]", m_peer_addr->toString().c_str());
                is_write_all = true;
                break;
            }
            int write_size = m_out_buffer->readAble();
            int read_index = m_out_buffer->readIndex();
            int rt = write(m_fd, &(m_out_buffer->m_buffer[read_index]), write_size);
            if (rt >= write_size)
            {
                // 说明数据发送成功
                DEBUGLOG("success sending all data, now no data need to send to cilent [%s]", m_peer_addr->toString().c_str());
                is_write_all = true;
                break;
            }
            if (errno == EAGAIN && rt == -1)
            { // 表示发送缓冲区满了,不能发送数据了
                // 这种情况等下次fd可写的时候再次发送数据即可
                DEBUGLOG("write data error, errno = EAGAIN and rt == -1");
                break;
            }
        }
        if (is_write_all)
        {
            m_fd_event->cancel(FdEvent::OUT_EVENT);
            m_event_loop->addEpollEvent(m_fd_event);
        }
    }
    void TcpConnection::setState(const TcpState state)
    {
        m_state = state;
    }
    TcpState TcpConnection::getState()
    {
        return m_state;
    }
    void TcpConnection::clear()
    {
        // 服务器去处理一些关闭连接后的清理动作
        if (m_state == Closed)
        {
            return;
        }
        m_fd_event->cancel(FdEvent::IN_EVENT);
        m_fd_event->cancel(FdEvent::OUT_EVENT);
        m_event_loop->delEpollEvent(m_fd_event); // 不会监听读写数据了
        m_state = Closed;
    }
    void TcpConnection::shutdown()
    {
        if (m_state == Closed && m_state == NotConnected)
        {
            return;
        }
        // 处于半关闭状态
        m_state = HalfClosing;
        // 调用系统shutdown关闭读写,意味着服务器不会再对这个fd进行读写操作了
        // 发送 FIN，触发了四次挥手的第一个阶段
        // 等到对端返回FIN，才是挥手结束
        // 当fd发生可读事件，但是可读的数据为0，即对端发送了FIN,服务器进入timerwait
        ::shutdown(m_fd, SHUT_RDWR);
    }

    void TcpConnection::setConnectionType(TcpConnectionType type) {
        m_connection_type = type;
    }

}