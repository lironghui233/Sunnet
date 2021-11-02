#pragma once
#include <vector>
#include <unordered_map>
#include "SocketWorker.h"
#include "Worker.h"
#include "Service.h"
#include "Conn.h"

class Worker;

class Sunnet {
public:
	//单例
	static Sunnet* inst;
public:
	//构造函数
	Sunnet();
	//初始化并开始
	void Start();
	//等待运行
	void Wait();


//管理网络线程
private:
	//Socket线程
	SocketWorker* socketWorker;
	std::thread* socketThread;	
private:
	//开启Socket线程
	void StartSocket();
public:	
	//修改socketfd对应的epoll_event
	void ModifyEvent(int fd, bool epollOut);


//管理工作线程	
private:
	//工作线程
	int WORKER_NUM = 3;	//工作线程数（配置）
	std::vector<Worker*> workers;	//worker对象
	std::vector<std::thread*> workerThreads;	//线程
private:
	//开启工作线程
	void StartWorker();
private:
	//休眠与唤醒
	pthread_mutex_t sleepMtx;
	pthread_cond_t sleepCond;
	int sleepCount = 0;	//正在睡眠中的工作线程数
public:
	//唤醒工作线程
	void CheckAndWeekUp();
	//让工作线程等待（仅工作线程调用）
	void WorkerWait();			


//管理service	
public:
	//服务列表
	std::unordered_map<uint32_t, std::shared_ptr<Service>> services; //unordered_map底层是哈希表，经常用于对象管理。选用哈希表是因为它的查找时间复杂度O(1)
	uint32_t maxId = 0;	//最大ID
	pthread_rwlock_t servicesLock;	//读写锁。在Sunnet系统中，会经常查找服务对象（读操作）并给它发消息，新增、删除服务的频率较低（写操作），因此使用读写锁能充分利用CPU。
public:
	//增删服务
	uint32_t NewService(std::shared_ptr<std::string> type);	//新建服务
	void KillService(uint32_t id);	//删除服务，仅限服务自己调用
private:
	//获取服务
	std::shared_ptr<Service> GetService(uint32_t id);


//管理全局队列	
private:
	//全局队列
	std::queue<std::shared_ptr<Service>> globalQueue;
	int globalLen = 0;	//队列长度
	pthread_spinlock_t globalLock;	//自旋锁，用于操作全局队列。
public:	
	//全局队列操作
	std::shared_ptr<Service> PopGlobalQueue();
	void PushGlobalQueue(std::shared_ptr<Service> srv);	


//管理Conn
public:
	//增删查Conn
	int AddConn(int fd, uint32_t id, Conn::TYPE type);
	std::shared_ptr<Conn> GetConn(int fd);
	bool RemoveConn(int fd);
private:
	//Conn列表
	std::unordered_map<uint32_t, std::shared_ptr<Conn>> conns;	// <socketfd, conn>
	pthread_rwlock_t connsLock;	//读写锁
public:
	//网络连接操作接口
	int Listen(uint32_t port, uint32_t serviceId);
	void CloseConn(uint32_t fd);	


//发送消息接口
public:
	//发送消息
	void Send(uint32_t toId, std::shared_ptr<BaseMsg> msg);

public:
	//仅测试用！！！
	//创建消息
	std::shared_ptr<BaseMsg> MakeMsg(uint32_t source, char* buff, int len);
		
};
