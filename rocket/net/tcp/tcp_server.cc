#include "rocket/net/tcp/tcp_server.h"
#include "rocket/common/config.h"


namespace rocket {
    TcpServer::TcpServer(NetAddr::s_ptr local_addr): m_local_addr(local_addr) {
        init();
        INFOLOG("rocket TcpServer listen  success on [%s]", m_local_addr->toString().c_str());
    }
    TcpServer::~TcpServer() {
        if (m_main_event_loop) {
            delete m_main_event_loop;
            m_main_event_loop = NULL;
        }
    }
    //设置回调函数
    void TcpServer::init() {
        m_acceptor = std::make_shared<TcpAcceptor>(m_local_addr);
        m_main_event_loop = EventLoop::GetCurrentEventLoop(); 
        //可以把所有连接放到一个公共队列中，每个IO线程从公共队列中取
        m_io_thread_group = new IOThreadGroup(Config::GetGlobalConfig()->m_io_threads);
        m_listen_fd_event =  new FdEvent(m_acceptor->getListenFd());
        //当listenfd可读的时候就会调用onAccept函数
        m_listen_fd_event->listen(FdEvent::IN_EVENT, std::bind(&TcpServer::onAccept, this));
        m_main_event_loop->addEpollEvent(m_listen_fd_event);
    }

    void TcpServer::onAccept() {
        auto re = m_acceptor->accept();
        int client_fd = re.first;
        NetAddr::s_ptr peer_addr = re.second;
        m_client_counts++;
        //把client_fd添加到任意IO线程里面
        IOThread * io_thread = m_io_thread_group->getIOThread();
        TcpConnection::s_ptr connection = std::make_shared<TcpConnection>(io_thread->getEventLoop(), client_fd, 128, peer_addr, m_local_addr); 
        /*
        老师最后一节留的任务，就是加入定时任务，通过状态来判断是否要析构connection 
        */
        connection->setState(Connected);
        m_client.insert(connection);
        INFOLOG("TcpServer succ get client, fd = %d", client_fd);
    }

    void TcpServer::start() {
        //开启主线程和IO线程的eventloop
        m_io_thread_group->start();
        m_main_event_loop->loop();  //主线程阻塞在这里
    }


}
