#ifndef __HH__SIGSLOTCOMMON_
#define __HH__SIGSLOTCOMMON_
#include "sigslot.h"
#include <functional>

namespace msgBroadcastCommon{
    enum{ MAX_NAME_LENGTH = 16 };
    const unsigned short MessageMaxLen = 2046;
    struct Message{
        unsigned short msgCmd = 0;
        unsigned short msgLen = 0;
        unsigned char msgBuf[MessageMaxLen] = {0};
    };
    using slotFunPtr = std::function<void(Message*)>;
};

class msgReceiverBase : public sigslot::has_slots<>{
    public:
        virtual ~msgReceiverBase() = default;
        virtual void handleMsg(msgBroadcastCommon::Message* msgPtr) = 0;
};

class msgBroadcast{
    public:
        msgBroadcast() = default;
        ~msgBroadcast() = default;
        void broadcast(msgBroadcastCommon::Message *msgPtr){
            SendMessage(msgPtr);
            return;
        }
        void addReceiver(msgReceiverBase* new_receiver){
            SendMessage.connect(new_receiver, &msgReceiverBase::handleMsg);
            return; 
        }

    private:
        sigslot::signal1<msgBroadcastCommon::Message *> SendMessage;
};

#endif