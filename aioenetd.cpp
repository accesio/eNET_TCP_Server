// aioenetd.cpp
// Main source file for aioenetd, the systemd service/application
//  that listens to some TCP ports and provides the packet-level interface to eNET- devices

/*
	when a connection to ~8080 (the "control" connection port) occurs send a Hello 'H' TMessage with several TDataItems.
		At least one data item is the ConnectionID, others may include PID, Model#, Serial#, what the onboard RTC thinks the time is, etc
		DId 0x7001 "TCP_ConnectionID", 4 byte ConnectionID is the data
	When a connection to ~8080+1 (the ADC Streaming port) occurs, send (__u32)(0x80000000|ConnectionID) ("Invalid ADC data value bit set + ConnectionID")

	ADC_StreamStart(ConnectionID) uses ConnectionID as the connection to stream ADC data on.
*/

/*[aioenetd Protocol 2 TCP-Listener/Server Daemon/Service implementation and concept notes]
from discord code-review conversation with Daria; these do not belong in this source file:
	your "the main loop" == "my worker thread that dequeues Actions";
	your "exploding" == "my Object factory construction";
	You've moved "my Object construction" into a single-threaded spot, "the main loop", from where I have it, in an
	individual socket's receive-thread.
	Because I have multiple receive threads it isn't nearly as necessary to "be fast": the TCP Stack will queue bytes for me.
	Because my error-checking location (the parser, the exploder, .FromBytes()) is in a socket-specific (ie client-specific)
	thread it is harde\
	{			  \
		;		  \
	}
	Because my worker and all receive threads share a thread-safe std::queue<> everything is serialized nicely.

	By putting 66% of the error checking, syntax AND semantic (but not operational errors, eg hardware timeouts) into the
	receive threads — and in fact, into the TMessage constructor, I am guaranteed all TMessage Objects are valid and safe
	to submit to the worker thread, thus less likely to cause errors in that single-thread / single-point-of-failure
	(and make the worker execution faster, as it is "my single thread" and thus bottleneck)
-----
	A TMessage is constructed from received bytes by `auto aMessage = TMessage::fromBytes(buf);`, or an "X" Response TMessage with
	syntax error details gets returned, instead.
	Either way, the constructed TMessage is pushed into the Action Queue.
	The asynchronous worker thread pops a TMessage off the Action Queue, does a `for(aDataItem : aMessage.Payload){aDataItem.Go()}`,
	and either modifies-in-place and sends the TMessage as a reply or constructs and sends a new TMessage as the reply,
	the TMessage(s) then goes out of scope and deallocates
-----
	The TMessage library needs to handle syntax errors, mistakes in the *format* of a bytestream, and semantic errors, mistakes
	in the *content* of the received bytes.  Some categoriese of semantic errors, however, are specific to a particular model of eNET-
	device, let alone specific to a model Family.

	Consider ADC_GetChannelV(iChannel): iChannel is valid if (0 <= iChannel <= 15), right?  Nope, not "generally": this is only true
	for the ~12 models in the base Family, eNET-AIO16-16F, eNET-AI12-16E, etc.  But eNET- devices, and therefore Protocol 2 devices
	intended to operate via this TMessage library and the aioenetd implementation, include the DPK and DPK M Families; this means:
	iChannel is valid if (0 <= iChannel <= highestAdcChan), and highestAdcChan is 15, 31, 63, 96, or 127; depending on the specific
	model running aioenetd/using this library.

	So, to catch Semantic errors of this type (invalid channel parameter in an ADC_GetChannelV() TDataItem) the parser must "know"
	the value of "highestAdcChan" for the model it is running on.

	The sum of all things that the parser needs to know to handle semantic error checking, specific to the model running the library,
	are encapsulated as "getters" in a HAL.  The list is quite long as there are a LOT of variations built out of the eNET-AIO design.

	These "getters" *could* be implemented in a static, compile-time, manner.  Consider a device_specific.h file that has a bunch of
	eg `#define highestAdcChan 31`-type constants defined.  However, this is a "write it for 1" approach, and would require loading a
	different TMessage library binary for every model, as the binary is effectively hard-coded for a specific device's needs.

	The USB FWE2.0 firmwares are almost this simplistic in their approach to a HAL: there is a device_specific.h, but there is also a
	run-time operation that introspects the DeviceID and tweaks some constants, like the friendly_model_name string, to a model-specific
	value.  This run-time operation is a hard-coded switch(DeviceId){}, and thus is implemented per Firmware (but allows one Firmware
	to run, as a binary, on any models built from the same or compatible PCB).

	This only works because there are very few variants per firmware source: the USB designs are sufficiently different that reusing
	firmware is implausible: it would require a *real* HAL implementation (one that abstracted every pin on the FX2, and every
	register that could ever exist on the external address/data bus).

	We're going for a better approach to the HAL in this library.

	TBD, LOL

	The library implementation, today (2022-07-21 @ 10:55am Pacific), only checks for Semantic errors in the one DId that's been
	implemented; REG_Read1(offset).  This DId uses a helper function hardcoded in the source called `WidthFromOffset(offset)`,
	which returns one of 0, 8, or 32, indicating "invalid offset", "offset is a valid 8-bit register", or "offset is a valid
	32-bit register", respectively.  In theory it should be able to respond "16" as well, but the eNET-AIO, in all of its models,
	has no 16-bit registers.  This is an instance where the HAL is weak; "WidthFromOffset()" shouldn't be hard-coded; it should
	be able to respond accurately about whatever device it is running on (or perhaps even whatever device it is asked about).

	One way to accomplish this would be loading configuration information tables from nonvolatile on-device storage.  "tables",
	here, is plural not to refer to both a hypothetical single table needed for WidthFromOffset() and all the other tables for
	supporting other HAL-queriables, but to express that WidthFromOffset() *alone* needs several tables, if it is to support
	the general case.  Sure, eNET-AIO only has registers that can each only be correctly accessed at a specific bit width (ie it is
	unsafe or impossible to successfully read or write 8 bits from any eNET-AIO 32-bit register), but many ACCES devices do not
	have this limitation; most devices support 8, 16, or 32-bit access to any register or group thereof (as long as offsets are
	width-aligned; eg 32-bit operations require that offset % 4 == 0).  WidthFromOffset() therefore becomes complex, and
	declaratively describing each device's capabilities and restrictions is also complex.

	Another approach would be to implement a "HAL interface" (C++ calls an interface an Abstract Base Class or ABC) which the library
	would use to access the needed polymorphisms via a device-specific TDeviceHAL object it is provided ("Dependency Injection").
	Every TDeviceHAL descendant would provide not-less-than a set of device-specific constants like `highestAdcChan` from the
	introduction example.  Better would be to also include "verbs" that implement generic operations as needed for the specific
	device, like an ADC_SetRange1(iChannel, rangeSpan), but this level of interface is incredibly complex, in any generic form.

	Consider the RA1216 which expects the ADC input range to be specified as a voltage span code plus an offset in ±16-bit counts;

	This is an extreme example, an outlier; an example that forces "the most general case" to be handled ... but this is merely the
	outlier in the "ADC RangeCode" axis.  Consider the RAD242 which has a 24-bit ADC, the AD8-16 which has an 8-bit ADC, the
	USB-DIO-32I which has 1 bit per I/O Group instead of the typical 8 or 4 bits — there are *many* axes of outliers, and they all
	become necessary to handle if you try to make a truly generic library/HAL interface.

	This is why "Universal Libraries" (like NI produces) are so big and difficult to code against.

	Thus, "Protocol 2" is designed to be slightly generic, but more importantly, extensible.  Devices running old Protocol libraries
	should interoperate safely, politely, with Messages sent using new, extended, versions of the protocol.

	[
		during the writing of this I've determined the extensibility I'd designed into the protocol was insufficient to support an
		already known "requirement" for extending TDataItem lengths beyond 127 bytes, and thinking about it I realized using the
		most significant bit of a Length field as a sentinel to indicate an alternate syntax, or even one of a set, applies, does
		not provide the ability to change the LENGTH field, unless that is defined in advance at day 1.

		As a result of discussing this with Daria we've decided to just double the existing max payload lengths, in both TMessages
		and TDataItems (i.e., moving from 16 to 32 bits, and from 8 to 16 bits, respectively).  The delta overhead from this change
		limits at 25%, and *bandwidth* isn't a concern (exceeding MTU-size multiples, thus increasing TCP packet count, is of some
		concern).

		No future version of this protocol will support longer length Messages.  Instead, if an eNET- device *needs* longer
		Messages or DataItems, or needs either in *unspecified* lengths, clients will connect to a different listen_port and use a different
		protocol.  This is how Protocol 1 supports "ADC Streaming".
	]

[program overview]
	Main spawns one action-thread for handling Protocol 2.
	NOTE: Main might also spawn a singleton receive/action/send thread, or one set per "Streaming" type (ADC in the eNET-AIO case), to handle the streaming Protocol(s).
	Each Client that connects spawns a receive-thread.

	EITHER
	1	the action-thread is responsible for sending data to the correct client
	OR
	2	each per-client receive-thread spawns a send-thread and queue which is stuffed from the action-thread
	OR
	3 	a single send-thread and queue exist, created by Main and stuffed by the action-thread

	Root listen in Main run-loop receives on primary connect listen_port#; valid connections spawn receive-threads that listen on the Socket
	Multiple Clients can connect; each gets one listen-thread (and perhaps one send-queue & thread).

	[receive-threads]
		Each receive-thread passes received bytes in >= Message-sized chunks to TMessage::fromBytes to construct a TMessage instance; .fromBytes is a
		class factory method that will construct the appropriate TDataItem descendants based on the TMessage.Payload bytes' DIds.
			errors throw exceptions; the exception handler will programmatically construct a TMessage to report the detected Errors
		Regardless, the receive-thread Queues the TMessage (either the one constructed via .fromBytes() or via the caught exception handler)
			into the action-thread's Queue to be handled, and resumes waiting for additional messages.
			The TMessage that was Queued is now owned by the Action Queue;
			In the case of an error the TMessage that was received goes out of scope and is destroyed when the receive-thread run-loop loops.
	[action-thread]
		The action-thread run-loop pops a TMessage "ActionBundle" out of the Action Queue
		If the Message is a report-error message it puts it into the send-queue and the run-loop loops.
			NOTE: TMessages in the Action Queue are guaranteed to be "syntactically and semantically valid".
		Normally it constructs a basic Reply TMessage. and proceeds.
		It loops over the ActionBundle.TDataItems[] and calls .Go() on each.
			Each .Go() performs its Action, using the derived-classes' DId-specific parameters as needed, and changes its internal state into a "resultDataItem"
				Basically: once resultCode has been set (from .Go() the TDataItem .AsBytes() and .AsString() functions produce different results than before .Go(),
				as they now include the resultCode and resultValue(s) as determined during .Go().  E.g., DIO_Read1() adds 1 or 0 to indicate the input level.
			The send-thread then checks the .getResultCode(); if not ERR_SUCCESS it sets the Reply Message MId to reflect the operational error, 'E'.
			Regardless, the send-thread then adds the modified TDataItem into the Reply Message. NOTE: TDataItems in TPayloads are actually shared_ptr<TDataItem>, so the
			being-assembled Reply Message AND the ActionBundle's Message both hold a reference to the TDataItem at this point.
		Once all TDataItems[] in the ActionBundle.Payload have been executed (.Go()), and stuffed into the Reply Message, .AsBytes() is called and the byte-buffer
		produced is added to a SendBundle, which is then:
		EITHER
		1	sent to the correct Client across the socket, directly from the Action thread.
			NOTE: this means ActionBundles need to hold a reference to the Client's Socket.
		OR
		2	added to a per-Client send-thread Queue to be sent asynchronously by the per-Client send-thread.
			NOTE: this means ActionBundles need a reference to the Client's send-Queue.
		OR
		3	added to a single send-thread Queue to be sent asynchronously.
			NOTE: this means ActionBundles AND SendBundles need a reference to the Client's Socket (the SendBundle just inherits it from the ActionBundle)
		TBD: once I know more about TCP Sockets and such in Linux I can figure out which is best.

		The send-thread run-loop has now completed one loop, so the TMessage it popped out of the Action Queue goes out of scope and is destroyed.
	[1. no send-thread exists separate from the action-thread]
	[2. one send-thread per Client]
		When Main gets a Connection it spawns a listen-thread, which constructs a Send Queue then spawns a send-thread that will operate on the Send Queue.
	[3. singleton send-thread]
		Main constructs both the action-thread and send-thread; only one of each exist, each with one input Queue

	The single Action Thread serves to serialize device operations, ensuring the Actions dictated in each received Message's payload get executed "atomically",
	BUT the execution order is determined by the "parsing-finished" time, not by the "Message-received" time; i.e., each Client gets its turn in the order the
	receive-threads' constructed TMessage gets added to the Action Queue.

	The Main run-loop should act as a Watchdog, monitoring the various threads to ensure they haven't dead-locked.

	All threads (including Main) should generate log file data, with programmatically configured verbosity level;

	Logs should be retrievable via Protocol 2 Messages. (NYI)
 */

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <cstdlib>

