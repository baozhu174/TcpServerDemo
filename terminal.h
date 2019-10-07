#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include "kernel_list.h"

#define IP_INS_LEN_MAX              4096




struct _t_Terminal
{
	pthread_mutex_t  recv_mutex;
	pthread_mutex_t  send_mutex;
	int				 recv_event;
	int				 epoll_fd;
    int              fd;
    time_t           last_hb;
};

extern struct _t_Terminal **ppTest_ptr;
extern int test_fd;


int SetNonBlocking(int sock);
ssize_t SocketSend(int sockfd, const uint8_t* buffer, size_t buflen);

void Init_TcpTerminal(struct _t_Terminal *cli);
struct _t_Terminal **Create_TcpTerminalObj(int epoll_fd, int socket_fd);
void Destory_TcpTerminalObj(struct _t_Terminal **ppCli);
int SetTcpTerminalRecvEvent(struct _t_Terminal *cli);
void ResetTcpTerminalRecvevent(struct _t_Terminal *cli);
void Close_TcpTerminal(struct _t_Terminal **ppCli);
int Recv_TcpTerminal(struct _t_Terminal *cli, uint8_t *data, int *size);
int Send_TcpTerminal(struct _t_Terminal *cli, const uint8_t *data, const int size);
int Register_TcpTerminal(struct _t_Terminal **ppCli);


#endif /* #define _TERMINAL_H_ */
