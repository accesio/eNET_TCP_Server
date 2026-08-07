#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <signal.h>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "safe_queue.h"
#include "TMessage.h"

using TActionQueueItem = struct TActionQueueItemClass
{
    int Socket = -1;
    std::uint32_t ConnectionId = 0;
    std::shared_ptr<TMessage> Message;
    std::function<int(void)> Work;
    std::shared_ptr<std::promise<int>> Done;
};

using TActionQueue = SafeQueue<TActionQueueItem *>;
extern TActionQueue ActionQueue;
extern volatile sig_atomic_t done;

void OpenDevFile();
void exit_handler(int s);
void abort_handler(int s);
void Intro(int argc, char **argv);
void HandleNewAdcClients(int listenSocket);
void HandleNewControlClients(int listenSocket, socklen_t addrSize, struct sockaddr_storage &addr);
void *ActionThread(TActionQueue *queue);
void *ControlListenerThread(void *arg);
void *AdcListenerThread(void *arg);

int AioEnetd_RunSerialized(const char *name, std::function<int(void)> work, unsigned timeout_ms);
