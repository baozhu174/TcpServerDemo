#ifndef _READ_MANAGER_H_
#define _READ_MANAGER_H_

#include <stdint.h>

#include "terminal.h"

#define IP_INS_RBUFFER_LEN          1024*1024*10
#define IP_INS_PACKET_HEAD_LEN      12
#define IP_INS_LEN_MAX              4096
#define IP_INS_HEAD_SIGN_LEN        2
#define STB_ID_LEN                  8


void AddToReadManagerList(struct _t_Terminal **cli);

void InitReadManager();
void StopReadManager();
void HandleReadThreadfunc(void *arg);


#endif /* _READ_MANAGER_H_ */

