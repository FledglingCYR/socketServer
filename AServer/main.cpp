
#include "mainController.h"
#include <unistd.h>

int main(){
    mainController serverController;
    serverController.init();
    while(1){
        sleep(1);
    }
}