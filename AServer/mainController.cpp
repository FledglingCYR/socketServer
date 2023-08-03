#include "mainController.h"
#include "serverListener.h"
#include "clientConnector.h"
#include "comdebug.h"
#include "sigslotCommon.h"
#include "dataParser.h"

#include <unistd.h>//for sleep
#include <sys/socket.h>//for accept4
#include <errno.h>//for errno
#include <string.h>//for strerror
#include <signal.h>
#include <assert.h>
#include <algorithm>


using std::vector;
using std::lock_guard;
using std::mutex;

mainController::mainController()
    : g_stop(0),
    g_serverListenerPtr(new (serverListener)),
    msgBroadcastPtr(new msgBroadcast())
{


}

mainController::~mainController(){
    g_stop.store(1);
    if(receiveThreadPtr->joinable()){
        pthread_cancel(receiveThreadPtr->native_handle());//终止该线程阻塞的系统调用
        receiveThreadPtr->join();
    }
    {
        lock_guard<mutex> lck(g_sendQueue.QueueMutex);
        g_sendQueue.stop = true;
        g_sendQueue.QueueCond.notify_one();
    }
    if(sendThreadPtr->joinable()){
        pthread_cancel(sendThreadPtr->native_handle());//终止该线程阻塞的系统调用
        sendThreadPtr->join();
    }
}

void mainController::init(){
    DPrintfMySocket("mainController::init()\n");
    int ret = g_serverListenerPtr->init();
    assert(ret == 0);
    receiveThreadPtr = std::make_shared<std::thread>(&mainController::receiveThread, this);
    sendThreadPtr = std::make_shared<std::thread>(&mainController::sendThread, this);
    DPrintfMySocket("mainController::init() end\n");
}

int mainController::broadcastToClient(const char* buf, const unsigned int bufLen){
    DPrintfMySocket("mainController::broadcastToClient a %d-len buf\n", bufLen);
    std::vector<char> t_sendData(buf, buf+bufLen);
    {
        lock_guard<mutex> lck(g_sendQueue.QueueMutex);
        g_sendQueue.DataQueue.push(std::move(t_sendData));
        g_sendQueue.QueueCond.notify_one();
    }
    return 0;
}

