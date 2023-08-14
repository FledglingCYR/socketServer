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
    g_msgBroadcastPtr(new msgBroadcast())
{


}

mainController::~mainController(){
    DPrintfMySocket("mainController::~mainController()\n");
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
    return;
}

void mainController::broadcastToClient(const unsigned char* buf, const unsigned int bufLen){
    DPrintfMySocket("mainController::broadcastToClient a %d-len buf \n", bufLen);
    vector<unsigned char> t_sendData(buf, buf+bufLen);
    {
        lock_guard<mutex> lck(g_sendQueue.QueueMutex);
        g_sendQueue.DataQueue.push(std::move(t_sendData));
        g_sendQueue.QueueCond.notify_one();
    }
    return;
}

void mainController::addReceiver(msgReceiverBase* new_receiver){
    g_msgBroadcastPtr->addReceiver(new_receiver);
    return; 
}


void mainController::receiveThread(){
    DPrintfMySocket("mainController::receiveThread()\n");
    signal(SIGPIPE, SIG_IGN);
    int ret = -1;
    while(!g_stop.load()){
        std::vector<std::shared_ptr<clientConnector>> t_clientConnectorVec;
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

        ret = handleNewClientEvent(t_pollFdVec, t_pollFdNum);
        if(ret < 0){
            continue;
        }
        DPrintfMySocket("t_pollFdNum = %d \n", t_pollFdNum);

        vector<parsedData> t_parsedDataVec;
        t_parsedDataVec.reserve(t_pollFdNum);
        handleNewDataEvent(t_pollFdVec, t_pollFdNum, t_parsedDataVec);

        msgBroadcastCommon::Message t_msg;
        for(auto e : t_parsedDataVec){
            t_msg.msgLen = std::min(msgBroadcastCommon::MessageMaxLen, e.DataLen);
            t_msg.msgCmd = e.Cmd;
            memcpy(t_msg.msgBuf, e.Buf, t_msg.msgLen);
            DPrintfMySocket("g_msgBroadcastPtr->broadcast");
            g_msgBroadcastPtr->broadcast(&t_msg);
        }
    }
    return;
}
void mainController::sendThread(){
    DPrintfMySocket("mainController::sendThread()\n");
    signal(SIGPIPE, SIG_IGN);
    int ret = -1;
    while(!g_stop.load()){
        vector<unsigned char> t_sendData;
        {
            std::unique_lock<mutex> lck(g_sendQueue.QueueMutex);
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
    return;
}
int mainController::handleNewClientEvent(const vector<pollfd> &i_pollFdVec, unsigned int &o_pollFdNum){
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
    auto t_clientPtr = std::make_shared<clientConnector>(client_sockfd);
    IPrintfMySocket("Note: a new client(fd=%d,ptr=%p) connected \n", client_sockfd, t_clientPtr.get());
    {
        lock_guard<mutex> lck(g_clientVecConnectorMutex);
        g_clientConnectorVec.push_back(t_clientPtr);
    }
    return 0;
}
int mainController::handleNewDataEvent(const vector<pollfd>& i_pollFdVec, unsigned int &i_pollFdNum, vector<parsedData> &o_parsedDataVec){
    DPrintfMySocket("some data is comming \n");
    lock_guard<mutex> lck(g_clientVecConnectorMutex);
    for(auto iter = i_pollFdVec.cbegin(); iter != i_pollFdVec.cend() && i_pollFdNum != 0; ++iter){
        short t_IVecRevents = (*iter).revents;
        DPrintfMySocket("(pollfd.revents=0x%x) \n", t_IVecRevents);
        if(t_IVecRevents != 0){
            int t_fd = (*iter).fd;
            auto it = std::find_if(g_clientConnectorVec.cbegin(), g_clientConnectorVec.cend(),
                [&t_fd](const std::shared_ptr<clientConnector> &clientPtr){return t_fd == clientPtr->Socket();});
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
                IPrintfMySocket("Warn: one client(%p) disconnect. \n", (*it).get());
                IPrintfMySocket("g_clientVec.size()=%zu \n", g_clientConnectorVec.size());
                g_clientConnectorVec.erase(it);
            }
        }
    }
    return 0;
}