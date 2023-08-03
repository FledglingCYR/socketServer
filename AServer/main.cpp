
#include "mainController.h"
#include "../comdebug.h"
#include <unistd.h>

int main(){
    DPrintfMySocket("main()\n");
    mainController serverController;
    serverController.init();
    char buf[] = {0x24, 0x24, 0x23, 0x23, 0x23, 0x24, 0x24, 0x21, 0x23, 0x23};
    while(1){
        getchar();
        serverController.broadcastToClient(buf, sizeof(buf));
    }
}