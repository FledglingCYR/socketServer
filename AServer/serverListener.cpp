#include "serverListener.h"
#include "clientConnector.h"
#include "comdebug.h"
#include "SocketUtils.h"

#include <sys/socket.h>//for SOCK_STREAM
#include <unistd.h>//for TEMP_FAILURE_RETRY
#include <errno.h>
#include <string.h>//for strerror

serverListener::~serverListener(){
    close(g_serverFd);
}

int serverListener::init(){
    return initServer();
}

int serverListener::waitPollEvent(const std::vector<std::shared_ptr<clientConnector>> &i_clientConnectorVec, std::vector<pollfd> &o_pollFdVec, unsigned int &o_pollFdNum){
    DPrintfMySocket("serverListener::waitPollEvent \n");
    o_pollFdVec.push_back({.fd = g_serverFd, .events = POLLIN, .revents = 0});//监听g_serverFd是否可读
    for(const auto &e : i_clientConnectorVec){
        o_pollFdVec.push_back({.fd = e->Socket(), .events = POLLIN, .revents = 0});//监听客户端描述符是否可读
    }

    DPrintfMySocket("poll the fd(size=%zu) \n", o_pollFdVec.size());
    int ret = TEMP_FAILURE_RETRY(poll(o_pollFdVec.data(), o_pollFdVec.size(), -1));//ret:表示有多少个文件描述符发生了事件
    if(ret < 0){
        IPrintfMySocket("poll failed, errno: %s", strerror(errno));
        return ret;
    }
    o_pollFdNum = static_cast<unsigned int>(ret);
    return 0;
}

int serverListener::initServer(){
    g_serverFd = SocketLocalServer(ServerName, SOCK_STREAM, maxSocketClientNum);
    if(g_serverFd < 0){
        EPrintfMySocket("Error: server(%s) init failed \n", ServerName);
        return -1;
    }
    return 0;
}