#include "apci.h"
#include "logging.h"
#include "TMessage.h"
#include "adc.h"
#include "config.h"
#include "DataItems/ADC_.h"
#include "DataItems/BRD_.h"
#include "DataItems/CFG_.h"
#include "DataItems/DAC_.h"
#include "DataItems/REG_.h"
#include "DataItems/TDataItem.h"
#include "aioenetd.h"
// #define MG_ARCH MG_ARCH_NEWLIB
// extern "C" {
// #include "mongoose.h"
// }

#define VersionString "0.7.5"

int apci = -1;
volatile sig_atomic_t done = 0;

static int ControlListenPort = 18767; // 0x494f, ASCII for "IO"

int AdcListenPort = ControlListenPort + 1;

pthread_t action_thread;
pthread_t controlListener_thread;
pthread_t adcListener_thread;

// // Function to serve static files
// static void serve_static(struct mg_connection *nc, struct mg_http_message *hm) {
//     struct mg_http_serve_opts opts = { .root_dir = "/home/acces/www" };
//     mg_http_serve_dir(nc, hm, &opts);
// }

// // Function to handle API requests
// static void handle_api(struct mg_connection *nc, struct mg_http_message *hm) {
// 	if (mg_match(hm->uri, mg_str("/api/data*"), NULL) )
// 	{
// 		// Example response with dynamic data
//         mg_http_reply(nc, 200, "Content-Type: application/json\r\n", "{\"adc\": [1.23, 2.34, 3.45]}");
// 	}
// 	else
// 	{
// 		mg_http_reply(nc, 404, "Content-Type: text/plain\r\n", "Not Found!");
// 	}
// }

