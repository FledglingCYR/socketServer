#include "mainController.h"
#include "sigslotCommon.h"
#include "comdebug.h"
#include "signalHander.hpp"

#include <unistd.h>
#include <iostream>
#include <string.h>

class externalModule : public msgReceiverBase{
    public:
        externalModule() = default;
        ~externalModule() = default;
        virtual void handleMsg(msgBroadcastCommon::Message* msgPtr){
            printBuf(msgPtr->msgBuf, msgPtr->msgLen, "handleMsg");
        }
};

int main(){
    
    InitSysSignal();

    IPrintfMySocket("main()\n");
    mainController serverController("/tmp/.qtDPH_srv");
    serverController.init();

    externalModule exModule;
    msgReceiverBase* ReceiverPtr = static_cast<msgReceiverBase*>(&exModule);
    serverController.addReceiver(ReceiverPtr);

    unsigned char buf[] = {0x24, 0x24, 0x0, 0xa, 0x1, 0xf1, 0x0, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x1e, 0x23, 0x23};
    while(1){
        getchar();
        serverController.broadcastToClient(buf, sizeof(buf));
    }
}