#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "SocketFd.h"
#include "read_write.h"
#include "select.h"

#define CMDHEAD (0xAA5AA555)

struct MessageHead
{
	unsigned int head; /*头标识， 0xAA5AA555*/
	int message_len;
};

#define SOCKETFD_LOG_ERR(format, arg...) do {\
			fprintf(stderr, format, ##arg);\
		}while(0)
	
#define SOCKETFD_LOG_DEGUG(format, arg...) do {\
				fprintf(stdout, format, ##arg);\
			}while(0)

int socketfd_recv_stream(int socketfd, void* buf, int buf_size, int *byte_read)
{
	int ret = 0;
	if (socketfd <= 0) {
		SOCKETFD_LOG_ERR("socket invalid:%d\n", socketfd);
		return -1;
	}
	ret = read(socketfd, buf, buf_size);
    if (ret < 0) {
        SOCKETFD_LOG_ERR("read failed:%s\n", strerror(errno));
		return -1;
    }
	if (ret == 0) {
        SOCKETFD_LOG_DEGUG("peer close the connection\n");
		return SOCKET_PEER_CLOSED;
	}
	*byte_read = ret;
	return 0;
}

int socketfd_recv_stream_timeout(int socketfd, void* buf, int buf_size, int *byte_read, int timeout_ms)
{
	int ret = 0;
	ret = my_read_select(socketfd, timeout_ms);
	if ( ret < 0) {
		return -1;
	}else if (ret == 0){
		return SOCKET_TIMEOUT;
	}
	ret = socketfd_recv_stream(socketfd, buf, buf_size, byte_read);
	return ret;	
}

int socketfd_send(int socketfd, void* buf, int send_size)
{
	//int ret = 0;
	if (socketfd <= 0) {
		SOCKETFD_LOG_ERR("socket invalid:%d\n", socketfd);
		return -1;
	}
    if (write(socketfd, buf, send_size) < 0) {
        SOCKETFD_LOG_ERR("write failed:%s\n", strerror(errno));
		return -1;
    }
	return 0;
}

/*
	返回值：
		失败，返回-1；否则返回已发送的字节数
	参数：
		time_ms，入参，指定超时毫秒数
		timeouted，出参，指示是否超时
	说明：
		socketfd要求是非阻塞的，否则超时有可能失效。
*/
int socketfd_send_timeout(int socketfd, const void* buf, int send_size, int time_ms, int *timeouted)
{
	int ret = 0;
	*timeouted = 0;
	ret = my_write_select(socketfd, time_ms);
	if ( ret < 0) {
		return -1;
	}else if (ret == 0){
		*timeouted = 1;
		return 0;
	}
    if ( (ret = write(socketfd, buf, send_size)) < 0) {
        SOCKETFD_LOG_ERR("write failed:%s\n", strerror(errno));
		return -1;
    }
	return ret;
}

/*
	发送一包数据，可能阻塞。
	失败，返回-1（包括对端关闭）。已处理信号中断。
	成功，返回1
	注意：返回值不完善。
*/
int socketfd_send_a_packet(int socketfd, void* buf, int send_size)
{
	int ret = 0;
	if (socketfd <= 0) {
		SOCKETFD_LOG_ERR("socket invalid:%d\n", socketfd);
		return -1;
	}
	struct MessageHead head = {0};
	head.head = CMDHEAD;
	head.message_len = send_size;
	ret = write_sure(socketfd, (unsigned char*)&head, sizeof(head));
	if (ret <= 0) {
		return ret;
    }
	return write_sure(socketfd, buf, send_size);
}

/*
	对端关闭，返回 SOCKET_PEER_CLOSED
	成功，返回0
	失败，返回-1
	注意：有缺陷，没有考虑被信号打断的情况；末尾加了0
*/
int socketfd_recv_a_packet(int socketfd, void* buf, int buf_size, int *len)
{
	int ret = 0;
	if (socketfd <= 0) {
		SOCKETFD_LOG_ERR("socket invalid:%d\n", socketfd);
		return -1;
	}
	struct MessageHead head = {0};
	ret = read(socketfd, &head, sizeof(head));
	if (ret < 0) {
        SOCKETFD_LOG_ERR("read failed:%s\n", strerror(errno));
		return -1;
    } else if (ret == 0) {
        SOCKETFD_LOG_DEGUG("peer close the connection\n");
		return SOCKET_PEER_CLOSED;		
	}
	if (head.message_len+1 > buf_size) {
        SOCKETFD_LOG_ERR("data too large, %d, max:%d\n", head.message_len+1, buf_size);
		return -1;
	}
	ret = read(socketfd, buf, head.message_len);
	if (ret < 0) {
        SOCKETFD_LOG_ERR("read failed:%s\n", strerror(errno));
		return -1;
    } else if (ret == 0) {
        SOCKETFD_LOG_DEGUG("peer close the connection\n");
		return SOCKET_PEER_CLOSED;		
	}
	*len = head.message_len;
	((char*)buf)[head.message_len] = '\0';
	return 0;
}

/*
	读取一包数据，可能阻塞。
	对端关闭，返回 SOCKET_PEER_CLOSED
	成功，返回0
	失败，返回-1
	已处理被信号打断的情况
*/
int socketfd_recv_a_packet_new(int socketfd, void* buf, int buf_size, int *len)
{
	int ret = 0;
	if (socketfd <= 0) {
		SOCKETFD_LOG_ERR("socket invalid:%d\n", socketfd);
		return -1;
	}
	struct MessageHead head = {0};
	ret = read_sure(socketfd, (char*)&head, sizeof(head));
	//ret = read(socketfd, &head, sizeof(head));
	if (ret < 0) {
        SOCKETFD_LOG_ERR("read failed:%s\n", strerror(errno));
		return -1;
    } else if (ret < sizeof(head)) {
        SOCKETFD_LOG_DEGUG("peer close the connection\n");
		return SOCKET_PEER_CLOSED;		
	}
	if (head.message_len > buf_size) {
        SOCKETFD_LOG_ERR("data too large, %d, max:%d\n", head.message_len, buf_size);
		return -1;
	}
	ret = read_sure(socketfd, buf, head.message_len);
	//ret = read(socketfd, buf, head.message_len);
	if (ret < 0) {
        SOCKETFD_LOG_ERR("read failed:%s\n", strerror(errno));
		return -1;
    } else if (ret < head.message_len) {
        SOCKETFD_LOG_DEGUG("peer close the connection\n");
		return SOCKET_PEER_CLOSED;		
	}
	*len = head.message_len;
	return 0;
}

//return 0 on recv a packet succeed.
int socketfd_recv_a_packet_timeout(int socketfd, void* buf, int buf_size, int *len, int timeout_ms)
{
	int ret = 0;
	ret = my_read_select(socketfd, timeout_ms);
	if ( ret < 0) {
		return -1;
	}else if (ret == 0){
		return SOCKET_TIMEOUT;
	}
	ret = socketfd_recv_a_packet(socketfd, buf, buf_size, len);
	return ret;
}


int socketfd_clean_recv_message(int socketfd)
{
	int ret = 0;
	char buf[2048];
	while(1) {
		ret = my_read_select(socketfd, 0);
		if ( ret < 0) {
			return -1;
		}else if (ret > 0){
			ret = read(socketfd, buf, sizeof(buf));
			if (ret < 0) {
      			SOCKETFD_LOG_ERR("read failed:%s\n", strerror(errno));
				return -1;
			}
		}else {
			break;
		}
	}
	return 0;
}

/********************************************************************************
socket end
********************************************************************************/

