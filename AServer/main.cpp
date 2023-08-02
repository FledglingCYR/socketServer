
#include "mainController.h"
#include "../comdebug.h"
#include <unistd.h>

int main(){
    DPrintfMySocket("main()\n");
    mainController serverController;
    serverController.init();
    while(1){
        sleep(1);
    }
}