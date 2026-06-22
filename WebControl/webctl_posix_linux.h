#pragma once

#include <pthread.h>

#include "webctl.h"

#ifndef AIOENETD_WEB_HTTP_PORT
#define AIOENETD_WEB_HTTP_PORT 80
#endif

#ifndef AIOENETD_WEB_ROOT
#define AIOENETD_WEB_ROOT "/home/acces/www"
#endif

#ifndef AIOENETD_WEB_RESPONSE_BUFFER_SIZE
#define AIOENETD_WEB_RESPONSE_BUFFER_SIZE 8192u
#endif

#ifndef AIOENETD_WEB_REQUEST_HEADER_MAX
#define AIOENETD_WEB_REQUEST_HEADER_MAX 8192u
#endif

#ifndef AIOENETD_WEB_POST_BODY_MAX
#define AIOENETD_WEB_POST_BODY_MAX 2048u
#endif

#ifndef AIOENETD_WEB_AUTH_ENABLE
#define AIOENETD_WEB_AUTH_ENABLE 1
#endif

#ifndef AIOENETD_WEB_AUTH_USERNAME
#define AIOENETD_WEB_AUTH_USERNAME "admin"
#endif

#ifndef AIOENETD_WEB_AUTH_PASSWORD
#define AIOENETD_WEB_AUTH_PASSWORD "admin"
#endif

#ifndef AIOENETD_WEB_AUTH_REALM
#define AIOENETD_WEB_AUTH_REALM "aioenetd"
#endif

int WebCtlPosixLinux_Start(pthread_t *thread, const WebCtlDeviceOps *device);
void WebCtlPosixLinux_Stop(void);
