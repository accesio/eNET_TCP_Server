#pragma once
#include <vector>

#include "safe_queue.h"
#include "TMessage.h"

typedef struct TActionQueueItemClass
{
	// pthread_t &sender; // which thread is responsible for sending results of the action to the client
	// TActinQueue &SendQueue; // which queue to stuff Responses into for sending to Clients
	int Socket; // which client is all this from/for
	TMessage &theMessage;
} TActionQueueItem;

typedef SafeQueue<TActionQueueItem *> TActionQueue;
TActionQueue ActionQueue;

void OpenDevFile();
void abort_handler(int s);
void Intro(int argc, char **argv);
void HandleNewAdcClients(int Socket, int addrSize, std::vector<int> &ClientList, struct sockaddr_in &addr, fd_set &ReadFDs);
void HandleNewControlClients(int Socket, int addrSize, std::vector<int> &ClientList, struct sockaddr_in &addr, fd_set &ReadFDs);
void *ActionThread(TActionQueue *Q);
void *ControlListenerThread(void *arg);
void *AdcListenerThread(void *arg);