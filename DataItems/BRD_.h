#pragma once
#include "TDataItem.h"

template <class T>
class TReadOnlyConfig : public TDataItem {
	private:
		T config;
		__u8 offset;
		TDataId DId;
	public:
		std::string  AsString(bool bAsReply)
		{
			if (bAsReply)
				return DIdList[getDIdIndex(this->DId)].desc + " → " + to_hex<T>(this->config);
				else
				return DIdList[getDIdIndex(this->DId)].desc;
		}

		TReadOnlyConfig(TDataId DId, __u8 offset);
		virtual TReadOnlyConfig &Go() {
			config = in<T>(offset);
			return *this;
		}
};

class TBRD_FpgaID : public TDataItem {
public:
	TBRD_FpgaID(){ setDId(BRD_FpgaID); }
	virtual std::string AsString(bool bAsReply = false);
	virtual TBRD_FpgaID &Go();
	virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u32 fpgaID = 0x00010005;
};

class TBRD_DeviceID : public TDataItem {
public:
	TBRD_DeviceID(){ setDId(BRD_DeviceID); }
	virtual std::string AsString(bool bAsReply = false);
	virtual TBRD_DeviceID &Go();
	virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u16 deviceID = 0;
};

class TBRD_Features : public TDataItem {
public:
	TBRD_Features(){ setDId(BRD_Features); }
	virtual std::string AsString(bool bAsReply = false);
	virtual TBRD_Features &Go();
	virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u8 features = 0;
};