void mainController::receiveThread(){
    DPrintfMySocket("mainController::receiveThread()\n");
    signal(SIGPIPE, SIG_IGN);
    int ret = -1;
    while(!g_stop.load()){
        std::vector<clientConnector*> t_clientConnectorVec;
        {
            lock_guard<mutex> lck(g_clientVecConnectorMutex);
            t_clientConnectorVec = g_clientConnectorVec;
        }
        vector<pollfd> t_pollFdVec;
        t_pollFdVec.reserve(1 + t_clientConnectorVec.size());
        unsigned int t_pollFdNum = 0;
        
        ret = g_serverListenerPtr->waitPollEvent(t_clientConnectorVec, t_pollFdVec, t_pollFdNum);//block until poll-even happends
        if(ret < 0){
            sleep(1);
            continue;
        }
        DPrintfMySocket("t_pollFdNum = %d \n", t_pollFdNum);
        ret = acceptNewClient(t_pollFdVec, t_pollFdNum);
        if(ret < 0){
            continue;
        }

        DPrintfMySocket("t_pollFdNum = %d \n", t_pollFdNum);
        vector<parsedData> t_parsedDataVec;
        t_parsedDataVec.reserve(t_pollFdNum);
        getParsedDataFromAllClient(t_pollFdVec, t_pollFdNum, t_parsedDataVec);

        msgBroadcastCommon::Message t_msg;

        for(auto e : t_parsedDataVec){
            memcpy(t_msg.msgBuf, &e, std::min(msgBroadcastCommon::MessageMaxLen, static_cast<int>(sizeof(parsedData)) ));
            DPrintfMySocket("msgBroadcastPtr->broadcast");
            msgBroadcastPtr->broadcast(&t_msg);
        }
    }
}
void mainController::sendThread(){
    DPrintfMySocket("mainController::sendThread()\n");
    signal(SIGPIPE, SIG_IGN);
    int ret = -1;
    while(!g_stop.load()){
        std::vector<char> t_sendData;
        {
            std::unique_lock<std::mutex> lck(g_sendQueue.QueueMutex);
            g_sendQueue.QueueCond.wait(lck, [this]{
                return !g_sendQueue.DataQueue.empty() || g_sendQueue.stop;
            });
            if(g_sendQueue.stop){
                IPrintfMySocket("g_sendQueue.stop=true, SendPacketThread exit\n");
                break;
            }
            t_sendData = g_sendQueue.DataQueue.front();
            g_sendQueue.DataQueue.pop();
        }
        //broadcast
        {
            lock_guard<mutex> lck(g_clientVecConnectorMutex);
            for(auto e : g_clientConnectorVec){
                e->SendData(t_sendData.data(), t_sendData.size());
            }
        }
    }

}
int mainController::acceptNewClient(const vector<pollfd> &i_pollFdVec, unsigned int &o_pollFdNum){
    if(i_pollFdVec.at(0).revents & (POLLIN | POLLERR)){
        if(acceptANewClient(i_pollFdVec.at(0).fd) < 0){
            return -1;
        }
        if(o_pollFdNum == 0){
            return -1;
        }
        --o_pollFdNum;
    }
    return 0;
}
int mainController::acceptANewClient(const int& pollFd){
    IPrintfMySocket("wait a new client to connect \n");
    int client_sockfd = TEMP_FAILURE_RETRY(accept4(pollFd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK));
    IPrintfMySocket("client_sockfd = %d \n", client_sockfd);
    if(client_sockfd < 0){
        EPrintfMySocket("fail to accept:%s \n", strerror(errno));
        return -1;
    }
    IPrintfMySocket("Note: a new client(%d) connected \n", client_sockfd);
    clientConnector* t_clientPtr = new clientConnector(client_sockfd);
    {
        lock_guard<mutex> lck(g_clientVecConnectorMutex);
        g_clientConnectorVec.push_back(t_clientPtr);
    }
    return 0;
}
int mainController::getParsedDataFromAllClient(const vector<pollfd>& i_pollFdVec, unsigned int &i_pollFdNum, vector<parsedData> &o_parsedDataVec){
    DPrintfMySocket("some data is comming \n");
    lock_guard<mutex> lck(g_clientVecConnectorMutex);
    for(unsigned int i = 1; i < i_pollFdVec.size() && i_pollFdNum != 0; ++i){
        short t_IVecRevents = i_pollFdVec.at(i).revents;
        DPrintfMySocket("(pollfd(%d).revents=0x%x) \n", i, t_IVecRevents);
        if(t_IVecRevents != 0){
            auto it = std::find_if(g_clientConnectorVec.begin(), g_clientConnectorVec.end(),
                                   [&i_pollFdVec, &i](clientConnector* &clientPtr){
                                        return i_pollFdVec.at(i).fd == clientPtr->Socket();
                                    });
            if(it != g_clientConnectorVec.end()){
                --i_pollFdNum;
                if(t_IVecRevents & (POLLHUP | POLLNVAL)){
                    ;
                }else if( t_IVecRevents & (POLLIN | POLLERR)){
                    if( (*it)->GetparsedData(o_parsedDataVec) > 0){
                        continue;
                    }
                }else{
                    continue;
                }

                /*到这说明：要么是POLLHUP和POLLNVAL置一, 要么是获取数据失败*/
                IPrintfMySocket("Warn: one client(%p) disconnect. \n", *it);
                IPrintfMySocket("g_clientVec.size()=%d \n", g_clientConnectorVec.size());
                delete (*it);
                g_clientConnectorVec.erase(it);
            }
        }
    }
    return 0;
}