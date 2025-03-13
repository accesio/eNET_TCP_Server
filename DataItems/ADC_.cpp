#include "ADC_.h"
#include "../apci.h"
#include "../logging.h"
#include "../eNET-AIO16-16F.h"
#include "../adc.h"

extern int apci;

TADC_BaseClock::TADC_BaseClock(DataItemIds dId, const TBytes &buf)
  : TDataItem<ADC_BaseClockParams>(dId, buf)
{
    // You previously had a constructor that validated 0 or 4 bytes
    GUARD((buf.size() == 0) || (buf.size() == 4), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);

    // Optionally parse the 4 bytes into this->params.baseClock if you want
    // If you do:
    if (buf.size() == 4) {
        // e.g.:
        this->params.baseClock = *reinterpret_cast<const __u32*>(buf.data());
    }
}

TBytes TADC_BaseClock::calcPayload(bool bAsReply)
{
    TBytes bytes;
    // stuff() your param into bytes
    stuff(bytes, this->params.baseClock);
    Trace("TADC_BaseClock::calcPayload built: ", bytes);
    return bytes;
}

TADC_BaseClock &TADC_BaseClock::Go()
{
    Trace("ADC_BaseClock Go() reading hardware");
    // Instead of a separate field, we do:
    this->params.baseClock = in(ofsAdcBaseClock);
    return *this;
}

std::string TADC_BaseClock::AsString(bool bAsReply)
{
    std::stringstream dest;
    dest << "ADC_BaseClock()";
    if (bAsReply)
        dest << " → " << this->params.baseClock;
    else
    dest << " ? " << this->params.baseClock;
    return dest.str();
}


// -------------------- TADC_StreamStart --------------------

TADC_StreamStart::TADC_StreamStart(DataItemIds dId, const TBytes &FromBytes)
  : TDataItem<ADC_StreamStartParams>(dId, FromBytes)
{
    // If you want to parse a 4-byte connection ID, do so here
    // For example:
    if (FromBytes.size() == 4) {
        int cid = *reinterpret_cast<const int*>(FromBytes.data());
        // If there's a global "AdcStreamingConnection" logic
        if (AdcStreamingConnection == -1) {
            AdcStreamingConnection = cid;
            this->params.argConnectionID = cid;
        } else {
            Error("ADC Busy, already streaming on Connection: " + std::to_string(AdcStreamingConnection));
            throw std::logic_error("ADC Busy already on Connection: " + std::to_string(AdcStreamingConnection));
        }
        Trace("AdcStreamingConnection: " + std::to_string(AdcStreamingConnection));
    }
}

TBytes TADC_StreamStart::calcPayload(bool bAsReply)
{
    TBytes bytes;
    // stuff() your argConnectionID from params
    stuff(bytes, this->params.argConnectionID);
    Trace("TADC_StreamStart::calcPayload built: ", bytes);
    return bytes;
}

