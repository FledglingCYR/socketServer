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
        int Socket() const {
            return g_socket;
        }
    private:
        int socketfdRecvAPacket(int socketfd, void* buf, int buf_size);
        int g_socket = -1;
        dataParser* g_dataParser;
        std::vector<char> g_receiveBuf;
};

#endif //__H__CLIENTCONNECTOR_