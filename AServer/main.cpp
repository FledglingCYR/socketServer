
#include "mainController.h"
#include "sigslotCommon.h"
#include "comdebug.h"
#include <unistd.h>
#include <iostream>
#include <string.h>

#include <fstream>
#include <signal.h>
#include <chrono>
#include <ctime>
#include <execinfo.h>
std::string GetISODate(){
	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(tp);
	std::string str = std::ctime(&t);
	str.resize(str.size()-1);
	return str;
}

static void sig_crash(int sig) {
    signal(sig, SIG_DFL);
    
    const int MAX_STACK_FRAMES = 128;
    void* array[MAX_STACK_FRAMES];
    int size = backtrace(array, MAX_STACK_FRAMES);
    char** strings = backtrace_symbols(array, size);
    std::vector<std::string> stack(size);
    for (int i = 0; i < size; ++i) {
        std::string symbol(strings[i]);
        stack[i] = symbol;
    }
    free(strings);

    std::stringstream ss;
    std::string crash_time = GetISODate();
    ss << "## crash date:" << crash_time << std::endl;
    ss << "## exe:       " << "dispatch" << std::endl;
    ss << "## signal:    " << sig << std::endl;
    ss << "## stack:     " << std::endl;
    for (size_t i = 0; i < stack.size(); ++i) {
        ss << "[" << i << "]: " << stack[i] << std::endl;
    }
    std::string crash_info_file = "/tmp/" + crash_time;
    std::string stack_info = ss.str();
    std::ofstream out(crash_info_file, std::ios::out | std::ios::binary | std::ios::trunc);
    out << stack_info;
    out.flush();
    std::cerr << stack_info << std::endl;
}

__attribute__ ((unused))
static void InitSysSignal() {
	signal(SIGSEGV, sig_crash);
	signal(SIGABRT, sig_crash);
	signal(SIGPIPE, SIG_IGN);
    // signal(SIGINT, [](int) { terminate_prog = true; });
}

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
    mainController serverController;
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