#pragma once

//连接类
class Conn {
public:
	//消息类型
	enum TYPE {
		LISTEN = 1,	//监听套接字
		CLIENT = 2,	//普通套接字
	};

	uint8_t type;	//套接字类型
	int fd;	//套接字描述符
	uint32_t serviceId;	//与套接字fd关联的Service服务id
};