// // Event handler for Mongoose
// static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
//     struct mg_http_message *hm = (struct mg_http_message *) ev_data;

//     switch (ev) {
//         case MG_EV_HTTP_MSG:
// 		    Log(std::string(hm->uri.buf, hm->uri.len));
//             if (mg_match(hm->uri, mg_str("/api/*"), NULL) ) {
//                 handle_api(nc, hm);
//             } else {
//                 serve_static(nc, hm);
//             }
//             break;
//         default:
//             break;
//     }
// }

int main(int argc, char *argv[])
{
	Intro(argc, argv);

	InitConfig(Config);
	InitializeConfigFiles(Config);

	try
	{
		LoadConfig();
	}
	catch (const std::logic_error &e)
	{
		Error(e.what());
	};
	ApplyConfig();
	OpenDevFile(); // sets apci

	pthread_create(&action_thread, NULL, (void *(*)(void *)) & ActionThread, &ActionQueue);
	pthread_create(&controlListener_thread, NULL, ControlListenerThread, (void *)AF_INET6);
	pthread_create(&adcListener_thread, NULL, AdcListenerThread, (void *)AF_INET6);

	// struct mg_mgr mgr;
	// struct mg_connection *nc;

	// mg_mgr_init(&mgr);
	// nc = mg_http_listen(&mgr, "http://0.0.0.0:80", ev_handler, &mgr);

	// if (nc == NULL) {
	//     printf("Failed to create listener\n");
	//     return 1;
	// }

	// printf("Starting server on port 80\n");

	do
	{
		usleep(10000);
		// mg_mgr_poll(&mgr, 1000);
	} while (!done);

	// mg_mgr_free(&mgr);

	// TODO:  if (bReboot) syscall("reboot"); // for isp-fpga and upgrader

	exit_handler(0);
	return 0;
}

