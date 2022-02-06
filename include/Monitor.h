#pragma once
#include <thread>
#include "Sunnet.h"
// #include "Atomic.h"

//監視器 監視所有worker對象 每個worker對應一個WrorkerMonitor

class Sunnet;
class Service;

struct WrorkerMonitor {
	int version;
	int check_version;
	int service_id;
};

class Monitor {
public:
	void operator()();	//线程函数	
	inline int Count() {
        return _count;
    }
	int _count; //監視數量
	std::unordered_map<uint32_t, std::shared_ptr<WrorkerMonitor>> wrorkerMonitors; //監視對象
public:
	//获取監視對象
	std::shared_ptr<WrorkerMonitor> GetWorkerMonitor(uint32_t worker_id);  
public:
	void NewWorkerMonitor(uint32_t worker_id);	
	void DeleteWorkerMonitor(uint32_t worker_id);
	void MonitorTrigger(uint32_t worker_id, int service_id);
	void MonitorCheck();
};
