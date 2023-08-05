#ifndef __H__DATAPARSER_
#define __H__DATAPARSER_
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
#include <cstring>
#include <vector>

#define SEND_PACKET_MAX_LEN (1280)
struct parsedData{
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
class dataParser{
    public:
        dataParser() = default;
        ~dataParser() = default;
        unsigned ParseReceivedData(char* str, unsigned len, std::vector<parsedData> &dataPacketVec);
        unsigned validBufLen() const{
            return SEND_PACKET_MAX_LEN - leftBufLen;
        }
    private:
        int ParseReceivedData2(char* str, unsigned len, std::vector<parsedData> &dataPacketVec);

        unsigned leftBufLen = 0;
        char leftBuf[SEND_PACKET_MAX_LEN] = {0};

};

#endif //__H__DATAPARSER_