#pragma once

#include <pthread.h>

#include "webctl.h"

const WebCtlDeviceOps *WebCtlAioEnetd_DeviceOps(void);
int WebCtlAioEnetd_Start(pthread_t *thread);
void WebCtlAioEnetd_Stop(void);
