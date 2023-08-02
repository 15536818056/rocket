#include "rocket/net/coder/tinypb_coder.h"

namespace rocket 
{
char TinyPBProtocol::PB_START = 0x02;   //注意不能写在头文件中，会重复包含
char TinyPBProtocol::PB_END = 0x03;
}