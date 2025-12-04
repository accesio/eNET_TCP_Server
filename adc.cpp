#include <arpa/inet.h> //MSG_NOSIGNAL
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>

// #include "safe_queue.h"
#include "logging.h"
#include "TMessage.h"
#include "TError.h"
#include "eNET-AIO16-16F.h"
#include "apcilib.h"
#include "adc.h"
extern volatile sig_atomic_t done;
static uint32_t ring_buffer[RING_BUFFER_SLOTS][SAMPLES_PER_TRANSFER];

volatile bool AdcStreamTerminate;

pthread_t worker_thread;
pthread_t AdcLogger_thread;

pthread_mutex_t mutex;
sem_t empty;
sem_t full;

timespec AdcLogTimeout;

int AdcStreamingConnection = -1;

int AdcLoggerThreadID = -1;
int AdcWorkerThreadID = -1;

bool AdcLoggerTerminate = false;

void *log_main(void *arg)
{
	AdcLoggerTerminate = false;
	Debug("ADC log_main Thread started");
	AdcLogTimeout.tv_sec = 1;
	int conn = *(int *)arg;
	int ring_read_index = 0;

	while (!AdcLoggerTerminate)
	{
		clock_gettime(CLOCK_REALTIME, &AdcLogTimeout);
		AdcLogTimeout.tv_sec++;
		if (-1 == sem_timedwait(&full, &AdcLogTimeout)) // TODO: check for other Errors
		{
			if (errno == ETIMEDOUT)
				continue;
			Error("Unexpected error from sem_timedwait(), errno: " + std::to_string(errno) + ", " + strerror(errno));
			break;
		}

		pthread_mutex_lock(&mutex);
		ssize_t sent = send(conn, ring_buffer[ring_read_index], (sizeof(uint32_t) * SAMPLES_PER_TRANSFER), MSG_NOSIGNAL);
		pthread_mutex_unlock(&mutex);

		if (sent < 0 && errno == EPIPE)
		{
			AdcStreamTerminate = true;
			apci_cancel_irq(apci, 1);
			AdcLoggerTerminate = true;
			continue;
		}

		sem_post(&empty);
		Trace("Sent ADC Data " + std::to_string(sent) + " bytes, on ConnectionID: " + std::to_string(conn));

		ring_read_index++;
		ring_read_index %= RING_BUFFER_SLOTS;
	};

	Debug("ADC log_main Thread ended");
	return 0;
}

void *worker_main(void *arg)
{
	done = 0;
	Debug("ADC worker_main Thread started");
	int *conn_fd = (int *)arg;
	int num_slots, first_slot, data_discarded, status = 0;
	bool logger_started = false;

	status = sem_init(&empty, 0, 255);
	status |= sem_init(&full, 0, 0);
	status |= pthread_mutex_init(&mutex, NULL);
	if (status)
	{
		Error("Mutex failed");
		return (void *)(size_t)status;
	}

	void *mmap_addr = mmap(NULL, DMA_BUFF_SIZE, PROT_READ, MAP_SHARED, apci, 0);
	if (mmap_addr == MAP_FAILED)
	{
		Error("mmap failed");
		return (void *)-1;
	}
	try
	{
		if (AdcLoggerThreadID == -1)
		{
			AdcLoggerTerminate = false;
			logger_started = true;
			Trace("No Logger Thread Found: Starting Logger thread.");

			int rc2 = pthread_create(&AdcLogger_thread, NULL, &log_main, conn_fd);
			if (rc2 != 0) {
				Error("Failed to start logger thread: " +
					std::to_string(rc2) + ", " + strerror(rc2));
				logger_started = false;
			} else {
				AdcLoggerThreadID = 0; // “running”
			}
		}

		while (done == 0)
		{
			status = apci_dma_data_ready(apci, 1, &first_slot, &num_slots, &data_discarded);
			if ((data_discarded != 0) || status)
			{
				Error("first_slot: " + std::to_string(first_slot) + "num_slots:" +
					  std::to_string(num_slots) + "+data_discarded:" + std::to_string(data_discarded) + "; status: " + std::to_string(status));
			}

			if (num_slots == 0) // Worker Thread: No data pending; Waiting for IRQ
			{
				// Log("no data yet, blocking");
				status = apci_wait_for_irq(apci, 1); // thread blocking
				if (status)
				{
					status = errno;
					if (status != ECANCELED) // "canceled" is not an error but we do want to close this thread
						Error("  Worker Thread: Error waiting for IRQ; status: " + std::to_string(status) + ", " + strerror(status));
					else
						Trace("  Thread canceled.");

					AdcStreamTerminate = true;
					AdcLoggerTerminate = true;
					break;
				}
				continue;
			}
			Trace("Taking ADC Data block(s)");
			for (int i = 0; i < num_slots; i++)
			{
				sem_wait(&empty);
				pthread_mutex_lock(&mutex);
				memcpy(ring_buffer[(first_slot + i) % RING_BUFFER_SLOTS], ((__u8 *)mmap_addr + (BYTES_PER_TRANSFER * ((first_slot + i) % RING_BUFFER_SLOTS))),
					   BYTES_PER_TRANSFER);
				pthread_mutex_unlock(&mutex);
				sem_post(&full);
				apci_dma_data_done(apci, 1, 1);
			}
		}
		Debug("ADC worker_main Thread ended");
	}
	catch (const std::logic_error &e)
	{
		Error(e.what());
	}
	Debug("Setting AdcStreamingConnection to idle");
	apci_write8(apci, 1, BAR_REGISTER, 0x12, 0); // turn off ADC start modes

	AdcLoggerTerminate = 1;
	sem_post(&full); // wake logger if it’s waiting

	if (logger_started) {
		pthread_join(AdcLogger_thread, NULL);
		logger_started = false;
		AdcLoggerThreadID = -1;
	}

	pthread_mutex_destroy(&mutex);
	sem_destroy(&full);
	sem_destroy(&empty);

	AdcStreamingConnection = -1;
	AdcWorkerThreadID = -1;

	Debug("ADC Log Thread exiting.");
	return 0;
}