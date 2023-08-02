/**
 * Copyright (C) 2022 Guang Zhou Tongda auto electric Co. Ltd. All rights reserved.
 * @file    clientsocket.h
 * @author  yrchen (1365145488@qq.com)
 * @version 1.0
 * @build   2023/05/13
 * @modify  2023/06/09
 * @brief   与QT通信的客户端管理类，接收客户端的数据并给客户端发送数据
 */

#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H

#include <atomic>
#include <thread>
#include <functional>

#include "./MyQueueStruct.h"
#include "formClass/CommonConfig.h" //for SOCKET_TYPE
//#include "dataparsed.h"

class DataParser;
struct ReceivePacket;

class ClientSocket
{
    typedef QTDISPATCH_COM_DATA SendPacket;
public:
    typedef std::function<void(const unsigned short& cmd,const char* buf,const unsigned short& data_len)> ParseDataCallBack;

public:
    ClientSocket(int socket);
    ~ClientSocket();
    int init();
    int SendData(char *data, unsigned dataLen);
    int GetData();//return dataLen
    inline int Socket() const {
        return g_socket;
    }
    inline SOCKET_TYPE Type() const {
        return clientType;
    }

    static ParseDataCallBack g_parseDataCallBack;

private:
    int g_socket = -1;
    SOCKET_TYPE clientType = CLIENT_UNKNOW;
    std::atomic<bool> g_stop;
    std::atomic<bool> g_inited;

    std::thread* SendPacketThreadName = nullptr;

    MyQueueStruct<SendPacket> g_sendQueue;
    static MyQueueStruct<ReceivePacket> g_receiveQueue;
    DataParser* g_dataParserptr = nullptr;
    std::vector<char> Receivebuf;

private:
    void SendPacketThread();
    void handleParsedDataThread();

    int socketfdSendAPacket(int socketfd, void* buf, int send_size);
    int socketfdRecvAPacket(int socketfd, void* buf, int buf_size);
};

#endif // CLIENTSOCKET_H
