#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <linux/types.h>
#include <mutex>
#include <poll.h>
#include <semaphore.h>
#include <signal.h>
#include <string>
#include <thread>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

#include "logging.h"
#include "eNET-AIO16-16F.h"
#include "apci.h"
#include "apcilib.h"
#include "daq_state.h"
#include "adc.h"

extern volatile sig_atomic_t done;

namespace
{
constexpr std::uint32_t ADC_CONNECTION_ID_MAX = 0x7FFFFFFFu;
constexpr auto WORKER_INIT_TIMEOUT = std::chrono::seconds(5);
constexpr long SEM_WAIT_NS = 100000000L;

static std::uint32_t ring_buffer[RING_BUFFER_SLOTS][SAMPLES_PER_TRANSFER];

struct TAdcDataConnection
{
    TAdcConnectionId Id = ADC_INVALID_CONNECTION_ID;
    int Socket = -1;
    std::atomic_bool Connected{true};
};

struct TAdcWorkerContext
{
    std::uint64_t Generation = 0;
    std::shared_ptr<TAdcDataConnection> DataConnection;
    std::atomic_bool StopRequested{false};
    std::atomic_bool AcquisitionStarted{false};
    std::atomic_bool SyncWakeupsEnabled{false};
    sem_t Empty{};
    sem_t Full{};
    bool EmptyInitialized = false;
    bool FullInitialized = false;
    pthread_t LoggerThread{};
    bool LoggerStarted = false;
    void *MmapAddress = MAP_FAILED;
};

struct TAdcStreamState
{
    std::uint64_t Generation = 0;
    TAdcConnectionId ControlConnectionId = ADC_INVALID_CONNECTION_ID;
    std::shared_ptr<TAdcDataConnection> DataConnection;
    std::shared_ptr<TAdcWorkerContext> Context;
    pthread_t WorkerThread{};
    bool Active = false;
    bool WorkerInitialized = false;
    int WorkerInitStatus = 0;
    bool WorkerJoinable = false;
    bool JoinInProgress = false;
};

std::mutex AdcStateMutex;
std::condition_variable AdcStateChanged;
std::unordered_map<TAdcConnectionId, int> ControlConnections;
std::unordered_map<TAdcConnectionId, std::shared_ptr<TAdcDataConnection>> DataConnections;
TAdcStreamState Stream;
TAdcConnectionId NextConnectionId = 1;
std::uint64_t NextStreamGeneration = 1;

TAdcConnectionId AllocateConnectionIdLocked()
{
    for (std::uint32_t attempt = 0; attempt < ADC_CONNECTION_ID_MAX; ++attempt)
    {
        const TAdcConnectionId candidate = NextConnectionId;
        ++NextConnectionId;
        if (NextConnectionId == ADC_INVALID_CONNECTION_ID || NextConnectionId > ADC_CONNECTION_ID_MAX)
            NextConnectionId = 1;

        if (candidate != ADC_INVALID_CONNECTION_ID && ControlConnections.find(candidate) == ControlConnections.end() && DataConnections.find(candidate) == DataConnections.end())
            return candidate;
    }

    return ADC_INVALID_CONNECTION_ID;
}

std::uint64_t AllocateStreamGenerationLocked()
{
    const std::uint64_t generation = NextStreamGeneration++;
    if (NextStreamGeneration == 0)
        NextStreamGeneration = 1;
    return generation;
}

bool IsControlConnectionActiveLocked(TAdcConnectionId connectionId)
{
    return ControlConnections.find(connectionId) != ControlConnections.end();
}

bool IsDataConnectionActiveLocked(const std::shared_ptr<TAdcDataConnection> &connection)
{
    if (!connection || !connection->Connected.load())
        return false;

    const auto found = DataConnections.find(connection->Id);
    return found != DataConnections.end() && found->second == connection;
}

bool ShouldStop(const std::shared_ptr<TAdcWorkerContext> &context)
{
    return done != 0 || context->StopRequested.load();
}

bool SocketWouldBlock(int error)
{
    if (error == EAGAIN)
        return true;
#if EWOULDBLOCK != EAGAIN
    if (error == EWOULDBLOCK)
        return true;
#endif
    return false;
}

void CancelAdcWait()
{
    if (DaqReady() && apci >= 0)
        apci_cancel_irq(apci, 1);
}

void RequestStopLocked(std::uint64_t generation, const char *reason)
{
    if (Stream.Generation != generation || !Stream.Context)
        return;

    if (!Stream.Context->StopRequested.exchange(true))
        Debug("ADC stream stop requested: " + std::string(reason ? reason : "unspecified"));

    if (Stream.Context->SyncWakeupsEnabled.load())
    {
        sem_post(&Stream.Context->Empty);
        sem_post(&Stream.Context->Full);
    }
}

void SetWorkerInitResult(const std::shared_ptr<TAdcWorkerContext> &context, int status)
{
    std::lock_guard<std::mutex> lock(AdcStateMutex);
    if (Stream.Generation != context->Generation || Stream.Context != context)
        return;

    Stream.WorkerInitialized = true;
    Stream.WorkerInitStatus = status;
    AdcStateChanged.notify_all();
}

void DisableSyncWakeups(const std::shared_ptr<TAdcWorkerContext> &context)
{
    std::lock_guard<std::mutex> lock(AdcStateMutex);
    if (Stream.Generation == context->Generation && Stream.Context == context)
        context->SyncWakeupsEnabled.store(false);
}

void WorkerExited(const std::shared_ptr<TAdcWorkerContext> &context, int initStatus)
{
    std::lock_guard<std::mutex> lock(AdcStateMutex);
    if (Stream.Generation != context->Generation || Stream.Context != context)
        return;

    if (!Stream.WorkerInitialized)
    {
        Stream.WorkerInitialized = true;
        Stream.WorkerInitStatus = initStatus;
    }

    Stream.Active = false;
    AdcStateChanged.notify_all();
}

int WaitForSemaphore(sem_t *semaphore)
{
    timespec deadline{};
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0)
        return -errno;

