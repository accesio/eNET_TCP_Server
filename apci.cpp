#include "apci.h"
#include "apcilib.h"
#include <iostream>
#include "eNET-AIO16-16F.h"


__u8 in8(int offset)
{
	__u8 value = 0;
	int status = apci_read8(apci, 0, BAR_REGISTER, offset, &value);
	std::cout << "in8("+to_hex<__u8>(offset)+"); Width= "+std::to_string(widthFromOffset(offset)) << " status = " << std::to_string(status) << " Value = " << to_hex<__u8>(value) <<'\n';
	return status ? -1 : value;
}

__u16 in16(int offset)
{
	__u16 value = 0;
	int status = apci_read16(apci, 0, BAR_REGISTER, offset, &value);
	std::cout << "in16(" + to_hex<__u8>(offset) + "); Width= " + std::to_string(widthFromOffset(offset)) << " status = " << std::to_string(status) << " Value = " << to_hex<__u16>(value) << '\n';
	return status ? -1 : value;
}

__u32 in32(int offset)
{
	__u32 value = 0;
	int status = apci_read32(apci, 0, BAR_REGISTER, offset, &value);
	std::string one = "in32(" + to_hex<__u8>(offset) + "); Width= ";
	std::string two = std::to_string(widthFromOffset(offset));
	std::string three = " status = ";
	std::string four = std::to_string(status);
	std::string five = " Value = ";
	std::string six = to_hex<__u32>(value);
	std::cout << one << two << three << four << five << six << '\n';
	return status ? -1 : value;
}

__u32 in(int offset)
{
	switch (widthFromOffset(offset))
	{
	case 8:
		return in8(offset);
	case 16:
		return in16(offset);
	case 32:
		return in32(offset);
	default:
		return -1;
		break;
	}
}

TError out8(int offset, __u8 value)
{
	int status = apci_write8(apci, 0, BAR_REGISTER, offset, value);
	std::cout << "out8(" + to_hex<__u8>(offset) + ", "<< to_hex<__u8>(value) <<"); Width = " + std::to_string(widthFromOffset(offset)) << " status = " << std::to_string(status) << '\n';
	return status;
}

TError out16(int offset, __u16 value)
{
	int status = apci_write16(apci, 0, BAR_REGISTER, offset, value);
	std::cout << "out16(" + to_hex<__u8>(offset) + ", "<< to_hex<__u16>(value) <<"); Width = " + std::to_string(widthFromOffset(offset)) << " status = " << std::to_string(status) << '\n';
	return status;
}

TError out32(int offset, __u32 value)
{
	int status = apci_write32(apci, 0, BAR_REGISTER, offset, value);
	std::cout << "out32(" + to_hex<__u8>(offset) + ", "<< to_hex<__u32>(value) <<"); Width = " + std::to_string(widthFromOffset(offset)) << " status = " << std::to_string(status) << '\n';
	return status;
}

TError out(int offset, __u32 value)
{
	switch (widthFromOffset(offset))
	{
	case 8:
		return out8(offset, value);
	case 16:
		return out16(offset, value);
	case 32:
		return out32(offset, value);
	default:
		return -1;
		break;
	}
}

int apciGetDevices() { return apci_get_devices(apci); }

int apciGetDeviceInfo(unsigned int *deviceID, unsigned long bars[6]) { return apci_get_device_info(apci, 0, deviceID, bars); }

int apciWaitForIRQ() { return apci_wait_for_irq(apci, 0); }

int apciCancelWaitForIRQ() { return apci_cancel_irq(apci, 0); }

int apciDmaTransferSize(__u8 slots, size_t size) { return apci_dma_transfer_size(apci, 0, slots, size); }

int apciDmaDataReady(int *start_index, int *slots, int *data_discarded) { return apci_dma_data_ready(apci, 0, start_index, slots, data_discarded); }

int apciDmaDataDone(int slots) { return apci_dma_data_done(apci, 0, slots); }

int apciDmaStart(){return apci_start_dma (apci);}

// int apci_writebuf8(int fd, unsigned long device_index, int bar, int bar_offset, unsigned int mmap_offset, int length);
// int apci_writebuf16(int fd, unsigned long device_index, int bar, int bar_offset, unsigned int mmap_offset, int length);
// int apci_writebuf32(int fd, unsigned long device_index, int bar, int bar_offset, unsigned int mmap_offset, int length);

// int apci_dac_buffer_size (int fd, unsigned long size);
