#pragma once
#include <pthread.h>
// ADC Streaming-related stuff for eNET-AIO Family hardware


extern volatile int AdcStreamTerminate;
void *worker_main(void *arg);
extern pthread_t worker_thread;
extern pthread_t logger_thread;
extern int AdcStreamingConnection;
extern int AdcWorkerThreadID;