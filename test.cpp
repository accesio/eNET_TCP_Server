
#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <regex>
#include <signal.h>
#include <string>
#include <unistd.h>

#include "logging.h"
#include "eNET-types.h"
#include "TMessage.h"
#include "config.h"
#include "DataItems/ADC_.h"
#include "DataItems/BRD_.h"
#include "DataItems/CFG_.h"
#include "DataItems/DAC_.h"
#include "DataItems/REG_.h"
#include "DataItems/TDataItem.h"

using namespace std;
void SendControlHello();
int apci;

bool RunMessage(TMessage &aMessage)
{
	Trace("Executing Message DataItems[].Go(), " + std::to_string(aMessage.DataItems.size()) + " total DataItems");
	try
	{
		for (auto anItem : aMessage.DataItems)
		{
			Debug(anItem->AsString(true), anItem->AsBytes(true));
			anItem->Go();
		}
		aMessage.setMId('R'); // FIX: should be performed based on anItem.getResultCode() indicating no errors
	}
	catch (std::logic_error e)
	{
		aMessage.setMId('X');
		Error("EXCEPTION! " + std::string(e.what()));
		Log("Error Message built: \n		  " + aMessage.AsString(true));
		return false;
	}
	Log("Control Reply Message built: \n		  " + aMessage.AsString(true));
	return true;
}


// Received: 4D 48 00 00 00 10 02 05 00 00 00 00 80 3F 0E 02 05 00 00 00 00 00 00 10 02 05 00 01 00 00 80 3F 0E 02 05 00 01 00 00 00 00 10 02 05 00 02 00 00 80 3F 0E 02 05 00 02 00 00 00 00 10 02 05 00 03 00 00 80 3F 0E 02 05 00 03 00 00 00 00 B3
int main(void) // "TEST"
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

    try
    {
        cout << endl << "0 ----------------------------------------------------------------------------------------------" << endl<< endl;
        TBytes msg = {0x4D, 0x48, 0x00, 0x00, 0x00, 0x10, 0x02, 0x05, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0E, 0x02, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x05, 0x00, 0x01, 0x00, 0x00, 0x80, 0x3F, 0x0E, 0x02, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x05, 0x00, 0x02, 0x00, 0x00, 0x80, 0x3F, 0x0E, 0x02, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x05, 0x00, 0x03, 0x00, 0x00, 0x80, 0x3F, 0x0E, 0x02, 0x05, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0xB3};

        TError result;
        TMessage *aMessage = new TMessage;
        *aMessage = TMessage::FromBytes(msg, result);

        if (result == ERR_SUCCESS)
            Debug("GotMessage says valid");

        RunMessage(*aMessage);


        if (result != ERR_SUCCESS)
        {
            Error("TMessage::fromBytes(buf) returned " + std::to_string(result) + err_msg[-result]);
            return false;
        }
        Log("Received on Control connection:\n		  " + aMessage->AsString());
        cout << endl << "1 ----------------------------------------------------------------------------------------------" << endl<< endl;
        SendControlHello();

        TMessage Message = TMessage('M');

        cout << endl << "2 ----------------------------------------------------------------------------------------------" << endl<< endl;

        TMessage Msg = TMessage(msg);
        cout << endl << "3 ----------------------------------------------------------------------------------------------" << endl<< endl;
        Msg.Go();
        cout << endl << "4 ----------------------------------------------------------------------------------------------" << endl<< endl;
        cout << Msg.AsString(true) << endl << "-----------------------------------------------" << endl;
        cout << endl << "5 ----------------------------------------------------------------------------------------------" << endl<< endl;
        printBytes(cout, "Msg = ", Msg.AsBytes(true), true);
        cout << "6 ----------------------------------------------------------------------------------------------" << endl<< endl;

        // TBytes REG_Read3C = {0x4D, 0x05, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x3C, 0x6F };
        // TMessage Msg2 = TMessage(REG_Read3C);
        // cout << Msg2.AsString(true) << endl << "-----------------------------------------------" << endl;

        // Debug("bytes: ",Msg2.AsBytes(true));


        // __u8 dacNum = 1;
        // float scale = 1.0;
        // float offset = 0.0;
        // __u16 len;
        // printf("Testing\n-----------------------------------\n");
        // TMessageId MId_C = 'C';
        // TMessage Message = TMessage(MId_C);
        // PTDataItem item;
        // TBytes bytes;
        // bytes.push_back(0x0C);
        // bytes.push_back(0x02);
        // bytes.push_back(0x09);
        // bytes.push_back(0x00);
        // bytes.push_back(0x01); // DAC# 0
        // stuff<float>(bytes, 1.0); // scale
        // stuff<float>(bytes, -1.0); // offset
        // Trace("bytes == ",bytes);
        // TError res;
        // item = TDataItem::fromBytes(bytes, res);
        // item->setDId(DAC_Calibrate1);

        // // std::shared_ptr<TConfigField<__u8, float, float>>
        // //     item2(new TConfigField<__u8, float, float>(dacNum, scale, offset));
        // // Message.addDataItem(item2);

        // Message.addDataItem(item);
        // // printf("Item pointer is %p", item->Data.data());

        // cout << Message.AsString() << endl;
        // Debug("bytes: ", Message.AsBytes());

        // printf("\nTESTING\n\n");

        // //test(Message.AsBytes(), "TMessage(TDAC_Calibrate1(0, 1.0, 0.0))", ERR_SUCCESS);
        // Config.dacScaleCoefficients[1] = 9.1;
        // Config.dacOffsetCoefficients[1] = -66.66;
        // std::cout << "before " << Config.dacScaleCoefficients[1] << ", " << Config.dacOffsetCoefficients[1] << std::endl;
        // item->Go();
        // std::cout << "after " << Config.dacScaleCoefficients[1] << ", " << Config.dacOffsetCoefficients[1]<<std::endl;


    }
    catch (logic_error e)
    {
        cout << endl
             << "!! Exception caught: " << e.what() << endl;
    }

    printf("\nDone.\n");
    return 0;
}






