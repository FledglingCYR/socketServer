#include "AClient.h"


int main(){
    AClient client1;
    client1.init();
    AClient client2;
    client2.init();
    unsigned char buf[] = {0x24, 0x24, 0x0, 0xa, 0x1, 0xf1, 0x0, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x1e, 0x23, 0x23};
    while(1){
        getchar();
        client1.sendDataToServer(buf, sizeof(buf));
    }
    return 0;
}