#pragma once
#include <memory>

//消息基类
class BaseMsg {
public:
	enum TYPE {	//消息类型
		SERVICE = 1,	//Service服务间消息
		SOCKET_ACCEPT = 2,	//有新的客户端连接
		SOCKET_RW = 3,	//客户端可读可写
	};	
	uint8_t type;	//消息类型
	char load[999999]{};	//用于检测内存泄漏，仅用于调试
	virtual ~BaseMsg(){};
};

//Service服务间消息
class ServiceMsg : public BaseMsg {
public:
	uint32_t source;	//消息发送方id
	std::shared_ptr<char> buff;	//消息内容
	size_t size;		//消息内容大小	
};

//有新连接
class SocketAcceptMsg : public BaseMsg {
public:
	int listenFd;	//监听套接字描述符
	int clientFd;	//新连入客户端的套接字描述符
};

//可读可写
class SocketRWMsg : public BaseMsg {
public:
	int fd;	//发生事件的套接字描述符
	bool isRead = false;	//可读
	bool isWrite = false; 	//可写
};
