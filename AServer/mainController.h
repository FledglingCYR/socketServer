#ifndef __H__MAINCONTROLLER_
#define __H__MAINCONTROLLER_

#include "MyQueueStruct.h"

#include <vector>
#include <atomic>
#include <mutex>
#include <poll.h> //for pollfd
#include <memory>
#include <thread>

struct parsedData;
class msgBroadcast;
class serverListener;
class clientConnector;
class msgReceiverBase;

class mainController{
    public:
        mainController();
        ~mainController();
        void init();
        void broadcastToClient(const unsigned char* buf, const unsigned int bufLen);
        void addReceiver(msgReceiverBase* new_receiver);
    private:
        void receiveThread();
        void sendThread();
        int handleNewClientEvent(const std::vector<pollfd> &i_pollFdVec, unsigned int &o_pollFdNum);
        int acceptANewClient(const int& pollFd);
        int handleNewDataEvent(const std::vector<pollfd>& i_pollFdVec, unsigned int &i_pollFdNum, std::vector<parsedData> &o_parsedDataVec);
  
        std::atomic<char> g_stop;
        std::shared_ptr<std::thread> receiveThreadPtr;
        std::shared_ptr<std::thread> sendThreadPtr;

        MyQueueStruct<std::vector<unsigned char>> g_sendQueue;
        std::shared_ptr<serverListener> g_serverListenerPtr;
        std::shared_ptr<msgBroadcast> g_msgBroadcastPtr;
        std::vector<std::shared_ptr<clientConnector>> g_clientConnectorVec;
        std::mutex g_clientVecConnectorMutex;
};

#endif //__H__MAINCONTROLLER_