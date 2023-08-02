#include "server.h"

#include "SocketUtils.h"
#include "clientsocket.h"

#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <thread>
#include <map>
#include <memory>

SocketServer& SocketServer::getInstance(){
    static SocketServer instance;
    return instance;
}
SocketServer::SocketServer() : g_stop(false)
{

}

SocketServer::~SocketServer(){
    g_stop = true;

    if(g_clientMonitorThreadName){
        pthread_cancel(g_clientMonitorThreadName->native_handle());//终止该线程阻塞的系统调用
        g_clientMonitorThreadName->join();
        delete g_clientMonitorThreadName;
        g_clientMonitorThreadName = nullptr;
    }
    {
        std::lock_guard<std::mutex> lck(g_clientVecMutex);
        for(auto it : g_clientVec){
            if(it) delete it;
        }
    }
    if(g_serverFd > 0){
        close(g_serverFd);
        g_serverFd = -1;
    }
}

int SocketServer::init(ParseDataCallBack i_parseDataCallBack){
    ClientSocket::g_parseDataCallBack = i_parseDataCallBack;
    return initServer();
}

/*return the num of the client, which successfully send data*/
int SocketServer::BroadcastToClient(char* data, unsigned data_len, SOCKET_TYPE client_type){
    DPrintfMySocket("BroadcastToClient(data_len=%d,client_type=%d) \n", data_len, client_type);
    int successNum = 0;
    std::lock_guard<std::mutex> lck(g_clientVecMutex);
    for(auto& element : g_clientVec){
        if( element != nullptr && (element->Type() == client_type) && element->SendData(data, data_len) == 0){
            successNum++;
        }
    }
    return successNum;
}


int SocketServer::BroadcastToClient(QTDISPATCH_COM_DATA& package){
    return BroadcastToClient((char*)package.data, package.len, (SOCKET_TYPE)package.client_type);
}

void SocketServer::clientMonitorThread(){
    signal(SIGPIPE, SIG_IGN);
    while(!g_stop){
        std::vector<pollfd> t_pollFdVec;
        {
            std::lock_guard<std::mutex> lck(g_clientVecMutex);
            t_pollFdVec.reserve(1 + g_clientVec.size());
            t_pollFdVec.push_back({.fd = g_serverFd, .events = POLLIN, .revents = 0});//监听g_serverFd是否可读
            for(const auto &e : g_clientVec){
                t_pollFdVec.push_back({.fd = e->Socket(), .events = POLLIN, .revents = 0});//监听客户端描述符是否可读
            }
        }

        DPrintfMySocket("poll the fd(size=%d) \n", t_pollFdVec.size());
        int ret = TEMP_FAILURE_RETRY(poll(t_pollFdVec.data(), t_pollFdVec.size(), -1));//ret:表示有多少个文件描述符发生了事件
        if(ret < 0){
            sleep(1);
            continue;
        }

        if(t_pollFdVec.at(0).revents & (POLLIN | POLLERR)){
            if(acceptANewClient(t_pollFdVec.at(0).fd) < 0){
                sleep(1);
                continue;
            }
            if(--ret == 0) continue;
        }

        getDataFromAllClient(t_pollFdVec, ret);
    }
}

int SocketServer::initServer(){
    g_serverFd = SocketLocalServer(ServerName, SOCK_STREAM, maxSocketClientNum);
    if(g_serverFd < 0){
        EPrintfMySocket("Error: server(%s) init failed \n", ServerName);
        return -1;
    }
    /*监听线程，监听客户端的连接*/
    g_clientMonitorThreadName = new std::thread(&SocketServer::clientMonitorThread, this);
    return g_serverFd;
}

int SocketServer::acceptANewClient(const int& pollFd){
    IPrintfMySocket("wait a new client to connect \n");
    int client_sockfd = TEMP_FAILURE_RETRY(accept4(pollFd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK));
    IPrintfMySocket("client_sockfd = %d \n", client_sockfd);
    if(client_sockfd < 0){
        EPrintfMySocket("fail to accept:%s \n", strerror(errno));
        return -1;
    }
    IPrintfMySocket("Note: a new client(%d) connected \n", client_sockfd);
    ClientSocket* t_clientPtr = new ClientSocket(client_sockfd);
    t_clientPtr->init();
    {
        std::lock_guard<std::mutex> lck(g_clientVecMutex);
        g_clientVec.push_back(t_clientPtr);
    }
    return 0;
}

int SocketServer::getDataFromAllClient(const std::vector<pollfd>& t_pollFdVec, int& handledCount){
    DPrintfMySocket("some data is comming \n");
    std::lock_guard<std::mutex> lck(g_clientVecMutex);
    for(unsigned int i = 1; i < t_pollFdVec.size() && handledCount != 0; ++i){
        short t_IVecRevents = t_pollFdVec.at(i).revents;
        DPrintfMySocket("(pollfd(%d).revents=0x%x) \n", i, t_IVecRevents);
        if(t_IVecRevents != 0){
            auto it = std::find_if(g_clientVec.begin(), g_clientVec.end(),
                                   [&t_pollFdVec, &i](ClientSocket* &clientPtr){
                                        return t_pollFdVec.at(i).fd == clientPtr->Socket();
                                    });
            if(it != g_clientVec.end()){
                --handledCount;
                if(t_IVecRevents & (POLLHUP | POLLNVAL)){
                    ;
                }else if( t_IVecRevents & (POLLIN | POLLERR)){
                    if( (*it)->GetData() > 0)  continue;
                }else{
                    continue;
                }

                /*到这说明：要么是POLLHUP和POLLNVAL置一, 要么是获取数据失败*/
                IPrintfMySocket("Warn: one client(%p) disconnect. \n", *it);
                IPrintfMySocket("g_clientVec.size()=%d \n", g_clientVec.size());
                delete (*it);
                g_clientVec.erase(it);
            }
        }
    }
    return 0;
}
