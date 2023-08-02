/**
 * Copyright (C) 2022 Guang Zhou Tongda auto electric Co. Ltd. All rights reserved.
 * @file    dataparsed.h
 * @author  yrchen (1365145488@qq.com)
 * @version 1.0
 * @build   2023/06/09
 * @modify  2023/06/09
 * @brief   解析从客户端传来的数据, 接口都是线程不安全的，只能用于一个线程;详细逻辑参考../../../doc/中的流程图
 */
/**
 * example:0x24, 0x24, 0x0, 0xa, 0x1, 0xf1, 0x0, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x1e, 0x23, 0x23
 * parse:
 *  head: 0x24, 0x24
 *  Len: 0x0, 0xa
 *  cmd: 0xf1, 0x0
 *  msgBOdy: 0x65, 0x76, 0x65, 0x6e, 0x74
 *  accByte: 0x1e
 *  tail: 0x23, 0x23
 */
#ifndef DATAPARSED_H
#define DATAPARSED_H

#include "MyQueueStruct.h"
#include <vector>
#include <cstring>
#include <algorithm>

#define SEND_PACKET_MAX_LEN (1280)
#define QT_DISPATCH_COM_MIN_MSG_LEN 10
#define QT_DISPATCH_COM_HEAD_FLAG 0x24
#define QT_DISPATCH_COM_TAIL_FLAG 0x23



struct ReceivePacket{
    unsigned short Cmd = 0;
    char Buf[SEND_PACKET_MAX_LEN] = "";
    unsigned short DataLen = 0;
    /* be sure  data_len<=QTDISPATCH_COMMUNICATION_DATA_MAX_LEN-7 */
    unsigned short init(const unsigned short& cmd, char* buf, const unsigned short& data_len){
        Cmd = cmd;
        DataLen = data_len;
        if(buf != nullptr && DataLen<=SEND_PACKET_MAX_LEN-7) // 7:the length of head
            memcpy(Buf, buf, DataLen+7);
        else
            DataLen = 0;
        return DataLen;
    }
};

class DataParser
{
public:
    DataParser();
    unsigned ParseReceivedData(char* str, unsigned len, std::vector<ReceivePacket> &dataPacketVec);
    unsigned validBufLen() const{
        return SEND_PACKET_MAX_LEN - leftBufLen;
    }
    void clearLeftBuf(){
        leftBufLen = 0;
    }
private:
    int ParseReceivedData2(char* str, unsigned len, std::vector<ReceivePacket> &dataPacketVec);
    unsigned leftBufLen = 0;
    char leftBuf[SEND_PACKET_MAX_LEN] = {0};


};

#endif // DATAPARSED_H
