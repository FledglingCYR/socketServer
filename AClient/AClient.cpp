#include "AClient.h"
#include "comdebug.h"
#include "SocketUtils.h"
#include "read_write.h"

#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>


AClient::AClient(/* args */)
{
}

AClient::~AClient()
{
    g_stop = true;
    {
        std::lock_guard<std::mutex> lck(g_receiveQueue.QueueMutex);
        g_receiveQueue.stop = true;
        g_receiveQueue.QueueCond.notify_one();//停止监听接收队列
    }
    if(g_receiveThreadPtr){
        pthread_cancel(g_receiveThreadPtr->native_handle());
        if(g_receiveThreadPtr->joinable())
            g_receiveThreadPtr->join();
        delete g_receiveThreadPtr;
        g_receiveThreadPtr = nullptr;
    }
    if(g_receiveHandleThreadPtr){
        if(g_receiveHandleThreadPtr->joinable())
            g_receiveHandleThreadPtr->join();
        delete g_receiveHandleThreadPtr;
        g_receiveHandleThreadPtr = nullptr;
    }
    close(g_clientFd);
}
void AClient::init(){
    initClient();
    return;
}

int AClient::sendDataToServer(const unsigned char *buf, const unsigned bufLen){
    return write_sure(g_clientFd, (unsigned char*)buf, (int)bufLen);
}

int AClient::initClient(){      
    g_receiveThreadPtr = new std::thread(&AClient::ReceiveThread, this);
    g_receiveHandleThreadPtr = new std::thread(&AClient::ReceiveHandleThread, this);
    return 0;
}

int AClient::connectWithServer(){
    printf("Note: start connect %s \n", TouchCoordinateSerName);
    while (!g_stop)
    {
        g_clientFd = SocketLocalClient(TouchCoordinateSerName, SOCK_STREAM);
        if(g_clientFd < 0){
            printf("Error: AClient connect server(%s) fail. \n", TouchCoordinateSerName);
            usleep(2*1000*1000);//2s后重新连接服务器
            continue;
        }
        printf("Note: success to connect %s \n", TouchCoordinateSerName);
        //g_clientFdCond.notify_one();
        break;
    }     
    return 0;
}

void AClient::ReceiveThread(){
    signal(SIGPIPE, SIG_IGN);
    int ret = -1;
    while(!g_stop){
        receivedPacket t_dataPacket;       
        ret = socketfdRecvAPacket(g_clientFd, t_dataPacket.msgBuf, sizeof(t_dataPacket.msgBuf));
        if(ret > 0){
            t_dataPacket.msgLen = ret;
            std::lock_guard<std::mutex> lck(g_receiveQueue.QueueMutex);
            printf("Debug: t_dataPacket.msgLen=%d\n",t_dataPacket.msgLen);
            g_receiveQueue.DataQueue.push(t_dataPacket);
            g_receiveQueue.QueueCond.notify_one();//通知ReceiveHandleThread从队列中取出数据来处理
        }else{
            if(g_clientFd <= 0){
                goto CONNECT_SERVER;
            }
            if(ret < 0){
                /*reconnect*/
                close(g_clientFd);
                g_clientFd = -1;
            CONNECT_SERVER:
                connectWithServer();
                continue;
            }
        }
    }
    return;
}

void AClient::ReceiveHandleThread(){
    signal(SIGPIPE, SIG_IGN);
    int ret = -1;
    while(!g_stop){
        receivedPacket t_datapacket;
        {
            std::unique_lock<std::mutex> lck(g_receiveQueue.QueueMutex);
            g_receiveQueue.QueueCond.wait(lck, [this]{return (g_receiveQueue.stop || g_receiveQueue.DataQueue.size() != 0);});
            if(g_receiveQueue.stop){
                printf("Note: g_receiveQueue.stop=%d, ReceiveThread exit\n", g_receiveQueue.stop);
                break;
            }  
            if(g_receiveQueue.DataQueue.size() == 0) continue;
            t_datapacket = g_receiveQueue.DataQueue.front();
            g_receiveQueue.DataQueue.pop();
        }
        printBuf(t_datapacket.msgBuf, t_datapacket.msgLen, "to handle buf:");
    }
    return;
}


//read all data from socket if buf is nut full;会阻塞。
int AClient::socketfdRecvAPacket(int socketfd, void* buf, unsigned buf_size){
    if(socketfd < 0){
        return -1;
    }
    fd_set allset;
    FD_ZERO(&allset);
    FD_SET(socketfd, &allset);
    if(select(socketfd+1, &allset, NULL, NULL, NULL) < 0){
        EPrintfMySocket("fail to select: %s \n", strerror(errno));
        if(errno == EINTR) return 0;
        return -1;
    }
    if(FD_ISSET(socketfd, &allset)){
        int len = read(socketfd, buf, buf_size);
        if(len < 0){
            EPrintfMySocket("fail to read:%s \n", strerror(errno));
            return -1;
        }
        return len;
    }
    return -1;
}