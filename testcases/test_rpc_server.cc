#include <pthread.h>
#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <memory>
#include <unistd.h>
#include <arpa/inet.h>
#include <google/protobuf/service.h>
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_server.h"
#include "order.pb.h"
#include "rocket/net/rpc/rpc_dispatcher.h"

class OrderImpl : public Order 
{
public:
    void makeOrder(google::protobuf::RpcController * controller,
                       const ::makeOrderRequest* request,
                       ::makeOrderResponse* response,
                       ::google::protobuf::Closure* done)
    {
        APPDEBUGLOG("start sleep 5");   //APPLOG只能在RPC方法中调用
        sleep(5);
        APPDEBUGLOG("end sleep 5s");
        if (request->price() < 10)
        {
            response->set_ret_code(-1);
            response->set_res_info("short balance");
            return;
        }
        response->set_order_id("20230514");
        APPDEBUGLOG("call makeOrder success");
    }

};

extern rocket::Config * g_config;

int main(int argc, char * argv[])
{
    if (argc != 2) {
        printf("Start test_rpc_server error, arg not 2 \n");
        printf("start like this: \n");
        printf("./test_rpc_server conf/rocket.xml \n");
        return 0;
    }
    rocket::Config::SetGlobalConfig(argv[1]);
    rocket::Logger::InitGlobalLogger();
    //生成service对象
    std::shared_ptr<OrderImpl> service = std::make_shared<OrderImpl>();
    //将service对象注册到rpc服务中
    rocket::RpcDispatcher::GetRpcDispatcher()->registerService(service);

    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", rocket::Config::GetGlobalConfig()->m_port);
    rocket::TcpServer tcp_server(addr);
    tcp_server.start(); 
    return 0;
}