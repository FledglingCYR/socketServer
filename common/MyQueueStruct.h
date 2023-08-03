#ifndef _H_MYQUEUE_
#define _H_MYQUEUE_
#include <queue>
#include <condition_variable>
#include <mutex>

template <class T>
struct MyQueueStruct
{
    std::queue<T> DataQueue;
    bool stop = false;
    std::condition_variable QueueCond;
    std::mutex QueueMutex;
};


#endif //_H_MYQUEUE_
