#ifndef _H_TOUCHCOORDINATE_
#define _H_TOUCHCOORDINATE_

#include <thread>
#include <atomic>
#include <vector>
#include <string.h>

#include "../common/MyQueueStruct.h"

#define SEND_PACKET_MAX_LEN (1280)
struct receivedPacket{
    unsigned char msgBuf[SEND_PACKET_MAX_LEN] = {0};
    int msgLen = 0;
};

class AClient
{
public:
    AClient();
    ~AClient();

    void init();
    int sendDataToServer(const unsigned char *buf, const unsigned bufLen);
    
private:
    int g_clientFd = -1;

    const char TouchCoordinateSerName[27] =  "/tmp/.qtDPH_srv";

    bool g_stop = false;
    MyQueueStruct<receivedPacket> g_receiveQueue;
    std::thread* g_receiveThreadPtr = nullptr;
    std::thread* g_receiveHandleThreadPtr = nullptr;

    void ReceiveThread();
    void ReceiveHandleThread();
    int initClient();
    int connectWithServer();
    int socketfdRecvAPacket(int socketfd, void* buf, unsigned buf_size);
    
};



#endif