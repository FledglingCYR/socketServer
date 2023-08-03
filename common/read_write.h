#ifndef __READ_WRITE_H__
#define __READ_WRITE_H__

#ifdef __cplusplus
    extern "C"{
#endif

int write_sure(int fd, unsigned char* buf, int len);
int read_sure(int fd, char* buf, int total_size);

#ifdef __cplusplus
    }
#endif

#endif

