#include "clientConnector.h"
#include "dataParser.h"
#include "../comdebug.h"


#include <errno.h>//for errno
#include <string.h>//for strerror
#include <unistd.h>//for read

#include "../read_write.h"//for write_sure

clientConnector::clientConnector
    (int socket): g_socket(socket), g_dataParser(new dataParser()), g_receiveBuf(SEND_PACKET_MAX_LEN, 0)
{

}

int clientConnector::GetparsedData(std::vector<parsedData> &o_parsedDataVec){
    int dataLen = socketfdRecvAPacket(g_socket, g_receiveBuf.data(), g_dataParser->validBufLen());
    if(dataLen > 0){
        g_dataParser->ParseReceivedData(g_receiveBuf.data(), dataLen, o_parsedDataVec);
    }else{
        EPrintfMySocket("Error: fail to socketfdRecvAPacket, dataLen = %d, errnomsg(%s) \n", dataLen , strerror(errno));
    }
    return dataLen;
}

int clientConnector::SendData(char *data, unsigned dataLen){
    DPrintfMySocket("ClientSocket::SendData(dataLen=%d) \n", dataLen);
    if(dataLen > SEND_PACKET_MAX_LEN){
        EPrintfMySocket("Error: the len(%d) of data is too long, and the max len is %d \n", dataLen, SEND_PACKET_MAX_LEN);
        return -1;
    }
    printBuf(data, dataLen, "socketfdSendAPacket:");
    return socketfdSendAPacket(g_socket, data, dataLen);
}

//read all data from socket if buf is nut full;会阻塞。
int clientConnector::socketfdRecvAPacket(int socketfd, void* buf, unsigned buf_size){
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

int clientConnector::socketfdSendAPacket(int socketfd, void* buf, unsigned send_size){
    if (socketfd <= 0) {
        EPrintfMySocket("socket invalid:%d\n", socketfd);
        return -1;
    }
    return write_sure(socketfd, (unsigned char*)buf, (int)send_size);
}