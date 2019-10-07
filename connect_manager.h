#ifndef _CONNECT_MANAGER_H_
#define _CONNECT_MANAGER_H_

#include <stdint.h>

#include "terminal.h"


void InitConnectListManager();
int AddTerminalToConnectList_Direct(struct _t_Terminal          **tCli);

int AddTerminalFDToConnectList(int fd, int epoll_fd);


int SendCommandToTerminalByFd(int fd, uint8_t *data, size_t datasize);
int SendCommandToAllTerminals(uint8_t *data, size_t datasize);


void InitTestSendThread();


#endif /* _CONNECT_MANAGER_H_ */

