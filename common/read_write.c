/********************************************************************************
**  (C) COPYRIGHT 2018 
**	author	:	HuangZiBin
**  E-mail	:	635568706@qq.com
** 	time	:	2019-09-27 13:44
**	If finding bugs, bother you contacting me.Thank you
********************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "select.h"

#define FILE_NAME "../read_write.c"
#define BUF_SIZE 10000

int read_file_block(int fd, char* buf, int total_size, int* eof)
{	
	int read_size = 0;
	int ret = 0;
	while(1) {
		if (read_size >= total_size) {
			ret = read_size;
			break;
		}
		ret = read(fd, buf, total_size-read_size);
		if (ret  < 0) {
			if(errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
				fprintf(stderr, "write failed, error %s\n", strerror(errno));
				goto out;
			}else {
				continue;
			}
		}
		if (ret == 0) {
			if (read_size == 0) {
				*eof = 1;
				ret = 0;
				goto out;
			}else {
				ret = read_size;
				goto out;
			}
		}
		buf+= ret;
		read_size+=ret;
	}
out:
	return ret;
}

int read_and_print()
{
	int ret = 0;
	int fd = open(FILE_NAME, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open file %s failed, error:%s\n", FILE_NAME, strerror(errno));
		return -1;
	}
	char data[BUF_SIZE];
	while(1) {
		bzero(data, BUF_SIZE);
		int eof = 0;
		ret = read_file_block(fd, data, BUF_SIZE-1, &eof);
		if (eof > 0) {
			fprintf(stdout, "read file end.\n");
			break;
		}
		if (ret < 0) {
			break;
		}
		fprintf(stdout, "%s", data);
	}
	close(fd);
	return 0;
}


//Write buffer to fd.
//Can be sure that specified number of bytes are worte. Fd can be block or nonblock.
//On success, the function return 1. 
//On error, -1 is returned, and errno is set appropriately.
int write_sure(int fd, unsigned char* buf, int len)
{
	int ret = 0;
	int already_wrote_bytes = 0;
	while(already_wrote_bytes < len) {
		ret = my_write_select(fd, 100);
		if (ret < 0) {
			return ret;
		}
		if (ret == 0) {
			continue;
		}
		ret = write(fd, buf+already_wrote_bytes, len-already_wrote_bytes);
		if (ret < 0 && (errno == EAGAIN || errno == EINTR)) {
			continue;
		}
		if (ret <= 0) {
			return ret;
		}
		already_wrote_bytes += ret;
	}
	return 1;
}

/*
	读取total_size大小的数据。遇到没有数据的情况，阻塞
	返回读取到的字节数，如果读取失败则返回-1并设置errno。	如读取到的字节数小于设置的字节数，意味着对端关闭。
	注意：返回值的设计不太合理，参考fread修改；
	已处理信号中断和EAGAIN。
*/
int read_sure(int fd, char* buf, int total_size)
{	
	int read_size = 0;
	int ret = 0;
	while(1) {
		if (read_size >= total_size) {
			ret = read_size;
			break;
		}
		ret = my_read_select(fd, 500);
		if (ret < 0) {
			fprintf(stderr, "select failed, error %s\n", strerror(errno));			
			ret = -1;
			goto out;
		}else if (ret == 0) {
			continue;
		}
		ret = read(fd, buf, total_size-read_size);
		if (ret  < 0) {
			if(errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
				fprintf(stderr, "read failed, error %s\n", strerror(errno));
				goto out;
			}else {
				continue;
			}
		}
		if (ret == 0) {
			ret = read_size;
			goto out;
		}
		buf+= ret;
		read_size+=ret;
	}
out:
	return ret;
}


