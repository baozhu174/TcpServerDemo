#ifndef _DESTROY_MANAGER_H_
#define _DESTROY_MANAGER_H__

#include <stdint.h>

#include "terminal.h"


void AddToDestroyManagerList(struct _t_Terminal **tClient);

void InitDestroyManager();
void StopDestroyManager();
void HandleDestroyThreadfunc(void *arg);


#endif /* _DESTROY_MANAGER */

