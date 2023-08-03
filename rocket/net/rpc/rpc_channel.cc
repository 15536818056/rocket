#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/common/msg_id_util.h"
#include "rocket/common/log.h"
#include "rocket/common/error_code.h" G

namespace rocket
{
    RpcChannel::RpcChannel(NetAddr::s_ptr peer_addr)
    {
    }
    RpcChannel::~RpcChannel()
    {
    }

    void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                                google::protobuf::Message *response, google::protobuf::Closure *done)
    {
        // 构造请求的协议对象
        std::shared_ptr<rocket::TinyPBProtocol> req_protocol = std::make_shared<rocket::TinyPBProtocol>();
        RpcController *my_controller = dynamic_cast<RpcController *>(controller);
        if (my_controller == NULL)
        {
            ERRORLOG("failed callmethod, RpcContorller convert error");
            return;
        }
        // 1.设置请求对象的msgid
        if (my_controller->GetMsgId().empty())
        {
            req_protocol->m_msg_id = MsgIDUtil::GenMsgID();
            my_controller->SetMsgId(req_protocol->m_msg_id);
        }
        else
        {
            req_protocol->m_msg_id = my_controller->GetMsgId();
        }
        // 2.设置方法名
        req_protocol->m_method_name = method->full_name();
        INFOLOG("%s | call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());

        // 3.请求request pb_data的序列化
        if (!request->SerializeToString(&(req_protocol->m_pb_data)))
        {
            std::string err_info = "failed to serialize";
            my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
            ERRORLOG("%s | %s, origin reqeust [%s]", req_protocol->m_msg_id.c_str(), err_info.c_str(), request->ShortDebugString().c_str());
            return;
        }

        // 4.连接
        TcpClient client(m_peer_addr);
        client.connect([&client, req_protocol, done]() { // 连接成功后调用回调函数
            // 将请求的协议对象发送给对方
            client.writeMessage(req_protocol, [&client, req_protocol, done](AbstractProtocol::s_ptr)
                                {
                INFOLOG("%s | send rpc request success. call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());
                //协程结合到这里就不用这么多回调函数了
                //发送成功后读回包
                client.readMessage(req_protocol->m_msg_id, [done](AbstractProtocol::s_ptr msg){
                    std::shared_ptr<rocket::TinyPBProtocol> rsp_protocol = std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg);
                    //打印汇报数据
                    INFOLOG("%s | succcess get rpc response, method name [%s]", rsp_protocol->m_msg_id.c_str(), rsp_protocol->m_method_name.c_str());
                    //执行客户端传入的回调函数
                    if (done)
                    {
                        done->Run();  
                    }
                }); 
            });
        });
    }
}