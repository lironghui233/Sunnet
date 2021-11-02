#pragma once
#include <list>
#include <stdint.h>
#include <memory>

//写缓冲区类
class WriteObject {
public:
	std::streamsize start; //代表已经写入套接字写缓冲区的字节数
	std::streamsize len; //代表需要发送的总字节数
	std::shared_ptr<char> buff;	
};

class ConnWriter {
public:
	int fd;
private:
	bool isClosing = false; //是否正在关闭
	std::list<std::shared_ptr<WriteObject>> objs; //双向链表，保存所有尚未发送成功的数据。
public:
	void EntireWrite(std::shared_ptr<char> buff, std::streamsize len); //尝试按序发送数据，如果未能全部发送，把未发送的数据存入objs列表。
	void LingerClose(); //延迟关闭的方法，调用该方法后，如果ConnWriter尚有待发送的数据，则ConnWriter会先把数据发送完，最后才关闭连接
	void OnWriteable(); //再次尝试发送剩余的数据	
private:
	void EntireWriteWhenEmpty(std::shared_ptr<char> buff, std::streamsize len);
	void EntireWriteWhenNotEmpty(std::shared_ptr<char> buff, std::streamsize len);	
	bool WriteFrontObj();
};
