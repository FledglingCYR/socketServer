#ifndef __H__CLIENTCONNECTOR_
#define __H__CLIENTCONNECTOR_

// #include "dataParser.h"
#include <vector>
class dataParser;
struct parsedData;
class clientConnector{
    public:
        explicit clientConnector(int socket);
        ~clientConnector() = default;
        int GetparsedData(std::vector<parsedData> &o_parsedDataVec);
        int SendData(char *data, unsigned dataLen);
        int Socket() const {
            return g_socket;
        }
    private:
        int socketfdRecvAPacket(int socketfd, void* buf, unsigned buf_size);
        int socketfdSendAPacket(int socketfd, void* buf, unsigned send_size);
        int g_socket = -1;
        dataParser* g_dataParser;
        std::vector<char> g_receiveBuf;
};

#endif //__H__CLIENTCONNECTOR_