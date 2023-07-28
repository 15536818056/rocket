#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/eventloop.h"

int main()
{
    rocket::Config::SetGlobalConfig("conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();

    rocket::EventLoop * eventloop = new rocket::EventLoop();
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        ERRORLOG("listenfd = -1");
        exit(0);
    }
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(12345);
    addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &addr.sin_addr);

    int rt = bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)); //listenfd套接字监听本地12345端口
    if (rt != 0)
    {
        ERRORLOG("bind error");
        exit(1);
    }
    rt = listen(listenfd, 100);
    if (rt != 0)
    {
        ERRORLOG("listen error");
        exit(1);
    }
    rocket::FdEvent event(listenfd);    //监听该套接字的可读事件
    event.listen(rocket::FdEvent::IN_EVENT, [=](){  //读回调函数
        sockaddr_in peer_addr;
        socklen_t addr_len = sizeof(peer_addr);
        memset(&peer_addr, 0, sizeof(peer_addr));
        int clientfd = accept(listenfd, reinterpret_cast<sockaddr*>(&peer_addr),&addr_len); 
        DEBUGLOG("success get client fd[%d], peer addr [%s:%d]", clientfd,inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
    });

    eventloop->addEpollEvent(&event);   //如果有可读事件，会在epoll_wait上返回，拿到可读事件，然后调用读回调函数
    eventloop->loop();



    return 0;
}
