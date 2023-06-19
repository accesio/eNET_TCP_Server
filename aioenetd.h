#pragma once

typedef struct TActionQueueItemClass
{
	// pthread_t &sender; // which thread is responsible for sending results of the action to the client
	// TActinQueue &SendQueue; // which queue to stuff Responses into for sending to Clients
	int Socket; // which client is all this from/for
	TMessage &theMessage;
} TActionQueueItem;

typedef SafeQueue<TActionQueueItem *> TActionQueue;
// SafeQueue<pthread_t> ReceiverThreadQueue;
TActionQueue ActionQueue;
// TActionQueue ReplyQueue; // J2H: consider one per ReceiveThread...(i.e., make one ReplyThread per ReceiveThread, each with an associated queue)

void OpenDevFile();
void abort_handler(int s);
void Intro(int argc, char **argv);
void HandleNewAdcClients(int Socket, int addrSize, std::vector<int> &ClientList, struct sockaddr_in &addr, fd_set &ReadFDs);
void HandleNewControlClients(int Socket, int addrSize, std::vector<int> &ClientList, struct sockaddr_in &addr, fd_set &ReadFDs);
void *ActionThread(TActionQueue *Q);
void *ControlListenerThread(void *arg);
void *AdcListenerThread(void *arg);