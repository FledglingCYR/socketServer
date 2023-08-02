#include "clientsocket.h"

#include "read_write.h"
#include "dataparsed.h"
#include "comdebug.h"

#include <unistd.h>
#include <stdio.h>
#include <thread>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <map>
#include <memory>
#include <algorithm>

MyQueueStruct<ReceivePacket> ClientSocket::g_receiveQueue;
ClientSocket::ParseDataCallBack ClientSocket::g_parseDataCallBack = nullptr;

ClientSocket::ClientSocket(int socket) :
    g_socket(socket), g_stop(false), g_inited(false), g_dataParserptr(new DataParser())
{
    IPrintfMySocket("g_socket init to %d \n", g_socket);
    Receivebuf.reserve(SEND_PACKET_MAX_LEN);
}

ClientSocket::~ClientSocket(){
    g_stop = true;
    {
        std::lock_guard<std::mutex> lck(g_sendQueue.QueueMutex);
        g_sendQueue.stop = true;
        g_sendQueue.QueueCond.notify_one();
    }
    if(SendPacketThreadName){
        SendPacketThreadName->join();
        delete SendPacketThreadName;
        SendPacketThreadName = nullptr;
    }
    if(g_socket >= 0){
        close(g_socket);
        g_socket = -1;
    }
    if(g_dataParserptr){
        delete g_dataParserptr;
        g_dataParserptr = nullptr;
    }
}

int ClientSocket::init(){
    SendPacketThreadName = new std::thread(&ClientSocket::SendPacketThread, this);
    static std::thread s_handleParsedDataThread(&ClientSocket::handleParsedDataThread, this);
    g_inited = true;
    return 0;
}

int ClientSocket::SendData(char *data, unsigned dataLen){
    DPrintfMySocket("ClientSocket::SendData(dataLen=%d) \n", dataLen);
    if(!g_inited){
        EPrintfMySocket("Error: ClientSocket has not been inited \n");
        return -1;
    }
    if(dataLen > SEND_PACKET_MAX_LEN){
        EPrintfMySocket("Error: the len(%d) of data is too long, and the max len is %d \n", dataLen, SEND_PACKET_MAX_LEN);
        return -1;
    }
    SendPacket t_packet;
    t_packet.len = dataLen;
    memcpy(t_packet.data, data, dataLen);
    {
        std::lock_guard<std::mutex> lck(g_sendQueue.QueueMutex);
        g_sendQueue.DataQueue.push(t_packet);
        g_sendQueue.QueueCond.notify_one();
    }
    return 0;
}

//return dataLen
int ClientSocket::GetData(){
    if(!g_inited){
        EPrintfMySocket("Error: ClientSocket has not been inited \n");
        return 0;
    }
    if(g_dataParserptr->validBufLen() <= 0){
        EPrintfMySocket("Error: g_dataParserptr->validBufLen() < 0 \n");
        g_dataParserptr->clearLeftBuf();
        return 0;
    }
    std::vector<ReceivePacket> t_dataPacketVec;
    int dataLen = socketfdRecvAPacket(g_socket, Receivebuf.data(), g_dataParserptr->validBufLen());
    if(dataLen > 0){
        g_dataParserptr->ParseReceivedData(Receivebuf.data(), dataLen, t_dataPacketVec);
        for(ReceivePacket e : t_dataPacketVec){
            std::lock_guard<std::mutex> lck(g_receiveQueue.QueueMutex);
            DPrintfMySocket("Debug: e.Cmd=0x%x, e.DataLen=%d\n", e.Cmd, e.DataLen);
            if(e.Cmd == REPORT_CLIENT_ID_INFO || e.Cmd == 0xA1){
                DPrintfMySocket("debug: get a cmd(0x%x) \n", e.Cmd);
                std::string nameBuf(e.Buf+7, e.Buf+7+strnlen(e.Buf+7, e.DataLen));
                if(nameBuf == DISPATCH_SOCKET){
                   clientType = DISPATCHER_CLIENT;
                }else if(nameBuf == VIDEO_SOCKET){
                    clientType = DVR_CLIENT;
                }
                IPrintfMySocket("QT get REPORT_CLIENT_ID_INFO is %s, clientType=%d", nameBuf.data(), clientType);
            }
            printBuf(e.Buf+7, e.DataLen, "to push-queue data");
            g_receiveQueue.DataQueue.push(e);
            g_receiveQueue.QueueCond.notify_one();//通知ReceiveHandleThread从队列中取出数据来处理
        }

    }else{
        EPrintfMySocket("Error: fail to socketfdRecvAPacket, dataLen = %d, errnomsg(%s) \n", dataLen , strerror(errno));
    }
    return dataLen;
}