void abort_handler(int s)
{
	done = 1;
}

void exit_handler(int s)
{
	Log("exit process starting");
	done = 1;
	apci_cancel_irq(apci, 1); // unblocks apci_wait_for_irq in worker

	// if (controlSocket >= 0)
	// {
	// 	shutdown(controlSocket, SHUT_RDWR);
	// 	close(controlSocket);
	// 	controlSocket = -1;
	// }
	// if (adcSocket >= 0)
	// {
	// 	shutdown(adcSocket, SHUT_RDWR);
	// 	close(adcSocket);
	// 	adcSocket = -1;
	// }
	ActionQueue.enqueue(nullptr);

	sleep(1);

	pthread_join(adcListener_thread, NULL);
	pthread_join(controlListener_thread, NULL);
	pthread_join(action_thread, NULL);

	/* put the card back in the power-up state */
	out(ofsReset, bmResetEverything);
	close(apci);
	SaveConfig();

	// note __attribute__((unused)) is to silence an incorrect compiler warning
	std::time_t end_time __attribute__((unused)) = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	Log(std::string("AIOeNET Daemon " VersionString " CLOSING, it is now: ") + std::string(std::ctime(&end_time)));
}

void Intro(int argc, char **argv)
{
	// note __attribute__((unused)) is to silence an incorrect compiler warning
	const char *env = std::getenv("AIOENET_LOG_LEVEL");
	if (env)
	{
		std::string lvl = env;
		Log("AIOENET_LOG_LEVEL='" + lvl + "'");
		std::transform(lvl.begin(), lvl.end(), lvl.begin(), ::tolower);
		SetLogLevel(LogLevel::Error);
		if (lvl == "trace")
			SetLogLevel(LogLevel::Trace);
		else if (lvl == "debug")
			SetLogLevel(LogLevel::Debug);
		else if (lvl == "info")
			SetLogLevel(LogLevel::Info);
		else if (lvl == "warning" || lvl == "warn")
			SetLogLevel(LogLevel::Warning);
		else if (lvl == "error")
			SetLogLevel(LogLevel::Error);
	} else
	{
		SetLogLevel(LogLevel::Info);
		Log("AIOENET_LOG_LEVEL not set, defaulting to 'debInfoug'");
	}

	std::time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	Log("AIOeNET Daemon " VersionString " STARTING, it is now: " + std::string(std::ctime(&start_time)));

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = abort_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGABRT, &sigIntHandler, NULL);
	sigaction(SIGTERM, &sigIntHandler, NULL);

	if (argc < 2)
	{
		Trace("Warning: no tcp port specified.  Using default: " + std::to_string(ControlListenPort));
		Trace("Usage: " + std::string(argv[0]) + " {port_to_listen — (i.e., 18767)}");
	}
	else
		sscanf(argv[1], "%d", &ControlListenPort);

	Trace("Control port: " + std::to_string(ControlListenPort));
}

