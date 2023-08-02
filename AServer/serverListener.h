#ifndef __H__SERVERLISTENER_
#define __H__SERVERLISTENER_
// #include <memory>
// #include <thread>
#include <vector>
#include <poll.h>


class clientConnector;

class serverListener{
    public:
        serverListener() = default;
        ~serverListener() = default;
        int init();
        int waitPollEvent(const std::vector<clientConnector*> &i_clientConnectorVec, std::vector<pollfd> &o_pollFdVec, unsigned int &o_pollFdNum);
    private:
        int initServer();
        // void listenThread();

        // std::shared_ptr<std::thread> listenThreadPtr;
        int g_serverFd = -1;
        const char ServerName[27] = "./.testServer";
        const unsigned maxSocketClientNum = 2;
};

#endif //__H__SERVERLISTENER_