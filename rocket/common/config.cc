#include <tinyxml/tinyxml.h>
#include "rocket/common/config.h"


/*
在宏定义中，加上反斜杠 \ 表示续行符号，用于将一行代码拆分成多行书写。
当宏的定义内容很长时，为了提高可读性和维护性，我们可以使用反斜杠 \ 将一行代码分解为多行书写。这样做可以让宏定义更加清晰，并且方便注释或修改其中的某些部分

在C和C++中，宏语句是一种用预处理器进行文本替换的机制。#符号是宏定义中的一个特殊字符，用于将宏参数转换为字符串常量。
当在宏定义中使用#时，它被称为字符串化操作符（stringizing operator）。它的作用是将宏参数转换为字符串常量。例如，考虑以下宏定义：
#define PRINT_VAR(x)  printf("The value of " #x " is %d\n", x)
在这个例子中，#x将会把宏参数x转换为一个字符串。当你调用PRINT_VAR(a)时，它会被扩展成printf("The value of " "a" " is %d\n", a)，其中"a"是字符串常量。
字符串化操作符可用于创建包含变量名或表达式的自定义格式化输出。它可以提高代码的灵活性和可读性，因为它允许在宏定义中动态地生成字符串常量。
需要注意的是，#仅在宏定义中使用，不能在普通的代码中使用。
*/
#define READ_XML_NODE(name, parent) \
    TiXmlElement * name##_node = parent->FirstChildElement(#name); \
    if (!name##_node) { \
        printf("Start rocket server error, failed  to read node [%s]\n", #name); \
        exit(0); \
    } \

#define READ_STR_FROM_XML_NODE(name, parent) \
    TiXmlElement * name##_node = parent->FirstChildElement(#name); \
    if (!name##_node || !name##_node->GetText()) {\
        printf("Start rocket server error, failed to read config file %s\n", #name); \
        exit(0); \
    } \
    std::string name##_str = std::string(name##_node->GetText()); \

namespace rocket
{
    Config * g_config = NULL;
    Config * Config::GetGlobalConfig()
    {
        return g_config;
    }

    void Config::SetGlobalConfig(const char * xmlfile)
    {
        if (g_config ==  NULL)
        {
            g_config = new Config(xmlfile);
        }
    }

    Config::Config(const char * xmlfile)
    {
        TiXmlDocument * xml_document = new TiXmlDocument();
        bool rt = xml_document->LoadFile(xmlfile);
        if (!rt)
        {
            printf("Start rocket server error, failed to read config file %s, error info [%s] \n", xmlfile, xml_document->ErrorDesc());
            exit(0);
        }
        READ_XML_NODE(root, xml_document);
        READ_XML_NODE(log, root_node);
        READ_STR_FROM_XML_NODE(log_level, log_node);
        m_log_level = log_level_str;
    }



}