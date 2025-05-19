#pragma once
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "safe_queue.h"
#include "TMessage.h"

using TActionQueueItem = struct TActionQueueItemClass
{
	// pthread_t &sender; // which thread is responsible for sending results of the action to the client
	// TActinQueue &SendQueue; // which queue to stuff Responses into for sending to Clients
	int Socket; // which client is all this from/for
	TMessage &theMessage;
};

// using TSendQueueItem = struct TSendQueueItemClass
// {
// 	// which TCP-per-client-read thread put this item into the Action Queue
// 	pthread_t &receiver;
// 	// which thread is responsible for sending results of the action to the client
// 	pthread_t &sender;
// 	// which queue is the sender-thread popping from
// 	SafeQueue<TSendQueueItemClass> &sendQueue;
// 	// which client is all this from/for
// 	int clientref;
// 	// what TCP port# was this received on
// 	int portReceive;
// 	// what TCP port# is this sending out on
// 	int portSend;
// };


using TActionQueue = SafeQueue<TActionQueueItem *>;
TActionQueue ActionQueue; // NOTE: This instantiates, but this is a header file! bad bad should be extern, no?

void OpenDevFile();
void exit_handler(int s);
void abort_handler(int s);
void Intro(int argc, char **argv);
void HandleNewAdcClients(int Socket, int addrSize, std::vector<int> &ClientList, struct sockaddr_storage &addr);
void HandleNewControlClients(int Socket, socklen_t addrSize, struct sockaddr_storage &addr );
void *ActionThread(TActionQueue *Q);
void *ControlListenerThread(void *arg);
void *AdcListenerThread(void *arg);

