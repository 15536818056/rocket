#ifndef ROCKET_NET_RPC_RPC_DISPATCHER_H
#define ROCKET_NET_RPC_RPC_DISPATCHER_H

#include <map>
#include <memory>
#include <google/protobuf/service.h>
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/coder/tinypb_protocol.h"

namespace rocket 
{
class TcpConnection;
class RpcDispatcher
{
public:
    static RpcDispatcher * GetRpcDispatcher();
public:
    typedef std::shared_ptr<google::protobuf::Service> service_s_ptr;
    //根据请求的message对象，调用rpc方法，最终得到响应的message对象
    void dispatcher(AbstractProtocol::s_ptr request, AbstractProtocol::s_ptr response, TcpConnection * connection);
    
    void registerService(service_s_ptr service);
    
    void setTinyPBError(std::shared_ptr<TinyPBProtocol> msg, int32_t err_code, const std::string err_info);

private:
    bool parseServiceFullName(const std::string & full_name, std::string & service_name, std::string & method_name);

private:
    //服务对象,string 是服务名称, value是service对象
    std::map<std::string, service_s_ptr> m_service_map; 




};


}

#endif