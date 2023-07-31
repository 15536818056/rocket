#ifndef ROCKET_NET_ABSTRACT_PROTOCOL_H
#define ROCKET_NET_ABSTRACT_PROTOCOL_H

#include <memory>

//抽象的协议基类
namespace rocket {

class AbstractProtocol {
public:
    //为了方便使用，定义一个智能指针`
    typedef std::shared_ptr<AbstractProtocol> s_ptr;




};



}



#endif