    deadline.tv_nsec += SEM_WAIT_NS;
    if (deadline.tv_nsec >= 1000000000L)
    {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }

    for (;;)
    {
        if (sem_timedwait(semaphore, &deadline) == 0)
            return 1;
        if (errno == EINTR)
            continue;
        if (errno == ETIMEDOUT)
            return 0;
        return -errno;
    }
}

int SendAdcBlockInterruptible(const std::shared_ptr<TAdcWorkerContext> &context, const void *data, std::size_t bytes)
{
    const auto *source = static_cast<const unsigned char *>(data);
    std::size_t sentTotal = 0;

    while (sentTotal < bytes)
    {
        if (ShouldStop(context))
            return -ECANCELED;

        const ssize_t sent = send(context->DataConnection->Socket, source + sentTotal, bytes - sentTotal, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent > 0)
        {
            sentTotal += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent == 0)
            return -ECONNRESET;
        if (errno == EINTR)
            continue;
        if (!SocketWouldBlock(errno))
            return -errno;

        pollfd descriptor{};
        descriptor.fd = context->DataConnection->Socket;
        descriptor.events = POLLOUT;
#ifdef POLLRDHUP
        descriptor.events |= POLLRDHUP;
#endif

        int pollStatus = 0;
        do
        {
            pollStatus = poll(&descriptor, 1, 100);
        }
        while (pollStatus < 0 && errno == EINTR && !ShouldStop(context));

        if (pollStatus < 0)
            return -errno;
        if (pollStatus == 0)
            continue;

        short disconnectEvents = POLLERR | POLLHUP | POLLNVAL;
#ifdef POLLRDHUP
        disconnectEvents |= POLLRDHUP;
#endif
        if ((descriptor.revents & disconnectEvents) != 0)
            return -ECONNRESET;
    }

    return 0;
}

