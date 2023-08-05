#ifndef COMDEBUG_H
#define COMDEBUG_H

#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

#define MySocketDebug 0
#define EPrintfMySocket(fmt, arg...){\
    printf(fmt, ##arg);\
}
#define IPrintfMySocket(fmt, arg...){\
    printf(fmt, ##arg);\
}
#define DPrintfMySocket(fmt, arg...){\
    printf(fmt, ##arg);\
}
inline void printBuf(unsigned char* buf, unsigned bufLen, std::string title){
    std::stringstream ss;
    std::for_each(buf, buf+bufLen, [&](unsigned char e){
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(e) << " ";
    });
    std::string hex_str = ss.str();
    DPrintfMySocket("%s: %s \n", title.c_str(), hex_str.c_str());
}
#endif // COMDEBUG_H
