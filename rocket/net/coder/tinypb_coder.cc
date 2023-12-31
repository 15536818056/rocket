#include <vector>
#include <string.h>
#include <arpa/inet.h>
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/common/util.h"
#include "rocket/common/log.h"

namespace rocket
{
    //将message对象转换为字节流，并写入到buffer中
    void TinyPBCoder::encode(std::vector<AbstractProtocol::s_ptr> &messages, TcpBuffer::s_ptr out_buffer)
    {
        for (auto &i : messages)
        {
            std::shared_ptr<TinyPBProtocol> msg = std::dynamic_pointer_cast<TinyPBProtocol>(i);
            int len = 0;
            //将一个message对象转换为buf
            const char * buf = encodeTinyPB(msg,  len);
            if (buf != NULL && len != 0) 
            {
                out_buffer->writeToBuffer(buf, len);
            }
            if (buf)
            {
                free ((void *)buf);
                buf = NULL;
            }
        }
    }
    void TinyPBCoder::decode(std::vector<AbstractProtocol::s_ptr> &out_messages, TcpBuffer::s_ptr buffer)
    {
        while (1)
        {
            // 解码
            // 遍历buffer，找到PB_START，找到之后解析出整包的长度，得到结束符位置，校验结束符是不是0x03，如果是
            // 说明数据包合法
            std::vector<char> tmp = buffer->m_buffer;
            int start_index = buffer->readIndex();
            int end_index = -1;
            int pk_len = 0;
            bool parse_success = false; // 说明读成功了
            int i = 0;
            for (i = start_index; i < buffer->writeIndex(); i++)
            {
                if (tmp[i] == TinyPBProtocol::PB_START)
                { // 读下去四个字节，由于是网络字节序，需要转换为主机字节序
                    if (i + 1 < buffer->writeIndex())
                    { // pk_len
                        pk_len = getInt32FromNetByte(&tmp[i + 1]);
                        DEBUGLOG("get pk_len = %d", pk_len);
                        // 结束符的索引
                        int j = i + pk_len - 1;
                        if (j >= buffer->writeIndex())
                        {
                            continue; // 说明没有读到整个包
                        }
                        if (tmp[j] == TinyPBProtocol::PB_END)
                        {
                            // 下次遍历从这里开始
                            start_index = i;
                            end_index = j;
                            parse_success = true;
                            break;
                        }
                    }
                }
            }
            if (i >= buffer->writeIndex())
            {
                DEBUGLOG("decode end, read all buffer data");
                return;
            }
            if (parse_success)
            {
                buffer->moveReadIndex(end_index - start_index + 1); // 移动成功读取的字节数个字节
                std::shared_ptr<TinyPBProtocol> message = std::make_shared<TinyPBProtocol>();
                message->m_pk_len = pk_len;

                int req_id_len_index = start_index + sizeof(char) + sizeof(message->m_pk_len);
                if (req_id_len_index >= end_index)
                {
                    // 说明包有问题
                    message->parse_success = false;
                    ERRORLOG("parse error, req_id_len_index[%d] > end_index[%d]", req_id_len_index, end_index);
                    continue;
                }
                // 包没问题，继续往下读
                message->m_msg_id_len = getInt32FromNetByte(&tmp[req_id_len_index]);
                DEBUGLOG("parse req_id_len = %d", message->m_msg_id_len);

                int req_id_index = req_id_len_index + sizeof(message->m_msg_id_len);
                char req_id[100] = {0};
                memcpy(&req_id[0], &tmp[req_id_index], message->m_msg_id_len);
                message->m_msg_id = std::string(req_id);
                DEBUGLOG("parse req_id = %s", message->m_msg_id.c_str());

                int method_name_len_index = req_id_index + message->m_msg_id_len;
                if (method_name_len_index >= end_index)
                {
                    message->parse_success = false;
                    ERRORLOG("parse error, method_name_len_index[%d] > end_index[%d]", req_id_len_index, end_index);
                    continue;
                }
                message->m_method_name_len = getInt32FromNetByte(&tmp[method_name_len_index]);

                int method_name_index = method_name_len_index + sizeof(message->m_method_name_len);
                char method_name[512] = {0};
                memcpy(&method_name[0], &tmp[method_name_index], message->m_method_name_len);
                message->m_method_name = std::string(method_name);
                DEBUGLOG("parse method_name = %s", message->m_method_name.c_str());

                int err_code_index = method_name_index + message->m_method_name_len;
                if (err_code_index >= end_index)
                {
                    message->parse_success = false;
                    ERRORLOG("parse error, err_code_index[%d] > end_index[%d]", req_id_len_index, end_index);
                    continue;
                }
                message->m_err_code = getInt32FromNetByte(&tmp[err_code_index]);

                int error_info_len_index = err_code_index + sizeof(message->m_err_code);
                if (error_info_len_index >= end_index)
                {
                    message->parse_success = false;
                    ERRORLOG("parse error, error_info_len_index[%d] > end_index[%d]", req_id_len_index, end_index);
                    continue;
                }
                message->m_err_info_len = getInt32FromNetByte(&tmp[error_info_len_index]);

                int error_info_index = error_info_len_index + sizeof(message->m_err_info_len);
                char error_info[512] = {0};
                memcpy(&error_info[0], &tmp[error_info_index], message->m_err_info_len);
                message->m_err_info = std::string(error_info);
                DEBUGLOG("parse error_info = %s", message->m_err_info.c_str());

                int pb_data_len = message->m_pk_len - message->m_method_name_len - message->m_msg_id_len - message->m_err_info_len - 2 - 24;
                int pb_data_index = error_info_index + message->m_err_info_len;
                message->m_pb_data = std::string(&tmp[pb_data_index], pb_data_len);
                // 这里校验和去解析
                message->parse_success = true;
                out_messages.push_back(message);
            }
        }
    }
    const char *TinyPBCoder::encodeTinyPB(std::shared_ptr<TinyPBProtocol> message, int &len)
    {
        if (message->m_msg_id.empty())
        {
            message->m_msg_id = "123456789";
        }
        DEBUGLOG("req_id = %s", message->m_msg_id.c_str());
        int pk_len = 2 + 24 + message->m_msg_id.length() + message->m_method_name.length() + message->m_err_info.length() + message->m_pb_data.length();
        DEBUGLOG("pk_len = %d", pk_len);

        char *buf = reinterpret_cast<char *>(malloc(pk_len));
        char *tmp = buf;
        *tmp = TinyPBProtocol::PB_START;
        tmp++;

        int32_t pk_len_net = htonl(pk_len);
        memcpy(tmp, &pk_len_net, sizeof(pk_len_net));
        tmp += sizeof(pk_len_net);

        int req_id_len = message->m_msg_id.length();
        int32_t req_id_len_net = htonl(req_id_len);
        memcpy(tmp, &req_id_len_net, sizeof(req_id_len_net));
        tmp += sizeof(req_id_len_net);

        if (!message->m_msg_id.empty())
        {
            memcpy(tmp, &(message->m_msg_id[0]), req_id_len);
            tmp += req_id_len;
        }

        int method_name_len = message->m_method_name.length();
        int32_t method_name_len_net = htonl(method_name_len);
        memcpy(tmp, &method_name_len_net, sizeof(method_name_len_net));
        tmp += sizeof(method_name_len_net);

        if (!message->m_method_name.empty())
        {
            memcpy(tmp, &(message->m_method_name[0]), method_name_len);
            tmp += method_name_len;
        }

        int32_t err_code_net = htonl(message->m_err_code);
        memcpy(tmp, &err_code_net, sizeof(err_code_net));
        tmp += sizeof(err_code_net);
        
        int err_info_len = message->m_err_info.length();
        int32_t err_info_len_net = htonl(err_info_len);
        memcpy(tmp, &err_info_len_net, sizeof(err_info_len_net));
        tmp += sizeof(err_info_len_net);

        if (!message->m_err_info.empty())
        {
            memcpy(tmp, &(message->m_err_info[0]), err_info_len);
            tmp += err_info_len;
        }

        if (!message->m_pb_data.empty())
        {
            memcpy(tmp, &(message->m_pb_data[0]), message->m_pb_data.length());
            tmp += message->m_pb_data.length();
        }

        int32_t check_sum_net = htonl(1);
        memcpy(tmp, &check_sum_net, sizeof(check_sum_net));
        tmp += sizeof(check_sum_net);

        *tmp = TinyPBProtocol::PB_END;
        
        message->m_pk_len = pk_len;
        message->m_msg_id_len = req_id_len;
        message->m_method_name_len = method_name_len;
        message->m_err_info_len = err_info_len;
        message->parse_success = true;
        len = pk_len;

        DEBUGLOG("encode message[%s]", message->m_msg_id.c_str());
        return buf;

    }

}