void ClientSocket::SendPacketThread(){
    signal(SIGPIPE, SIG_IGN);
    while(!g_stop){
        SendPacket t_struct;
        char* t_buf = (char *)t_struct.data;
        {
            std::unique_lock<std::mutex> lck(g_sendQueue.QueueMutex);
            g_sendQueue.QueueCond.wait(lck, [this]{
                return !g_sendQueue.DataQueue.empty() || g_sendQueue.stop;
            });
            if(g_sendQueue.stop){
                IPrintfMySocket("g_sendQueue.stop=true, SendPacketThread exit\n");
                break;
            }
            t_struct = g_sendQueue.DataQueue.front();
            g_sendQueue.DataQueue.pop();
        }
        if(t_struct.len > SEND_PACKET_MAX_LEN){
            EPrintfMySocket("Error: the len(%d) of SendPacket is too long, and the max len is %d \n", t_struct.len, SEND_PACKET_MAX_LEN);
            EPrintfMySocket("Error: Now throw the SendPacket \n");
        }else{
            DPrintfMySocket("socketfdSendAPacket(dataLen=%d) \n", t_struct.len);
            int ret = socketfdSendAPacket(g_socket, t_buf, t_struct.len);
            if(ret < 0){
                EPrintfMySocket("Error: fail to socketfd_send_a_packet, ret = %d, errnomsg(%s) \n", ret , strerror(errno));
                break;
            }
        }
    }
}

void ClientSocket::handleParsedDataThread(){
    signal(SIGPIPE, SIG_IGN);
    while(1){
        ReceivePacket t_struct;
        {
            std::unique_lock<std::mutex> lck(g_receiveQueue.QueueMutex);
            g_receiveQueue.QueueCond.wait(lck, [this]{
                return !g_receiveQueue.DataQueue.empty() || g_receiveQueue.stop;
            });
            if(g_receiveQueue.stop){
                IPrintfMySocket("g_sendQueue.stop=true, SendPacketThread exit\n");
                break;
            }
            t_struct = g_receiveQueue.DataQueue.front();
            g_receiveQueue.DataQueue.pop();
        }

        if(g_parseDataCallBack){
            DPrintfMySocket("debug: g_parseDataCallBack(0x%x, len=%d) \n", t_struct.Cmd, t_struct.DataLen);
            printBuf(t_struct.Buf+7, t_struct.DataLen, "to handle data");
            g_parseDataCallBack(t_struct.Cmd, t_struct.Buf, t_struct.DataLen);
        }else{
            EPrintfMySocket("error: g_parseDataCallBack is null \n");
        }
    }

}

int ClientSocket::socketfdSendAPacket(int socketfd, void* buf, int send_size){
    if (socketfd <= 0) {
        EPrintfMySocket("socket invalid:%d\n", socketfd);
        return -1;
    }
    return write_sure(socketfd, (unsigned char*)buf, send_size);
}

//read all data from socket if buf is nut full;会阻塞。
int ClientSocket::socketfdRecvAPacket(int socketfd, void* buf, int buf_size){
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
            printf("fail to read:%s \n", strerror(errno));
            return -1;
        }
        return len;
    }
    return -1;
}

