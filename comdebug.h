#ifndef COMDEBUG_H
#define COMDEBUG_H

#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

#define MySocketDebug 0
#define EPrintfMySocket(fmt, TDlog_arg...){\
    printf(fmt, ##TDlog_arg);\
}
#define IPrintfMySocket(fmt, TDlog_arg...){\
    printf(fmt, ##TDlog_arg);\
}
#define DPrintfMySocket(fmt, TDlog_arg...){\
    printf(fmt, ##TDlog_arg);\
}
inline void printBuf(char* buf, unsigned bufLen, std::string title){
    std::stringstream ss;
    std::for_each(buf, buf+bufLen, [&](char e){
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(e) << " ";
    });
    std::string hex_str = ss.str();
    DPrintfMySocket("%s: %s", title.c_str(), hex_str.c_str());
}
#endif // COMDEBUG_H