void MarkDataConnectionFailedFromStream(const std::shared_ptr<TAdcWorkerContext> &context, int savedErrno)
{
    const auto connection = context->DataConnection;
    if (!connection)
        return;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        connection->Connected.store(false);

        const auto found = DataConnections.find(connection->Id);
        if (found != DataConnections.end() && found->second == connection)
            DataConnections.erase(found);

        RequestStopLocked(context->Generation, "ADC data socket send failed");
    }

    if (connection->Socket >= 0)
        shutdown(connection->Socket, SHUT_RDWR);

    CancelAdcWait();
    Error("ADC data socket send failed on connection " + std::to_string(connection->Id) + ", errno=" + std::to_string(savedErrno) + ", " + std::strerror(savedErrno));
}

void RequestStopFromWorker(const std::shared_ptr<TAdcWorkerContext> &context, const char *reason)
{
    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        RequestStopLocked(context->Generation, reason);
    }
    CancelAdcWait();
}

void *log_main(void *arg)
{
    std::unique_ptr<std::shared_ptr<TAdcWorkerContext>> contextHolder(static_cast<std::shared_ptr<TAdcWorkerContext> *>(arg));
    const std::shared_ptr<TAdcWorkerContext> context = *contextHolder;
    const std::shared_ptr<TAdcDataConnection> connection = context->DataConnection;
    int ringReadIndex = 0;

    Debug("ADC log thread started for data connection " + std::to_string(connection ? connection->Id : ADC_INVALID_CONNECTION_ID));

    while (!ShouldStop(context))
    {
        const int waitStatus = WaitForSemaphore(&context->Full);
        if (waitStatus == 0)
            continue;
        if (waitStatus < 0)
        {
            Error("ADC logger sem_timedwait failed: " + std::to_string(-waitStatus) + ", " + std::strerror(-waitStatus));
            RequestStopFromWorker(context, "ADC logger semaphore failure");
            break;
        }
        if (ShouldStop(context))
            break;

        const std::size_t bytesExpected = sizeof(std::uint32_t) * SAMPLES_PER_TRANSFER;
        const int sendStatus = SendAdcBlockInterruptible(context, ring_buffer[ringReadIndex], bytesExpected);
        if (sendStatus != 0)
        {
            if (sendStatus != -ECANCELED)
                MarkDataConnectionFailedFromStream(context, -sendStatus);
            break;
        }

        sem_post(&context->Empty);
        Trace("Sent ADC data " + std::to_string(bytesExpected) + " bytes on data connection " + std::to_string(connection->Id));

        ++ringReadIndex;
        ringReadIndex %= RING_BUFFER_SLOTS;
    }

    Debug("ADC log thread ended");
    return nullptr;
}

int InitializeWorker(const std::shared_ptr<TAdcWorkerContext> &context)
{
    if (sem_init(&context->Empty, 0, RING_BUFFER_SLOTS) != 0)
        return -errno;
    context->EmptyInitialized = true;

    if (sem_init(&context->Full, 0, 0) != 0)
        return -errno;
    context->FullInitialized = true;

    context->MmapAddress = mmap(nullptr, DMA_BUFF_SIZE, PROT_READ, MAP_SHARED, apci, 0);
    if (context->MmapAddress == MAP_FAILED)
        return -errno;

    auto *loggerArgument = new std::shared_ptr<TAdcWorkerContext>(context);
    const int threadStatus = pthread_create(&context->LoggerThread, nullptr, &log_main, loggerArgument);
    if (threadStatus != 0)
    {
        delete loggerArgument;
        return -threadStatus;
    }

    context->LoggerStarted = true;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        if (Stream.Generation == context->Generation && Stream.Context == context)
            context->SyncWakeupsEnabled.store(true);
    }

    return 0;
}

void CleanupWorker(const std::shared_ptr<TAdcWorkerContext> &context)
{
    context->StopRequested.store(true);

    if (context->LoggerStarted)
    {
        if (context->FullInitialized)
            sem_post(&context->Full);
        pthread_join(context->LoggerThread, nullptr);
        context->LoggerStarted = false;
    }

    DisableSyncWakeups(context);

    if (context->MmapAddress != MAP_FAILED)
    {
        if (munmap(context->MmapAddress, DMA_BUFF_SIZE) != 0)
            Error("ADC DMA munmap failed: " + std::string(std::strerror(errno)));
        context->MmapAddress = MAP_FAILED;
    }

    if (context->FullInitialized)
    {
        sem_destroy(&context->Full);
        context->FullInitialized = false;
    }

    if (context->EmptyInitialized)
    {
        sem_destroy(&context->Empty);
        context->EmptyInitialized = false;
    }
}

