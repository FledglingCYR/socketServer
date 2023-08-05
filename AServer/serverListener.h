#ifndef __H__SERVERLISTENER_
#define __H__SERVERLISTENER_
#include <memory>
#include <vector>
#include <poll.h>


class clientConnector;

class serverListener{
    public:
        serverListener() = default;
        ~serverListener();
        int init();
        int waitPollEvent(const std::vector<std::shared_ptr<clientConnector>> &i_clientConnectorVec, std::vector<pollfd> &o_pollFdVec, unsigned int &o_pollFdNum);
    private:
        int initServer();
        int g_serverFd = -1;
        const char ServerName[27] = "/tmp/.qtDPH_srv";
        const unsigned maxSocketClientNum = 2;
};

#endif //__H__SERVERLISTENER_