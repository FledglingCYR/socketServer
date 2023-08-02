/**
 * Copyright (C) 2022 Guang Zhou Tongda auto electric Co. Ltd. All rights reserved.
 * @file    server.h
 * @author  yrchen (1365145488@qq.com)
 * @version 1.0
 * @build   2023/05/13
 * @modify  2023/05/18
 * @brief   重新构建服务端，实现与调度进程和录像进程的通信。
 */

#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include "comdebug.h"

#include <poll.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>

class ClientSocket;
typedef std::function<void(const unsigned short& cmd,const char* buf,const unsigned short& data_len)> ParseDataCallBack;

class SocketServer 
{
public:
    static SocketServer& getInstance();
    ~SocketServer();
    int init(ParseDataCallBack i_parseDataCallBack);
    /*return the num of the client, which successfully send data*/
    int BroadcastToClient(char* data, unsigned data_len, SOCKET_TYPE client_type = CLIENT_UNKNOW);
    int BroadcastToClient(QTDISPATCH_COM_DATA& package);
private:
    SocketServer();
    void clientMonitorThread();
    void receiveThread();

    int initServer();
    int acceptANewClient(const int &pollFd);
    int getDataFromAllClient(const std::vector<pollfd>& t_pollFdVec, int& handledCount);

    const char ServerName[27] =  QTDISPATCH_SERVER_FD_LOCAL_SOCK_NAME;
    const unsigned maxSocketClientNum = 2;
    int g_serverFd = -1;
    std::atomic<bool> g_stop;
    std::vector<ClientSocket*> g_clientVec;
    std::mutex g_clientVecMutex;
    std::thread* g_clientMonitorThreadName = nullptr;
};

#endif // SOCKETSERVER_H