void *worker_main(void *arg)
{
    std::unique_ptr<std::shared_ptr<TAdcWorkerContext>> contextHolder(static_cast<std::shared_ptr<TAdcWorkerContext> *>(arg));
    const std::shared_ptr<TAdcWorkerContext> context = *contextHolder;

    Debug("ADC worker thread started");

    if (!DaqReady() || apci < 0)
    {
        Error("ADC worker refused to start because DAQ hardware is unavailable");
        SetWorkerInitResult(context, -ENODEV);
        WorkerExited(context, -ENODEV);
        return reinterpret_cast<void *>(static_cast<intptr_t>(-ENODEV));
    }

    const int initStatus = InitializeWorker(context);
    SetWorkerInitResult(context, initStatus);

    if (initStatus != 0)
    {
        Error("ADC worker initialization failed: " + std::to_string(-initStatus) + ", " + std::strerror(-initStatus));
        CleanupWorker(context);
        WorkerExited(context, initStatus);
        return reinterpret_cast<void *>(static_cast<intptr_t>(initStatus));
    }

    while (!ShouldStop(context) && !context->AcquisitionStarted.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    int finalStatus = 0;
    int ringWriteIndex = 0;

    while (!ShouldStop(context))
    {
        int firstSlot = 0;
        int numSlots = 0;
        int dataDiscarded = 0;
        const int readyStatus = apci_dma_data_ready(apci, 1, &firstSlot, &numSlots, &dataDiscarded);

        if (readyStatus != 0 || dataDiscarded != 0)
        {
            Error("ADC DMA data-ready failure: first_slot=" + std::to_string(firstSlot) + ", num_slots=" + std::to_string(numSlots) + ", data_discarded=" + std::to_string(dataDiscarded) + ", status=" + std::to_string(readyStatus));
            if (readyStatus != 0)
            {
                finalStatus = readyStatus;
                RequestStopFromWorker(context, "ADC DMA data-ready failure");
                break;
            }
        }

        if (numSlots == 0)
        {
            const int waitStatus = apci_wait_for_irq(apci, 1);
            if (waitStatus != 0)
            {
                const int savedErrno = errno;
                if (savedErrno == ECANCELED && ShouldStop(context))
                    Trace("ADC IRQ wait canceled during stream shutdown");
                else
                {
                    finalStatus = savedErrno != 0 ? -savedErrno : -EIO;
                    Error("ADC worker IRQ wait failed: " + std::to_string(savedErrno) + ", " + std::strerror(savedErrno));
                    RequestStopFromWorker(context, "ADC IRQ wait failure");
                }
                break;
            }
            continue;
        }

        Trace("Taking ADC data block(s)");
        for (int index = 0; index < numSlots && !ShouldStop(context); ++index)
        {
            int emptyStatus = 0;
            do
            {
                emptyStatus = WaitForSemaphore(&context->Empty);
            }
            while (emptyStatus == 0 && !ShouldStop(context));

            if (emptyStatus < 0)
            {
                finalStatus = emptyStatus;
                Error("ADC worker sem_timedwait failed: " + std::to_string(-emptyStatus) + ", " + std::strerror(-emptyStatus));
                RequestStopFromWorker(context, "ADC worker semaphore failure");
                break;
            }
            if (ShouldStop(context))
                break;

            const int dmaSlot = (firstSlot + index) % RING_BUFFER_SLOTS;
            std::memcpy(ring_buffer[ringWriteIndex], static_cast<const __u8 *>(context->MmapAddress) + BYTES_PER_TRANSFER * dmaSlot, BYTES_PER_TRANSFER);
            sem_post(&context->Full);
            ringWriteIndex = (ringWriteIndex + 1) % RING_BUFFER_SLOTS;
            apci_dma_data_done(apci, 1, 1);
        }
    }

    if (DaqReady() && apci >= 0)
        apci_write8(apci, 1, BAR_REGISTER, ofsAdcTriggerOptions, 0);

    CleanupWorker(context);
    WorkerExited(context, finalStatus);

    Debug("ADC worker thread ended");
    return reinterpret_cast<void *>(static_cast<intptr_t>(finalStatus));
}

int StopStreamGeneration(std::uint64_t expectedGeneration, TAdcConnectionId requester, bool requireOwner, bool shutdownDataSocket, const char *reason)
{
    pthread_t workerThread{};
    bool joinWorker = false;
    std::shared_ptr<TAdcDataConnection> dataConnection;

    for (;;)
    {
        std::unique_lock<std::mutex> lock(AdcStateMutex);

        if (Stream.Generation != expectedGeneration)
            return 0;

        if (requireOwner && Stream.Active && Stream.ControlConnectionId != requester)
            return -EPERM;

        RequestStopLocked(expectedGeneration, reason);
        dataConnection = Stream.DataConnection;

        if (shutdownDataSocket && dataConnection)
        {
            dataConnection->Connected.store(false);
            const auto found = DataConnections.find(dataConnection->Id);
            if (found != DataConnections.end() && found->second == dataConnection)
                DataConnections.erase(found);
        }

        if (!Stream.WorkerJoinable)
        {
            Stream.Active = false;
            Stream.Generation = 0;
            Stream.ControlConnectionId = ADC_INVALID_CONNECTION_ID;
            Stream.DataConnection.reset();
            Stream.Context.reset();
            Stream.WorkerInitialized = false;
            Stream.WorkerInitStatus = 0;
            AdcStateChanged.notify_all();
            lock.unlock();

            if (shutdownDataSocket && dataConnection && dataConnection->Socket >= 0)
                shutdown(dataConnection->Socket, SHUT_RDWR);
            CancelAdcWait();
            return 0;
        }

        if (Stream.JoinInProgress)
        {
            AdcStateChanged.wait(lock, [expectedGeneration]() { return Stream.Generation != expectedGeneration || !Stream.JoinInProgress; });
            if (Stream.Generation != expectedGeneration)
                return 0;
            continue;
        }

        Stream.JoinInProgress = true;
        workerThread = Stream.WorkerThread;
        joinWorker = true;
        lock.unlock();
        break;
    }

    if (shutdownDataSocket && dataConnection && dataConnection->Socket >= 0)
        shutdown(dataConnection->Socket, SHUT_RDWR);

    CancelAdcWait();

    int joinStatus = 0;
    if (joinWorker)
        joinStatus = pthread_join(workerThread, nullptr);

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        if (Stream.Generation == expectedGeneration)
        {
            Stream.Active = false;
            Stream.Generation = 0;
            Stream.WorkerJoinable = false;
            Stream.JoinInProgress = false;
            Stream.ControlConnectionId = ADC_INVALID_CONNECTION_ID;
            Stream.DataConnection.reset();
            Stream.Context.reset();
            Stream.WorkerInitialized = false;
            Stream.WorkerInitStatus = 0;
        }
        AdcStateChanged.notify_all();
    }

    if (joinStatus != 0)
    {
        Error("pthread_join(ADC worker) failed: " + std::to_string(joinStatus) + ", " + std::strerror(joinStatus));
        return -joinStatus;
    }

    return 0;
}

