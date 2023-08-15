#ifndef __H__SERVERLISTENER_
#define __H__SERVERLISTENER_
#include <memory>
#include <vector>
#include <poll.h>
#include <string>


class clientConnector;

class serverListener{
    public:
        serverListener(const std::string &serverName);
        ~serverListener();
        int init();
        int waitPollEvent(const std::vector<std::shared_ptr<clientConnector>> &i_clientConnectorVec, std::vector<pollfd> &o_pollFdVec, unsigned int &o_pollFdNum);
    private:
        int initServer();
        int g_serverFd = -1;
        const std::string g_serverName;
        const unsigned maxSocketClientNum = 2;
};

#endif //__H__SERVERLISTENER_