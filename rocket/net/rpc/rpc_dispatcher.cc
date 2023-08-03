#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/common/log.h"
#include "rocket/common/error_code.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_connection.h"

/*
RPC服务端流程
启动的时候,注册OrderService 对象
RPC调用流程
1.从buffer读取数据，decode得到请求的TinyPBProtocol对象，请求的TinyPBProtocol 得到method_name, \
从OrderService对象里根据service.method_name中找到rpc方法func
2.根据RPC方法 找到对应的request type以及response type
3.将请求体里面的pb_date 反序列化为request type的一个对象，声明一个空的response type对象
4.func(request, response)
5.将response对象序列化为pb_date字节流。再塞入到TinyPBProtocol结构体中。做encode转换为二进制字节流,\
然后塞入到buffer里面，注册可写事件监听，就会发送回包了
*/
namespace rocket
{
    static RpcDispatcher *g_rpc_dispatcher = NULL;
    RpcDispatcher *RpcDispatcher::GetRpcDispatcher()
    {
        if (g_rpc_dispatcher != NULL)
        {
            return g_rpc_dispatcher;
        }
        g_rpc_dispatcher = new RpcDispatcher;
        return g_rpc_dispatcher;
    }

    void RpcDispatcher::dispatcher(AbstractProtocol::s_ptr request, AbstractProtocol::s_ptr response, TcpConnection *connection)
    {
        // 拿到协议的对象
        std::shared_ptr<TinyPBProtocol> req_protocol = std::dynamic_pointer_cast<TinyPBProtocol>(request);
        std::shared_ptr<TinyPBProtocol> rsp_protocol = std::dynamic_pointer_cast<TinyPBProtocol>(response);
        std::string method_full_name = req_protocol->m_method_name;
        std::string service_name; // 可以找到service对象
        std::string method_name;

        rsp_protocol->m_req_id = req_protocol->m_req_id;
        rsp_protocol->m_method_name = req_protocol->m_method_name;
        if (!parseServiceFullName(method_full_name, service_name, method_name))
        {
            setTinyPBError(rsp_protocol, ERROR_PARSE_SERVICE_NAME, "parse service name error");
            return;
        }
        auto it = m_service_map.find(service_name);
        if (it == m_service_map.end())
        {
            ERRORLOG("%s | service name [%s] not found", req_protocol->m_req_id.c_str(), service_name.c_str());
            setTinyPBError(rsp_protocol, ERROR_SERVICE_NOT_FOUND, "service not found");
            return;
        }
        // 找到service对象
        service_s_ptr service = (*it).second; // 通过service指针可以找到方法、request response
        // 找到method对象
        const google::protobuf::MethodDescriptor *method = service->GetDescriptor()->FindMethodByName(method_name);
        if (method == NULL)
        {
            ERRORLOG("method name [%s] not found in service [%s]", method_name.c_str(), service_name.c_str());
            setTinyPBError(rsp_protocol, ERROR_SERVICE_NOT_FOUND, "method not found");
            return;
        }
        // 反序列化，将pb_data反序列化为rst_msg
        google::protobuf::Message *req_msg = service->GetRequestPrototype(method).New();
        if (!req_msg->ParseFromString(req_protocol->m_pb_data))
        {
            ERRORLOG("%s | deserilize error", req_protocol->m_req_id.c_str());
            setTinyPBError(rsp_protocol, ERROR_FAILED_DESERIALIZE, "deserilize error");
            if (req_msg != NULL)
            {
                delete req_msg;
                req_msg = NULL;
            }
            return;
        }
        INFOLOG("%s | req_id[%s], get rpc request [%s]", req_protocol->m_req_id.c_str(), req_protocol->m_req_id.c_str(), req_msg->ShortDebugString().c_str());

        google::protobuf::Message *rsp_msg = service->GetResponsePrototype(method).New();

        // 通过controller对象可以获取本次调用的一些信息
        RpcController rpcController;
        rpcController.SetLocalAddr(connection->getLocalAddr());
        rpcController.SetPeerAddr(connection->getPeerAddr());
        rpcController.SetReqId(req_protocol->m_req_id);

        service->CallMethod(method, &rpcController, req_msg, rsp_msg, NULL);

        // 将rsp_msg对象序列化成字节流,然后返回到m_pd_data中
        if (!rsp_msg->SerializeToString(&(rsp_protocol->m_pb_data)))
        {
            ERRORLOG("%s | serilize error, origin message [%s]", req_protocol->m_req_id.c_str(), rsp_msg->ShortDebugString().c_str());
            setTinyPBError(rsp_protocol, ERROR_FAILED_SERIALIZE, "serilize error");
            return;
            if (req_msg != NULL)
            {
                delete req_msg;
                req_msg = NULL;
            }
            if (rsp_msg != NULL)
            {
                delete rsp_msg;
                rsp_msg = NULL;
            }
        }

        rsp_protocol->m_err_code = 0;
        INFOLOG("%s | dispatch success, request[%s], response[%s]", req_protocol->m_req_id.c_str(), req_msg->ShortDebugString().c_str(), rsp_msg->ShortDebugString().c_str());
        delete req_msg;
        delete rsp_msg;
        req_msg = NULL;
        rsp_msg = NULL;
    }

    bool RpcDispatcher::parseServiceFullName(const std::string &full_name, std::string &service_name, std::string &method_name)
    {
        if (full_name.empty())
        {
            ERRORLOG("full name empty");
            return false;
        }
        size_t i = full_name.find_first_of(".");
        if (i == full_name.npos)
        {
            ERRORLOG("not find . in full name [%s]", full_name.c_str());
            return false;
        }
        service_name = full_name.substr(0, i);
        method_name = full_name.substr(i + 1, full_name.length() - i - 1);
        INFOLOG("parse service_name[%s] and method_name[%s] from full name [%s]", service_name.c_str(), method_name.c_str(), full_name.c_str());
        return true;
    }

    void RpcDispatcher::registerService(service_s_ptr service)
    {
        std::string service_name = service->GetDescriptor()->full_name();
        m_service_map[service_name] = service;
    }

    void RpcDispatcher::setTinyPBError(std::shared_ptr<TinyPBProtocol> msg, int32_t err_code, const std::string err_info)
    {
        msg->m_err_code = err_code;
        msg->m_err_info = err_info;
        msg->m_err_info_len = err_info.length();
    }

}