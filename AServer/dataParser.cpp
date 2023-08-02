#include "dataParser.h"
#include "../comdebug.h"
#include <algorithm>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdio>

#define QT_DISPATCH_COM_MIN_MSG_LEN 10
#define QT_DISPATCH_COM_HEAD_FLAG 0x24
#define QT_DISPATCH_COM_TAIL_FLAG 0x23

unsigned char MyCalculateAccWithoutCarry(const unsigned char* buf, const unsigned int len)
{
	unsigned char sum = 0;
	unsigned int i = 0;
	
	if (buf == NULL)
	{
		return -1;
	}
	
	for (i=0; i<len; i++)
	{
		sum += buf[i];
	}

	return sum;
}

/*return freelen of the leftBuf*/
unsigned dataParser::ParseReceivedData(char* str, unsigned len, std::vector<parsedData> &dataPacketVec){
    if(leftBufLen >= SEND_PACKET_MAX_LEN){
        EPrintfMySocket("some error happened so that LeftBuf has too long data, now clear it");
        leftBufLen = 0;
    }
    unsigned freeLen = (SEND_PACKET_MAX_LEN - leftBufLen);
    if(!str || len == 0 || len > freeLen){
        EPrintfMySocket("ParseReceivedData failed: error param(len=%d)", len);
        return freeLen;
    }
    
    memcpy((leftBuf + leftBufLen), str, len);
    leftBufLen += len;
    printBuf(leftBuf, leftBufLen, "leftBuf");

    int handledLen = ParseReceivedData2(leftBuf, leftBufLen, dataPacketVec);
    DPrintfMySocket("Debug: ParseReceivedData2 finish , handledLen=%d\n", handledLen);
    if(handledLen > 0 && (unsigned)handledLen <= leftBufLen){
        /*parse successfully*/
        leftBufLen -= handledLen;
        memmove(leftBuf, leftBuf+handledLen, leftBufLen);
    }else{
        /*handledLen==0 可能是因为读到半帧数据*/
        if(handledLen != 0){
            EPrintfMySocket("some error happened, and now clear the LeftBuf to recover th work, leftBufLen=%d", leftBufLen);
            leftBufLen = 0;
        }
    }
    return (SEND_PACKET_MAX_LEN - leftBufLen);
}

//return len of parsedData or -1 for error
int dataParser::ParseReceivedData2(char* str, unsigned len, std::vector<parsedData> &dataPacketVec)
{
    if(str == NULL)
    {
       EPrintfMySocket("ParseReceivedData2 Function error");
       return -1;
    }
    if(len < QT_DISPATCH_COM_MIN_MSG_LEN){
        return 0;
    }

    printBuf(str, len, "to parse data");
    unsigned int parsedDataLen = 0;
    char* end = str+len;
    char* search_ptr = str;
    char* head_ptr = str;
    char* tail_ptr = str;

    while(search_ptr < end){
        /*1.find the head*/
        head_ptr = std::search_n(search_ptr, end, 2, QT_DISPATCH_COM_HEAD_FLAG);
        if(head_ptr != end){
            /*found the head*/
            DPrintfMySocket("pos of head_ptr is %d \n", head_ptr-str);
            if((head_ptr+QT_DISPATCH_COM_MIN_MSG_LEN) > (str+len)){
                /*说明数据不足，等下次数据补全后再次进行解析*/
                parsedDataLen = head_ptr - str;
                return parsedDataLen;
            }

            /*2.check the len and tailPos*/
            unsigned short actualDataLen = head_ptr[2];
            actualDataLen = (actualDataLen << 8) + head_ptr[3];
            DPrintfMySocket("actualDataLen=%d", actualDataLen);
            tail_ptr = head_ptr + 2 + actualDataLen + 1;//2:两个字节的帧头，1：一个字节的ACC字节
            if(tail_ptr+1 > end){
                /*长度超过了缓存区数据长度*/
                DPrintfMySocket("debug: tail_ptr+1 > end");
                if((actualDataLen+5) > SEND_PACKET_MAX_LEN || actualDataLen <= 5){
                    DPrintfMySocket("debug: actualDataLen+5) > SEND_PACKET_MAX_LEN || actualDataLen <= 5");
                    /*数据长度无效，说明找错帧头或者该帧失效, 需要重新寻找帧头*/
                    search_ptr++;
                    continue;
                }else{
                    /*数据长度在有效范围，等下次数据补全再次解析*/
                    parsedDataLen = head_ptr - str;
                    return parsedDataLen;
                }
            }
            if(tail_ptr[0] != QT_DISPATCH_COM_TAIL_FLAG || tail_ptr[1] != QT_DISPATCH_COM_TAIL_FLAG){
                DPrintfMySocket("tail_ptr[0] != QT_DISPATCH_COM_TAIL_FLAG || tail_ptr[1] != QT_DISPATCH_COM_TAIL_FLAG");
                /*数据长度无效，说明找错帧头或者该帧失效, 需要重新寻找帧头*/
                search_ptr++;
                continue;
            }

            /*3.check the acc*/
            unsigned char acc_byte = MyCalculateAccWithoutCarry((unsigned char*)(head_ptr+2), actualDataLen);
            if(acc_byte != *(head_ptr+1+actualDataLen+1)){
                EPrintfMySocket("acc_byte error");
                /*数据无效，说明找错帧头或者该帧失效, 需要重新寻找帧头*/
                search_ptr++;
                continue;
            }

            /*4.the data is valid, now get the useful parts*/
            unsigned short cmdid = *((unsigned short*) &head_ptr[5]);
            DPrintfMySocket("debug: get a cmd(0x%x) \n", cmdid);
            unsigned short msgLen = actualDataLen - 5;
            DPrintfMySocket("debug: msgLen=%d \n", msgLen);
            parsedData t_dataPacket;
            t_dataPacket.init(cmdid, head_ptr, msgLen);//be sure
            dataPacketVec.push_back(t_dataPacket);

            /*开始解析下一帧*/
            search_ptr = tail_ptr+2;

        }else{
            /*从search_ptr开始找帧头，找不到的话，说明search之后的数据都是无效的，而search之前的数据又都是已经解析完的*/
            EPrintfMySocket("can not find the head \n");
            break;
        }
    }

    /*循环正常结束，说明缓存区中的所有数据都完全解析完成*/
    parsedDataLen = len;
    DPrintfMySocket("ParseReceivedData2 finish, parsedDataLen=%d \n", parsedDataLen);
    return parsedDataLen;
}
