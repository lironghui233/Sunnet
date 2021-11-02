#pragma once
#include <thread>
#include "Sunnet.h"
//#include "Service.h"

class Sunnet;
class Service;

class Worker {
public:
	int id;	//编号
	int eachNum;	//每次处理多少条消息
	void operator()();	//线程函数	
private:	
	void CheckAndPutGlobal(std::shared_ptr<Service> srv);	
};
