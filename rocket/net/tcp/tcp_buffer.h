#ifndef ROCKET_NET_TCP_TCP_BUFFER_H
#define ROCKET_NET_TCP_TCP_BUFFER_H

#include <vector>
#include <memory>

namespace rocket {
class TcpBuffer {
public:
    typedef std::shared_ptr<TcpBuffer> s_ptr;
    TcpBuffer(int size);
    ~TcpBuffer();
    //返回可读字节数
    int readAble();
    //返回可写的字节数
    int writeAble();
    //获取index
    int readIndex();
    int writeIndex();
    void writeToBuffer(const char * buf, int size);
    void readFromBuffer(std::vector<char> & re, int size);
    void resizeBuffer(int new_size);
    void adjustBuffer();
    //往右调整,调整已读位置
    void  moveReadIndex(int size);
    //往右调整,调整可写位置 
    void moveWriteIndex(int size);

private:
    int m_read_index {0};
    int m_write_index {0};
    int m_size {0};
public:
    std::vector<char> m_buffer; //双端队列性能更好
};

}


#endif