TError test(TBytes Msg, const char *TestDescription, TError expectedResult = ERR_SUCCESS)
{

    printf("TEST %s", TestDescription);
    if (expectedResult != ERR_SUCCESS)
        printf(" (testing error code %d, %s), ", expectedResult, err_msg[-expectedResult]);
    printf("\n");
    TError result;
    try
    {
        result = TMessage::validateMessage(Msg);
        if (result != expectedResult)
            printf("FAIL: result (%d, %s) is not expected result (%d, %s)\n",
                   result, err_msg[-result], expectedResult, err_msg[-expectedResult]);
        else
            printf("PASS: result is as expected [%d, %s]\n", result, err_msg[-result]);
    }
    catch (logic_error e)
    {
        cout << endl
             << "Exception caught: " << e.what() << endl
             << "if the exception is " << expectedResult << " then this is PASS" << endl;
    }
    printf("----------------------\n");
    return result;
}

extern "C" {
    void __cyg_profile_func_enter(void* func, void* callsite) __attribute__((no_instrument_function));
    void __cyg_profile_func_exit(void* func, void* callsite) __attribute__((no_instrument_function));
}

bool is_ignored_function(const std::string& func_name) __attribute__((no_instrument_function));
bool is_ignored_function(const std::string& func_name) {
    static std::regex ignored_patterns(R"(^.*\b(std::|__cxx|cxxabi|__gnu_cxx|operator new)\b.*)");
    return std::regex_search(func_name, ignored_patterns);
}



void log_function(const char* action, void* func) __attribute__((no_instrument_function));
void log_function(const char* action, void* func) {
    static thread_local bool inside_log_function = false;

    if (inside_log_function) {
        return;
    }

    inside_log_function = true;
    Dl_info info;
    if (dladdr(func, &info) && info.dli_sname) {
        int status = -1;
        char* demangled_name = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        if (status == 0) {
            std::string demangled_str(demangled_name);
            if (!is_ignored_function(demangled_str)) {
                std::cout << action << ": " << demangled_name << std::endl;
            }
            free(demangled_name);
        } else {
            std::string mangled_str(info.dli_sname);
            if (!is_ignored_function(mangled_str)) {
                std::cout << action << ": " << info.dli_sname << std::endl;
            }
        }
    }
    inside_log_function = false;
}

void __cyg_profile_func_enter(void* func, void* callsite) {
    static thread_local bool inside_log_function = false;
    if (inside_log_function) {
        return;
    }
    inside_log_function = true;
    log_function("[ ENTER ] ", func);
    inside_log_function = false;
}

void __cyg_profile_func_exit(void* func, void* callsite) {
    static thread_local bool inside_log_function = false;
    if (inside_log_function) {
        return;
    }
    inside_log_function = true;

    log_function("[  EXIT ] ", func);

    inside_log_function = false;
}

bool done = false;

void SendControlHello()
{
	TMessageId MId_Hello = 'H';
	TPayload Payload;
	TBytes data{4,3,2,1};

	PTDataItem d2 = std::unique_ptr<TDataItem>(new TDataItem(TCP_ConnectionID, data));
	Payload.push_back(d2);

	//__u32 dacRangeDefault = 0x3031E142;
	//__u32 dacRangeDefault = 0x35303055;
	for (int channel = 0; channel < 4; channel++)
	{
		data.clear();
		data.push_back(channel);
		for (int byt = 0; byt < sizeof(Config.dacRanges[channel]); byt++)
			data.push_back((Config.dacRanges[channel] >> (8 * byt)) & 0x000000FF);
		d2 = std::unique_ptr<TDataItem>(new TDataItem(DAC_Range1, data));
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
	catch (std::logic_error e)
	{
		Error(e.what());
		perror(e.what());
	}

	TMessage HelloControl = TMessage(MId_Hello, Payload);

	TBytes rbuf = HelloControl.AsBytes(true);
	Log("Sent 'Hello' to Control Client#:\n		  " + HelloControl.AsString(true)+"\n", HelloControl.AsBytes());
}

