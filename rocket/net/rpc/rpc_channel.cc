#include <memory>
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
    RpcChannel::RpcChannel(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr)
    {
        m_client = std::make_shared<TcpClient>(m_peer_addr);
    }
    RpcChannel::~RpcChannel()
    {
        INFOLOG("~RpcChannel");
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
            // 设置到请求中
            my_controller->SetMsgId(req_protocol->m_msg_id);
        }
        else
        {
            req_protocol->m_msg_id = my_controller->GetMsgId();
        }
        // 2.设置方法名
        req_protocol->m_method_name = method->full_name();
        INFOLOG("%s | call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());

        // 确保调用rpc前初始化，设置好智能指针
        if (!m_is_init)
        {
            std::string err_info = "RpcChannel not init";
            my_controller->SetError(ERROR_RPC_CHANNEL_INIT, err_info);
            ERRORLOG("%s | %s, RpcChannel not init", req_protocol->m_msg_id.c_str(), err_info.c_str());
            return;
        }

        // 3.请求request pb_data的序列化
        if (!request->SerializeToString(&(req_protocol->m_pb_data)))
        {
            std::string err_info = "failed to serialize";
            my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
            ERRORLOG("%s | %s, origin reqeust [%s]", req_protocol->m_msg_id.c_str(), err_info.c_str(), request->ShortDebugString().c_str());
            return;
        }

        // 构造对象的时候一定要用智能指针去构造，不要用栈或者new，不然会会造成野指针的问题
        s_ptr channel = shared_from_this(); // 将channel转换为智能指针对象

        // 4.连接
        m_client->connect([req_protocol, channel]() mutable { // 连接成功后调用回调函数
            // 将请求的协议对象发送给对方
            channel->getTcpClient()->writeMessage(req_protocol, [req_protocol, channel](AbstractProtocol::s_ptr) mutable
                                {
                INFOLOG("%s | send rpc request success. call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());
                //协程结合到这里就不用这么多回调函数了
                //发送成功后读回包
                channel->getTcpClient()->readMessage(req_protocol->m_msg_id, [channel](AbstractProtocol::s_ptr msg) mutable {
                    //成功获取回包
                    std::shared_ptr<rocket::TinyPBProtocol> rsp_protocol = std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg);
                    //打印回包数据
                    INFOLOG("%s | succcess get rpc response, method name [%s]", rsp_protocol->m_msg_id.c_str(), rsp_protocol->m_method_name.c_str());

                    RpcController * my_controller = dynamic_cast<RpcController *>(channel->getController());
                    //序列化
                    if (!(channel->getResponse()->ParseFromString(rsp_protocol->m_pb_data)))
                    {
                        ERRORLOG("deserilize error"); 
                        //在controller中设置信息后才能在外能拿到rpc调用结果
                        my_controller->SetError(ERROR_FAILED_SERIALIZE, "serialize error");
                        return;
                    }
                    if (rsp_protocol->m_err_code != 0)
                    {
                        ERRORLOG("%s | call rpc method[%s] failed, error code [%d], error info[%s]", rsp_protocol->m_msg_id.c_str(), rsp_protocol->m_method_name.c_str(), rsp_protocol->m_err_code, rsp_protocol->m_err_info.c_str());
                        my_controller->SetError(rsp_protocol->m_err_code, rsp_protocol->m_err_info);
                        return;
                    }

                    //执行客户端传入的回调函数
                    if (channel->getClosure())  //获取回调函数
                    {
                        channel->getClosure()->Run();  //如果有回调函数就执行
                    }
                    channel.reset();    //将channel智能指针引用-1，如果析构的话，成员指针也会-1 
                }); });
        });
    }

    // 保存对象的智能指针,防止回调的时候对象析构
    void RpcChannel::Init(controller_s_ptr controller, message_s_ptr req, message_s_ptr res, closure_s_ptr done)
    {
        if (m_is_init)
        {
            return;
        }
        m_controller = controller;
        m_request = req;
        m_response = res;
        m_closure = done;
        m_is_init = true;
    }

    google::protobuf::RpcController *RpcChannel::getController()
    {
        return m_controller.get(); // 返回裸指针
    }
    google::protobuf::Message *RpcChannel::getRequest()
    {
        return m_request.get();
    }
    google::protobuf::Message *RpcChannel::getResponse()
    {
        return m_response.get();
    }
    google::protobuf::Closure *RpcChannel::getClosure()
    {
        return m_closure.get();
    }


    TcpClient * RpcChannel::getTcpClient() 
    {
        return m_client.get();
    }
}