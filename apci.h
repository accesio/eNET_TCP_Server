#pragma once

#include <unistd.h>
#include "apcilib.h"
#include "utilities.h"
#include "eNET-AIO16-16F.h"
extern int apci;

__u32 in(int offset);
TError out(int offset, __u32 value);


int apciGetDevices();
int apciGetDeviceInfo(unsigned int *deviceID, unsigned long bars[6]);
int apciWaitForIRQ();
int apciCancelWaitForIRQ();
int apciDmaTransferSize(__u8 slots, size_t size);
int apciDmaDataReady(int *start_index, int *slots, int *data_discarded);
int apciDmaDataDone(int slots);
int apciDmaStart();