void OpenDevFile()
{
	std::string devicefile = "";
	std::string devicepath = "/dev/apci";
	for (const auto &devfile : std::filesystem::directory_iterator(devicepath))
	{
		apci = open(devfile.path().c_str(), O_RDONLY);
		if (apci >= 0)
		{
			devicefile = devfile.path().c_str();
			break;
		}
	}
	Log("Opening device @ " + devicefile);
}

void Bind(int &Socket, int &Port, void *structaddr, int iNET)
{
	struct sockaddr_in *addr4 = (sockaddr_in *)structaddr;
	struct sockaddr_in6 *addr6 = (sockaddr_in6 *)structaddr;
	int result = -1;

	if ((Socket = socket(iNET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	int opt = 1;
	if (setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
	{
		Error("setsockopt failed");
		perror("setsockopt failed");
		exit(EXIT_FAILURE);
	}
	int yes = 1;
	setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	if (iNET == AF_INET)
	{
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(static_cast<short>(Port));
		addr4->sin_addr.s_addr = INADDR_ANY;
		result = bind(Socket, (struct sockaddr *)addr4, sizeof(sockaddr_in));
	}
	else // if (iNET == AF_INET6)
	{
		memset(addr6, 0, sizeof(*addr6));
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons(static_cast<short>(Port));
		addr6->sin6_flowinfo = 0; // 0x607ACCE5;
		addr6->sin6_addr = IN6ADDR_ANY_INIT;
		addr6->sin6_scope_id = 0;
		// addr6->sin6_scope_id = 0x0e;
		result = bind(Socket, (struct sockaddr *)addr6, sizeof(sockaddr_in6));
	}

	if (result < 0)
	{
		Error("Bind on port " + std::to_string(Port) + " failed");
		exit(EXIT_FAILURE);
	}
	// if (Port == ControlListenPort)
	// 	controlSocket = Socket;
	// if (Port == AdcListenPort)
	// 	adcSocket = Socket;
}

void Listen(int &Socket, int num)
{
	if (listen(Socket, num) < 0) // 32 connections; soft-cap or hard-cap?
	{
		Error("listen(ControlSocket) failed");
		perror("listen(ControlSocket) failed");
		exit(EXIT_FAILURE);
	}
}

void *ControlListenerThread(void *arg)
{
	__s64 iNET = (__s64)arg;
	struct sockaddr_storage ControlAddr;
	int ControlSocket;
	socklen_t ControlAddrSize;
	std::vector<int> ControlClients;
	ControlAddrSize = sizeof(sockaddr_storage);
	memset(&ControlAddr, 0, ControlAddrSize);

	if (iNET == AF_INET6)
		Bind(ControlSocket, ControlListenPort, &ControlAddr, AF_INET6);
	else
		Bind(ControlSocket, ControlListenPort, &ControlAddr, AF_INET);

	Trace("Listen for Control Socket");
	Listen(ControlSocket, 32);
	for (; done == 0;)
		HandleNewControlClients(ControlSocket, ControlAddrSize, ControlAddr);
	ControlClients.clear();
	return nullptr;
}

void SendAdcHello(int Socket)
{
	__u32 HelloAdc = Socket | 0x80000000; // "invalid ADC bit, and connection ID"
	ssize_t bytesSent = send(Socket, &HelloAdc, 4, MSG_NOSIGNAL);
	if (bytesSent == -1)
	{
		Error("! TCP Send of ADC Hello appears to have failed, bytesSent != Message Length (" + std::to_string(bytesSent) + " != " + std::to_string(sizeof(HelloAdc)) + ")");
		// handle xmit error
	}
	else
	{
		Log("sent 'Hello' to new ADC Client# " + to_hex<__u32>(Socket) + ", (ORed with 0x80000000) [" + to_hex<__u32>(HelloAdc) + "]");
	}
}

void HandleNewAdcClients(int Socket, socklen_t addrSize, struct sockaddr_storage &addr)
{
	int new_socket;
	Trace("accept ADC");

	while (!done)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(Socket, &readfds);

		// select() needs the highest fd + 1
		int nfds = Socket + 1;

		// Set a 1-second timeout (adjust to taste)
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		int ret = select(nfds, &readfds, nullptr, nullptr, &tv);
		if (ret < 0)
		{
			if (errno == EINTR)
			{
				// Interrupted by signal—check if we should exit
				if (done)
					break;
				continue; // otherwise just keep going
			}
			// Some real error
			perror("select() failed");
			break;
		}
		else if (ret == 0)
		{
			// Timeout expired, no sockets ready
			if (done)
				break; // see if we should exit
			// else just continue polling
			continue;
		}

		// If we get here, ret > 0, meaning 'listenSock' is readable
		// => accept() should not block
		if (FD_ISSET(Socket, &readfds)) // There's at least one pending connection
		{
			// There's at least one pending connection
			struct sockaddr_storage socka;
			socklen_t sockl = sizeof(socka);
			if ((new_socket = accept(Socket, (struct sockaddr *)&socka, &addrSize)) < 0)
			{
				Error("accept failed");
				perror("accept failed");
				exit(EXIT_FAILURE);
			}
			SendAdcHello(new_socket);
		}
	}
}

void *AdcListenerThread(void *arg)
{
	int iNET = static_cast<int>((__s64)arg);
	struct sockaddr_storage AdcAddr;
	int AdcSocket;
	socklen_t AdcAddrSize;
	std::vector<int> AdcClients;
	AdcAddrSize = sizeof(sockaddr_storage);

	Bind(AdcSocket, AdcListenPort, &AdcAddr, iNET);

	Listen(AdcSocket, 1);
	for (; done == 0;)
		HandleNewAdcClients(AdcSocket, AdcAddrSize, AdcAddr);
	AdcClients.clear();
	return nullptr;
}

void SendControlHello(int Socket)
{
	TMessageId MId_Hello = 'H';
	TPayload Payload;
	TBytes data{};
	TBytes bytes{};
	TError result;
	for (uint byt = 0; byt < sizeof(Socket); byt++)
		data.push_back(static_cast<__u8>((Socket >> (8 * byt)) & 0x000000FF));
	stuff<__u16>(bytes, static_cast<__u16>(DataItemIds::TCP_ConnectionID));
	stuff<__u16>(bytes, (__u16)data.size());
	bytes.insert(bytes.end(), data.begin(), data.end());

	PTDataItem d2 = TDataItemBase::fromBytes(bytes, result);
	Payload.push_back(d2);
	Log("DID[TCP_ConnectionID] = ", d2->AsBytes(true));

	//__u32 dacRangeDefault = 0x3031E142; "B±10"
	//__u32 dacRangeDefault = 0x35303055;
	for (__u8 channel = 0; channel < Config.NUM_DACs; channel++)
	{
		data.clear();
		data.push_back(channel);
		for (uint byt = 0; byt < sizeof(Config.dacRanges[channel]); byt++)
			data.push_back(static_cast<__u8>((Config.dacRanges[channel] >> (8 * byt)) & 0x000000FF));
		bytes.clear();
		stuff<__u16>(bytes, static_cast<__u16>(DataItemIds::DAC_Range1));
		bytes.push_back(5);
		bytes.push_back(0);
		bytes.insert(bytes.end(), data.begin(), data.end());
		d2 = TDataItemBase::fromBytes(bytes, result);
		Payload.push_back(d2);
	}

	PTDataItem features = std::unique_ptr<TBRD_Features>(new TBRD_Features());
	PTDataItem deviceID = std::unique_ptr<TBRD_DeviceID>(new TBRD_DeviceID());
	PTDataItem adcBaseClock = std::unique_ptr<TADC_BaseClock>(new TADC_BaseClock());
	PTDataItem fpgaId = std::unique_ptr<TBRD_FpgaId>(new TBRD_FpgaId());
	try
	{
		features->Go();
		Payload.push_back(features);
		deviceID->Go();
		Payload.push_back(deviceID);
		adcBaseClock->Go();
		Payload.push_back(adcBaseClock);
		fpgaId->Go();
		Payload.push_back(fpgaId);
	}
	catch (const std::logic_error &e)
	{
		Error(e.what());
		perror(e.what());
	}

	TMessage HelloControl = TMessage(MId_Hello, Payload);

	TBytes rbuf = HelloControl.AsBytes(true);
	ssize_t bytesSent = send(Socket, rbuf.data(), rbuf.size(), MSG_NOSIGNAL);
	if (bytesSent == -1)
	{
		Error("! TCP Send of Control Hello appears to have failed, bytesSent != Message Length (" + std::to_string(bytesSent) + " != " + std::to_string(rbuf.size()) + ")");
		// handle xmit error
	}
	else
	{
		Log("Sent 'Hello' to Control Client# " + to_hex<__u32>(Socket) + ":\n		  " + HelloControl.AsString() + ", bytes=", rbuf);
	}
}

void Disconnect(int aClient)
{
	struct sockaddr_storage addr; // Can hold IPv4 or IPv6
	socklen_t addrSize = sizeof(addr);

	if (getpeername(aClient, reinterpret_cast<struct sockaddr *>(&addr), &addrSize) == -1)
		Error("getpeername() failed");
	else
	{
		char ipStr[INET6_ADDRSTRLEN] = {0}; // Enough space for IPv6 text
		uint16_t port = 0;

		if (addr.ss_family == AF_INET) // IPv4
		{
			auto *v4 = reinterpret_cast<struct sockaddr_in *>(&addr);
			inet_ntop(AF_INET, &(v4->sin_addr), ipStr, sizeof(ipStr));
			port = ntohs(v4->sin_port);
		}
		else if (addr.ss_family == AF_INET6) // IPv6
		{
			auto *v6 = reinterpret_cast<struct sockaddr_in6 *>(&addr);
			inet_ntop(AF_INET6, &(v6->sin6_addr), ipStr, sizeof(ipStr));
			port = ntohs(v6->sin6_port);
		}
		else
		{
			// Some other address family?
			strncpy(ipStr, "UnknownAF", sizeof(ipStr));
			port = 0;
		}

		Log("Host " + to_hex<__u32>(aClient) + " disconnected; IP: " + ipStr + ", Port " + std::to_string(port));
	}
	close(aClient);

	// // Remove from list
	// auto it = std::find(ClientList.begin(), ClientList.end(), aClient);
	// if (it != ClientList.end())
	//     ClientList.erase(it);
	// else
	//     Error("INTERNAL ERROR: Attempted to remove socket not in ClientList");
}

bool GotMessage(char theBuffer[], int bytesRead, TMessage &parsedMessage)
{
	TError result;
	TBytes buf(theBuffer, theBuffer + bytesRead);

	parsedMessage = TMessage::FromBytes(buf, result);
	Log("Received " + std::to_string(bytesRead) + " bytes on Control connection:");
	if (result != ERR_SUCCESS)
	{
		Error("TMessage::fromBytes(buf) returned " + std::to_string(-result) + ": " + err_msg[-result]);
		return false;
	}
	Log("Received " + std::to_string(bytesRead) + " bytes on Control connection:\n		  " + parsedMessage.AsString());
	return true;
}

// new version of threadReceiver written by ChatGPT to accumulate and chunk incoming packets
ssize_t ReceiveFromSocket(int aSocket, std::vector<char> &buffer)
{
	char tempBuffer[65536];
	ssize_t bytesRead = recv(aSocket, tempBuffer, sizeof(tempBuffer), MSG_NOSIGNAL);
	if (bytesRead > 0)
	{
		buffer.insert(buffer.end(), tempBuffer, tempBuffer + bytesRead);
	}
	return bytesRead;
}

int CheckDisconnect(ssize_t bytesRead, int aSocket)
{
	if (bytesRead == 0)
		return true;
	return false;
}

void ProcessMessage(std::vector<char> &buffer, int aSocket)
{
	static __u32 expectedMessageLength = 0;

	if (expectedMessageLength == 0 && buffer.size() >= 5)
	{
		expectedMessageLength = *reinterpret_cast<__u32 *>(&buffer[1]);
		expectedMessageLength = le32toh(expectedMessageLength);
		Debug("-- Expected Length: " + std::to_string(expectedMessageLength));
	}

	if (buffer.size() >= expectedMessageLength + 5 + 1)
	{
		TMessage *aMessage = new TMessage;
		if (GotMessage(buffer.data(), expectedMessageLength + 5 + 1, *aMessage))
		{
			TActionQueueItem *action = new TActionQueueItem{aSocket, *aMessage};
			ActionQueue.enqueue(action);
			Debug(action->theMessage.AsString(true));
		}
		buffer.erase(buffer.begin(), buffer.begin() + expectedMessageLength + 5 + 1);
		expectedMessageLength = 0;
	}
}

// J2H: In progress
void *threadReceiver(void *arg)
{
	int controlSocket = static_cast<int>((__u64)arg);
	struct sockaddr_storage addr; // Can hold IPv4 or IPv6
	socklen_t addrSize = sizeof(addr);

	if (getpeername(controlSocket, reinterpret_cast<struct sockaddr *>(&addr), &addrSize) == -1)
		Error("getpeername() failed");
	else
	{
		char ipStr[INET6_ADDRSTRLEN] = {0}; // Enough space for IPv6 text
		uint16_t port = 0;

		if (addr.ss_family == AF_INET) // IPv4
		{
			auto *v4 = reinterpret_cast<struct sockaddr_in *>(&addr);
			inet_ntop(AF_INET, &(v4->sin_addr), ipStr, sizeof(ipStr));
			port = ntohs(v4->sin_port);
		}
		else if (addr.ss_family == AF_INET6) // IPv6
		{
			auto *v6 = reinterpret_cast<struct sockaddr_in6 *>(&addr);
			inet_ntop(AF_INET6, &(v6->sin6_addr), ipStr, sizeof(ipStr));
			port = ntohs(v6->sin6_port);
		}
		else
		{
			// Some other address family?
			strncpy(ipStr, "UnknownAF", sizeof(ipStr));
			port = 0;
		}
		Log("New Control connection thread, socket fd is: " + to_hex<__u32>(controlSocket) + " IP: " + ipStr + ", Port " + std::to_string(port));
	}
	SendControlHello(controlSocket);

	std::vector<char> buffer;

	while (done == 0)
	{
		ssize_t bytesRead = ReceiveFromSocket(controlSocket, buffer);
		if (bytesRead < 0)
		{
			Error("error on Control recv(): " + std::to_string(errno));
			break; // error handling? frex "connection closed" or EAGAIN or EINTR?
		}
		if (CheckDisconnect(bytesRead, controlSocket))
		{
			Disconnect(controlSocket);
			break;
		}

		try
		{
			Debug("control receiver got " + std::to_string(bytesRead) + " bytes");
			ProcessMessage(buffer, controlSocket);
		}
		catch (const std::logic_error &e)
		{
			Error(e.what());
		}
	};
	Log("Closing threadReceiver for connection " + to_hex<__u32>(controlSocket));
	Disconnect(controlSocket);
	return nullptr;
}

void HandleNewControlClients(int ControlListenSocket, socklen_t addrSize, sockaddr_storage &addr)
{
	int new_socket;
	Trace("Accept for Control");

	while (!done)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(ControlListenSocket, &readfds);

		// select() needs the highest fd + 1
		int nfds = ControlListenSocket + 1;

		// Set a 1-second timeout (adjust to taste)
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		int ret = select(nfds, &readfds, nullptr, nullptr, &tv);
		if (ret < 0)
		{
			if (errno == EINTR)
			{
				if (done)
					break;
				continue; // otherwise just keep going
			}
			perror("select() failed");
			break;
		}
		else if (ret == 0)
		{
			// Timeout expired, no sockets ready
			if (done)
				break; // see if we should exit
			// else just continue polling
			continue;
		}

		// If we get here, ret > 0, meaning 'listenSock' is readable
		// => accept() should not block
		if (FD_ISSET(ControlListenSocket, &readfds))
		{
			sockaddr_storage socka;
			socklen_t sockl = sizeof(socka);
			if ((new_socket = accept(ControlListenSocket, (struct sockaddr *)&socka, &addrSize)) < 0)
			{
				Error("accept failed");
				perror("accept failed");
				continue;
			}
			pthread_t receive_thread;
			Log("New Control connection, socket fd is: " + to_hex<__u32>(new_socket));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
			pthread_create(&receive_thread, NULL, &threadReceiver, (void *)new_socket); // spawn Control Read thread here, pass in new_socket
			pthread_detach(receive_thread);
#pragma GCC diagnostic pop
		}
	}
	// ReceiverThreadQueue.enqueue(receive_thread);
}

bool RunMessage(TMessage &aMessage)
{
	Trace("Executing Message DataItems[].Go(), " + std::to_string(aMessage.DataItems.size()) + " total DataItems");
	try
	{
		for (auto anItem : aMessage.DataItems)
		{
			// Debug("About to execute " + anItem->AsString(true)+": ", anItem->AsBytes(true));
			anItem->Go();
			// if an error happens what do we do? This is a "run-time" error
		}
		aMessage.setMId('R'); // FIX: should be performed based on anItem.getResultCode() indicating no errors
	}
	catch (const std::logic_error &e)
	{
		aMessage.setMId('X');
		Error("EXCEPTION! " + std::string(e.what()));
		Log("Error Message built: \n		  " + aMessage.AsString(true));
		return false;
	}
	Log("Control Reply Message built: \n		  " + aMessage.AsString(true));
	return true;
}

void SendResponse(int Client, TMessage &aMessage)
{
	TBytes rbuf = aMessage.AsBytes(true);									  // valgrind
	ssize_t bytesSent = send(Client, rbuf.data(), rbuf.size(), MSG_NOSIGNAL); // valgrind
	if (bytesSent == -1)
	{
		Error("! TCP Send of Reply to Control failed, bytesSent != Message Length (" + std::to_string(bytesSent) + " != " + std::to_string(rbuf.size()) + ")");
		// handle xmit error
	}
	else
	{
		Trace("sent Reply to Control Client# " + std::to_string(Client) + " " + std::to_string(bytesSent) + " bytes: ", rbuf);
	}
}

void *ActionThread(TActionQueue *Q)
{
	for (; done == 0;)
	{
		TActionQueueItem *anAction = Q->dequeue();
		if (!anAction)
			continue; // should only occur if done != 0, "poison pill"

		Log("---DEQUEUED---");
		RunMessage(anAction->theMessage);
		SendResponse(anAction->Socket, anAction->theMessage); // move to send threads?
															  // free(anAction);
	}
	return nullptr;
}

// //------------------- Signal---------------------------
// #define max_BRK_attempts 3
// static void sig_handler(int sig)
// {
// 	static int sig_count = 1;
// 	Log("signal " + std::to_string(sig) + " detected " + std::to_string(sig_count++) + " time; exiting");
// 	done = true;
// 	if (sig_count > max_BRK_attempts)
// 	{
// 		Error("main runloop's `done` flag not managing to exit, Terminating!");
// 		exit(1);
// 	}
// }
// //---------------------End signal -----------------------