TADC_StreamStart &TADC_StreamStart::Go()
{
    Trace("ADC_StreamStart::Go(), ADC Streaming Data will be sent on ConnectionID: "
          + std::to_string(AdcStreamingConnection));

    // Example code from your snippet
    auto status = apciDmaTransferSize(RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
    if (status)
    {
        Error("Error setting apciDmaTransferSize: " + std::to_string(status));
        throw std::logic_error(err_msg[-status]);
    }

    AdcStreamTerminate = 0;
    if (AdcWorkerThreadID == -1)
    {
        AdcWorkerThreadID = pthread_create(&worker_thread, NULL, &worker_main, &AdcStreamingConnection);
    }
    apciDmaStart();

    return *this;
}

std::string TADC_StreamStart::AsString(bool bAsReply)
{
    std::string msg = this->getDIdDesc(this->DId);
    if (bAsReply)
    {
        msg += ", ConnectionID = " + to_hex<int>(this->params.argConnectionID);
    }
    return msg;
}


// -------------------- TADC_StreamStop --------------------

TBytes TADC_StreamStop::calcPayload(bool bAsReply)
{
    // No fields to serialize, so return an empty vector
    return {};
}

TADC_StreamStop &TADC_StreamStop::Go()
{
    Trace("ADC_StreamStop::Go(): terminating ADC Streaming");
    AdcStreamTerminate = 1;
    apciCancelWaitForIRQ();
    AdcStreamingConnection = -1;
    AdcWorkerThreadID = -1;
    // possibly join or cancel the worker_thread
    Trace("ADC_StreamStop::Go() exiting");
    return *this;
}

std::string TADC_StreamStop::AsString(bool bAsReply)
{
    return this->getDIdDesc(this->DId); // for example
}


// #include "ADC_.h"
// #include "../apci.h"
// #include "../logging.h"
// #include "../eNET-AIO16-16F.h"
// #include "../adc.h"

// extern int apci;

// TADC_BaseClock::TADC_BaseClock(TBytes buf)
// {
// 	this->setDId(ADC_BaseClock);
// 	GUARD((buf.size() == 0) || (buf.size() == 4), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);
// }

// TBytes TADC_BaseClock::calcPayload(bool bAsReply)
// {
// 	TBytes bytes;
// 	stuff(bytes, this->baseClock);
// 	Trace("TADC_BaseClock::calcPayload built: ", bytes);
// 	return bytes;
// };

// TADC_BaseClock &TADC_BaseClock::Go()
// {
// 	Trace("ADC_BaseClock Go()");
// 	this->baseClock = in(ofsAdcBaseClock);
// 	return *this;
// }

// std::string TADC_BaseClock::AsString(bool bAsReply)
// {
// 	std::stringstream dest;

// 	dest << "ADC_BaseClock()";
// 	if (bAsReply)
// 	{
// 		dest << " → " << this->baseClock;
// 	}
// 	Trace("Built: " + dest.str());
// 	return dest.str();
// }

// TADC_StreamStart::TADC_StreamStart(TBytes buf)
// {
// 	this->setDId(ADC_StreamStart);
// 	GUARD((buf.size() == 0) || (buf.size() == 4), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);

// 	if (buf.size() == 4)
// 	{
// 		if (-1 == AdcStreamingConnection)
// 		{
// 			this->argConnectionID = (int)*(__u32 *)buf.data();
// 			AdcStreamingConnection = this->argConnectionID;
// 		}
// 		else
// 		{
// 			Error("ADC Busy");
// 			throw std::logic_error("ADC Busy already, on Connection: "+std::to_string(AdcStreamingConnection));
// 		}
// 	}
// 	Trace("AdcStreamingConnection: "+std::to_string(AdcStreamingConnection));
// }

// TBytes TADC_StreamStart::calcPayload(bool bAsReply)
// {
// 	TBytes bytes;
// 	stuff(bytes, this->argConnectionID);
// 	Trace("TADC_StreamStart::calcPayload built: ", bytes);
// 	return bytes;
// };

// TADC_StreamStart &TADC_StreamStart::Go()
// {
// 	Trace("ADC_StreamStart::Go(), ADC Streaming Data will be sent on ConnectionID: "+std::to_string(AdcStreamingConnection));
// 	auto status = apciDmaTransferSize(RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
// 	if (status)
// 	{
// 		Error("Error setting apciDmaTransferSize: "+std::to_string(status));
// 		throw std::logic_error(err_msg[-status]);
// 	}

// 	AdcStreamTerminate = 0;
// 	if (AdcWorkerThreadID == -1)
// 	{
// 		AdcWorkerThreadID = pthread_create(&worker_thread, NULL, &worker_main, &AdcStreamingConnection);
// 	}
// 	apciDmaStart();
// 	return *this;
// };

// std::string TADC_StreamStart::AsString(bool bAsReply)
// {
// 	std::string msg = this->getDIdDesc();
// 	if (bAsReply)
// 	{
// 		msg += ", ConnectionID = " + to_hex<int>(this->argConnectionID);
// 	}
// 	return msg;
// };



// TADC_StreamStop::TADC_StreamStop(TBytes buf)
// {
// 	this->setDId(ADC_StreamStop);
// 	GUARD(buf.size() == 0, ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);
// }

// TBytes TADC_StreamStop::calcPayload(bool bAsReply)
// {
// 	TBytes bytes;
// 	return bytes;
// };

// TADC_StreamStop &TADC_StreamStop::Go()
// {
// 	Trace("ADC_StreamStop::Go(): terminating ADC Streaming");
// 	AdcStreamTerminate = 1;
// 	apciCancelWaitForIRQ();
// 	AdcStreamingConnection = -1;
// 	AdcWorkerThreadID = -1;
// 	// pthread_cancel(worker_thread);
// 	// pthread_join(worker_thread, NULL);
// 	Trace("ADC_StreamStop::Go() exiting");
// 	return *this;
// };
