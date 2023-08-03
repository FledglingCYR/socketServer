#ifndef __READ_WRITE_H__
#define __READ_WRITE_H__

int write_sure(int fd, unsigned char* buf, int len);
int read_sure(int fd, char* buf, int total_size);


#endif

