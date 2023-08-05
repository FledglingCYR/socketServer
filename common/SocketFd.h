#ifndef SOCKETFD_H
#define SOCKETFD_H

#ifdef __cplusplus
extern "C" {
#endif

#define SOCKET_PEER_CLOSED 1
#define SOCKET_TIMEOUT 2


int socketfd_recv_stream_timeout(int socketfd, void* buf, int buf_size, int *byte_read, int timeout_ms);
int socketfd_send(int socketfd, void* buf, int send_size);
int socketfd_send_timeout(int socketfd, const void* buf, int send_size, int time_ms, int *timeouted);
int socketfd_send_a_packet(int socketfd, void* buf, int send_size);
int socketfd_recv_a_packet(int socketfd, void* buf, int buf_size, int *len);
int socketfd_recv_a_packet_new(int socketfd, void* buf, int buf_size, int *len);
int socketfd_recv_a_packet_timeout(int socketfd, void* buf, int buf_size, int *len, int timeout_ms);
int socketfd_clean_recv_message(int socketfd);

#ifdef __cplusplus
}
#endif

#endif

