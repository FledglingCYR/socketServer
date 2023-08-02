#ifndef __H__MAINCONTROLLER_
#define __H__MAINCONTROLLER_

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

class mainController{
    public:
        mainController();
        ~mainController();
        void init();
    private:
        void receiveThread();
        void sendThread();
        int acceptNewClient(const std::vector<pollfd> &i_pollFdVec, unsigned int &o_pollFdNum);
        int acceptANewClient(const int& pollFd);
        int getParsedDataFromAllClient(const std::vector<pollfd>& i_pollFdVec, unsigned int &i_pollFdNum, std::vector<parsedData> &o_parsedDataVec);
  
        std::atomic<char> g_stop;
        std::shared_ptr<std::thread> receiveThreadPtr;
        std::shared_ptr<std::thread> sendThreadPtr;

        std::shared_ptr<serverListener> g_serverListenerPtr;
        std::shared_ptr<msgBroadcast> msgBroadcastPtr;
        std::vector<clientConnector*>  g_clientConnectorVec;
        std::mutex g_clientVecConnectorMutex;
};

#endif //__H__MAINCONTROLLER_