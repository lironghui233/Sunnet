#pragma once
#include <thread>
#include <vector>
#include <map>
#include "Sunnet.h"

//定时器 底层用最小堆实现

class Sunnet;

typedef void (*TimerHandler) (struct TimerNode *node);

struct TimerNode {
    int idx = 0;
    int id = 0;
    int service_id = 0;
    unsigned int expire = 0;
    char* cb = NULL;
};

class Timer {
public:
    Timer() {
        _count = 0;
        _heap.clear();
        _map.clear();
    }
    inline int Count() {
        return ++_count;
    }

public:
    void Init();	//初始化
    void operator()();	//线程函数

public:
    int AddTimer(uint32_t serviceId, uint32_t expire, char* cb);
    bool DelTimer(int id);
    void ExpireTimer();
    uint32_t GetNearestTimer();

private:
    inline bool _lessThan(int lhs, int rhs) {
        return _heap[lhs]->expire < _heap[rhs]->expire;
    }
    bool _shiftDown(int pos);
    void _shiftUp(int pos);
    void _delNode(TimerNode *node);

private:
    std::vector<TimerNode*>  _heap;
    std::map<int, TimerNode*> _map;
    int _count;
};

