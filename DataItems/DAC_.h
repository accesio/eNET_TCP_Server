#pragma once

#include "TDataItem.h"
#include "../utilities.h"

class TDAC_Output : public TDataItem
{
public:
	explicit TDAC_Output(TBytes buf);
	TDAC_Output(DataItemIds DId, TBytes FromBytes) : TDataItem(DId, FromBytes) {}
	virtual TBytes calcPayload(bool bAsReply=false);
	virtual std::string AsString(bool bAsReply = false);
	virtual TDAC_Output &Go();
protected:
	__u8 dacChannel = 0;
	__u16 dacCounts = 0;
};



class TDAC_Range1 : public TDataItem
{
public:
	explicit TDAC_Range1(TBytes buf);
	TDAC_Range1() : TDataItem(DataItemIds::DAC_Range1){};
	TDAC_Range1(DataItemIds DId, TBytes FromBytes) : TDataItem(DId, FromBytes) {}
	virtual TBytes calcPayload(bool bAsReply=false);
	virtual std::string AsString(bool bAsReply = false);
	virtual TDAC_Range1 &Go();
protected:
	__u8 dacChannel = 0;
	__u32 dacRange = 0;
};