int ReapPreviousWorkerIfNeeded()
{
    std::uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        if (!Stream.WorkerJoinable || Stream.Active)
            return 0;
        generation = Stream.Generation;
    }

    return StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, false, "reaping completed ADC worker");
}
}

TAdcConnectionId AdcRegisterControlConnection(int socket)
{
    std::lock_guard<std::mutex> lock(AdcStateMutex);
    const TAdcConnectionId connectionId = AllocateConnectionIdLocked();
    if (connectionId != ADC_INVALID_CONNECTION_ID)
        ControlConnections.emplace(connectionId, socket);
    return connectionId;
}

bool AdcIsControlConnectionActive(TAdcConnectionId connectionId, int socket)
{
    std::lock_guard<std::mutex> lock(AdcStateMutex);
    const auto found = ControlConnections.find(connectionId);
    return found != ControlConnections.end() && found->second == socket;
}

void AdcControlConnectionClosed(TAdcConnectionId connectionId)
{
    std::uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        ControlConnections.erase(connectionId);
        if (Stream.WorkerJoinable && Stream.ControlConnectionId == connectionId)
            generation = Stream.Generation;
    }

    if (generation != 0)
        StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, true, "control connection closed");
}

TAdcConnectionId AdcRegisterDataConnection(int socket)
{
    std::lock_guard<std::mutex> lock(AdcStateMutex);
    const TAdcConnectionId connectionId = AllocateConnectionIdLocked();
    if (connectionId == ADC_INVALID_CONNECTION_ID)
        return connectionId;

    auto connection = std::make_shared<TAdcDataConnection>();
    connection->Id = connectionId;
    connection->Socket = socket;
    DataConnections.emplace(connectionId, connection);
    return connectionId;
}

