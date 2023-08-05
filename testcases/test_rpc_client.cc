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
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/rpc/rpc_closure.h"

#include "order.pb.h"


void test_tcp_client()
{
    rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 11111);
    rocket::TcpClient client(addr);
    client.connect([addr, &client]() { // 这里如果是=就会报错
        DEBUGLOG("connect to [%s] success", addr->toString().c_str());
        std::shared_ptr<rocket::TinyPBProtocol> message = std::make_shared<rocket::TinyPBProtocol>();
        message->m_msg_id = "99998888";
        message->m_pb_data = "test pb data";

        makeOrderRequest request;
        request.set_price(100);
        request.set_goods("apple");
        if (!request.SerializeToString(&(message->m_pb_data)))
        {
           ERRORLOG("serilize error"); 
           return;
        }
        message->m_method_name = "Order.makeOrder";




        client.writeMessage(message, [=](rocket::AbstractProtocol::s_ptr msg_ptr)
            { 
                DEBUGLOG("send message success, request [%s]", request.ShortDebugString().c_str()); 
            });
        client.readMessage("99998888", [=](rocket::AbstractProtocol::s_ptr msg_ptr)
            { 
                std::shared_ptr<rocket::TinyPBProtocol> message = std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg_ptr);
                DEBUGLOG("req_id [%s], get response %s", message->m_msg_id.c_str(), message->m_pb_data.c_str()); 
                makeOrderResponse response;
                if (!response.ParseFromString(message->m_pb_data))
                {
                    ERRORLOG("deserilize error");
                    return;
                }
                DEBUGLOG("get response success, response [%s]", response.ShortDebugString().c_str());
            });
      
    });
}

void test_rpc_channel()
{
    // rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 11111);
    // std::shared_ptr<rocket::RpcChannel> channel = std::make_shared<rocket::RpcChannel>(addr);

    NEWPRCCHANNEL("127.0.0.1:11111", channel);

    //构造请求信息
    // std::shared_ptr<makeOrderRequest> request = std::make_shared<makeOrderRequest>();
    NEWMESSAGE(makeOrderRequest,request);
    NEWMESSAGE(makeOrderResponse,response);
    request->set_price(100);
    request->set_goods("apple");
    // std::shared_ptr<makeOrderResponse> response = std::make_shared<makeOrderResponse>();
    // std::shared_ptr<rocket::RpcController> controller = std::make_shared<rocket::RpcController>();

    NEWRPCCONTROLLER(controller);
    controller->SetMsgId("99998888");
    controller->SetTimeout(10000);

    std::shared_ptr<rocket::RpcClosure> closure = std::make_shared<rocket::RpcClosure>([request, response, channel, controller]() mutable {
        if (controller->GetErrorCode() == 0)
        {
            INFOLOG("call rpc success, request [%s], response [%s]", request->ShortDebugString().c_str(), response->ShortDebugString().c_str());
            //执行业务逻辑
            if (response->order_id() == "xxd")
            {
            }
        }
        else
        {
            ERRORLOG("call rpc failed, request [%s], error code[%d], error info [%s]", request->ShortDebugString().c_str(), controller->GetErrorCode(), controller->GetErrorInfo().c_str());
        }
        INFOLOG("now exit eventloop");
        // channel->getTcpClient()->stop();
        channel.reset();
    });
    // //初始化channel
    // channel->Init(controller, request, response, closure);
    
    CALLRPC("127.0.0.1:11111", Order_Stub, makeOrder, controller, request, response, closure);
    //这里如果是调用回调函数，就是异步的，会导致回调函数的层层调用，但如果是同步的，就会阻塞
    //通过携程就可以解决这个问题，只阻塞一个携程，不会阻塞当前线程 
}

int main()
{
    rocket::Config::SetGlobalConfig(NULL);
    rocket::Logger::InitGlobalLogger(0);
    // test_tcp_client();
    test_rpc_channel();

    INFOLOG("test_rpc_channel end");

    return 0;
}