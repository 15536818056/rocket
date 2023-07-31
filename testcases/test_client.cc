#include <pthread.h>
#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <memory>
#include <unistd.h>
#include <arpa/inet.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/tcp/net_addr.h"

void test_connect()
{
    // 1.调用connect连接 server
    // 2.send一个字符串
    // 3.等待read返回结果
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        ERRORLOG("invalid listenfd %d", fd);
        exit(0);
    }
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(11111);
    inet_aton("127.0.0.1", &server_addr.sin_addr);
    int rt = connect(fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    DEBUGLOG("connect success");

    std::string msg = "hello rocket!";
    rt = write(fd, msg.c_str(), msg.length());
    DEBUGLOG("success write %d bytes, [%s]", rt, msg.c_str());
    char buf[BUFSIZ];   //https://blog.csdn.net/ImwaterP/article/details/119140242
    rt = read(fd, buf, sizeof(buf));
    DEBUGLOG("success read %d bytes, [%s]", rt, std::string(buf).c_str()); 
}

void test_tcp_client() {
    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 11111);
    rocket::TcpClient client(addr);
    client.connect([=](){
        DEBUGLOG("connect to [%s] success", addr->toString().c_str());
    });
}

int main()
{
    rocket::Config::SetGlobalConfig("conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();

    // test_connect();
    test_tcp_client();
    return 0;
}