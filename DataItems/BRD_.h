#pragma once
#include "TDataItem.h"
#include "../apci.h"

template <class T>
class TReadOnlyConfig : public TDataItem {
	protected:
		T config;
		__u8 offset;
		DataItemIds DId;
	public:
		TReadOnlyConfig(TBytes FromBytes) : TDataItem(FromBytes){};
		TReadOnlyConfig(DataItemIds DId, __u8 offset): DId{DId},offset{offset}{}
		std::string  AsString(bool bAsReply)
		{
			if (bAsReply)
				return DIdList[getDIdIndex(this->DId)].desc + " → " + to_hex<T>(this->config);
				else
				return DIdList[getDIdIndex(this->DId)].desc;
		}
		virtual TReadOnlyConfig &Go() {
			config = in(offset);
			return *this;
		}
};

class TBRD_FpgaId : public TReadOnlyConfig<__u32> {
public:
	TBRD_FpgaId() : TReadOnlyConfig(BRD_FpgaID, 0x68){}
	TBRD_FpgaId(TBytes FromBytes) : TReadOnlyConfig<__u32>(FromBytes){DId = BRD_FpgaID; offset = ofsFpgaID; }
};


// class TBRD_FpgaID : public TDataItem {
// public:
// 	TBRD_FpgaID(){ setDId(BRD_FpgaID); }
// 	virtual std::string AsString(bool bAsReply = false);
// 	virtual TBRD_FpgaID &Go();
// 	virtual TBytes calcPayload(bool bAsReply=false);
// protected:
// 	__u32 fpgaID = 0x00010005;
// };

class TBRD_DeviceID : public TReadOnlyConfig<__u16> {
public:
	TBRD_DeviceID() : TReadOnlyConfig(BRD_DeviceID, ofsDeviceID){};
	TBRD_DeviceID(TBytes FromBytes) : TReadOnlyConfig<__u16>(FromBytes){DId = BRD_DeviceID; offset = ofsDeviceID; }
	// virtual std::string AsString(bool bAsReply = false);
	// virtual TBRD_DeviceID &Go();
	// virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u16 deviceID = 0;
};

class TBRD_Features : public TReadOnlyConfig<__u8> {
public:
	TBRD_Features() : TReadOnlyConfig( BRD_Features, ofsFeatures){};
	TBRD_Features(TBytes FromBytes) : TReadOnlyConfig<__u8>(FromBytes) { DId = BRD_Features, offset = ofsFeatures; }
	// virtual std::string AsString(bool bAsReply = false);
	// virtual TBRD_Features &Go();
	// virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u8 features = 0;
};