void AdcDataConnectionClosed(TAdcConnectionId connectionId, int socket)
{
    std::uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        const auto found = DataConnections.find(connectionId);
        if (found != DataConnections.end() && found->second->Socket == socket)
        {
            found->second->Connected.store(false);
            DataConnections.erase(found);
        }

        if (Stream.WorkerJoinable && Stream.DataConnection && Stream.DataConnection->Id == connectionId && Stream.DataConnection->Socket == socket)
        {
            Stream.DataConnection->Connected.store(false);
            generation = Stream.Generation;
        }
    }

    if (socket >= 0)
        shutdown(socket, SHUT_RDWR);

    if (generation != 0)
        StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, false, "ADC data connection closed");
}

int AdcStartStream(TAdcConnectionId controlConnectionId, TAdcConnectionId dataConnectionId)
{
    if (!DaqReady() || apci < 0)
        return -ENODEV;

    for (;;)
    {
        const int reapStatus = ReapPreviousWorkerIfNeeded();
        if (reapStatus != 0)
            return reapStatus;

        std::uint64_t staleGeneration = 0;
        bool shutdownStaleDataSocket = false;
        std::shared_ptr<TAdcDataConnection> dataConnection;

        {
            std::lock_guard<std::mutex> lock(AdcStateMutex);

            if (!IsControlConnectionActiveLocked(controlConnectionId))
                return -ENOTCONN;

            const auto dataFound = DataConnections.find(dataConnectionId);
            if (dataFound == DataConnections.end() || !IsDataConnectionActiveLocked(dataFound->second))
                return -ENOTCONN;
            dataConnection = dataFound->second;

            if (Stream.Active)
            {
                const bool ownerStillConnected = IsControlConnectionActiveLocked(Stream.ControlConnectionId);
                const bool dataStillConnected = IsDataConnectionActiveLocked(Stream.DataConnection);
                if (ownerStillConnected && dataStillConnected)
                    return -EBUSY;

                staleGeneration = Stream.Generation;
                shutdownStaleDataSocket = !ownerStillConnected;
            }
        }

        if (staleGeneration != 0)
        {
            StopStreamGeneration(staleGeneration, ADC_INVALID_CONNECTION_ID, false, shutdownStaleDataSocket, "reclaiming stale ADC stream owner");
            continue;
        }

        const int transferStatus = apciDmaTransferSize(RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
        if (transferStatus != 0)
        {
            Error("Error setting ADC DMA transfer size: " + std::to_string(transferStatus));
            return transferStatus < 0 ? transferStatus : -EIO;
        }

        std::shared_ptr<TAdcWorkerContext> context;
        std::uint64_t generation = 0;

        {
            std::lock_guard<std::mutex> lock(AdcStateMutex);

            if (!IsControlConnectionActiveLocked(controlConnectionId))
                return -ENOTCONN;

            const auto dataFound = DataConnections.find(dataConnectionId);
            if (dataFound == DataConnections.end() || dataFound->second != dataConnection || !IsDataConnectionActiveLocked(dataConnection))
                return -ENOTCONN;

            if (Stream.Active || Stream.WorkerJoinable)
                continue;

            generation = AllocateStreamGenerationLocked();
            context = std::make_shared<TAdcWorkerContext>();
            context->Generation = generation;
            context->DataConnection = dataConnection;

            Stream.Generation = generation;
            Stream.ControlConnectionId = controlConnectionId;
            Stream.DataConnection = dataConnection;
            Stream.Context = context;
            Stream.Active = true;
            Stream.WorkerInitialized = false;
            Stream.WorkerInitStatus = 0;
            Stream.WorkerJoinable = false;
            Stream.JoinInProgress = false;

            auto *workerArgument = new std::shared_ptr<TAdcWorkerContext>(context);
            const int threadStatus = pthread_create(&Stream.WorkerThread, nullptr, &worker_main, workerArgument);
            if (threadStatus != 0)
            {
                delete workerArgument;
                Stream.Active = false;
                Stream.Generation = 0;
                Stream.ControlConnectionId = ADC_INVALID_CONNECTION_ID;
                Stream.DataConnection.reset();
                Stream.Context.reset();
                Error("ADC worker pthread_create failed: " + std::to_string(threadStatus) + ", " + std::strerror(threadStatus));
                return -threadStatus;
            }

            Stream.WorkerJoinable = true;
        }

        std::unique_lock<std::mutex> lock(AdcStateMutex);
        const bool initialized = AdcStateChanged.wait_for(lock, WORKER_INIT_TIMEOUT, [generation]() { return Stream.Generation != generation || Stream.WorkerInitialized; });

        if (!initialized)
        {
            lock.unlock();
            StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, false, "ADC worker initialization timed out");
            return -ETIMEDOUT;
        }

        if (Stream.Generation != generation)
            return -ECANCELED;

        const int initStatus = Stream.WorkerInitStatus;
        if (initStatus != 0)
        {
            lock.unlock();
            StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, false, "ADC worker initialization failed");
            return initStatus;
        }

        if (!Stream.Active || !IsControlConnectionActiveLocked(controlConnectionId) || !IsDataConnectionActiveLocked(dataConnection))
        {
            lock.unlock();
            StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, true, "ADC connection disappeared during stream startup");
            return -ECANCELED;
        }

        apciDmaStart();
        context->AcquisitionStarted.store(true);
        lock.unlock();

        Debug("ADC stream started: control=" + std::to_string(controlConnectionId) + ", data=" + std::to_string(dataConnectionId));
        return 0;
    }
}

int AdcStopStream(TAdcConnectionId controlConnectionId)
{
    std::uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        if (!Stream.Active && !Stream.WorkerJoinable)
            return 0;

        if (Stream.Active && Stream.ControlConnectionId != controlConnectionId)
            return -EPERM;

        generation = Stream.Generation;
    }

    const int status = StopStreamGeneration(generation, controlConnectionId, true, false, "ADC_StreamStop");
    if (status == 0)
        Debug("ADC stream stopped");
    return status;
}

void AdcShutdown()
{
    std::vector<int> dataSockets;
    std::uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(AdcStateMutex);
        ControlConnections.clear();

        dataSockets.reserve(DataConnections.size());
        for (auto &entry : DataConnections)
        {
            entry.second->Connected.store(false);
            if (entry.second->Socket >= 0)
                dataSockets.push_back(entry.second->Socket);
        }
        DataConnections.clear();

        if (Stream.WorkerJoinable)
            generation = Stream.Generation;
    }

    for (const int socket : dataSockets)
        shutdown(socket, SHUT_RDWR);

    if (generation != 0)
        StopStreamGeneration(generation, ADC_INVALID_CONNECTION_ID, false, true, "daemon shutdown");
    else
        CancelAdcWait();
}
