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
		virtual TBytes calcPayload(bool bAsReply=false) = 0;
		explicit TReadOnlyConfig(TBytes FromBytes) : TDataItem(FromBytes){}
		TReadOnlyConfig(DataItemIds DId, TBytes FromBytes) : TDataItem(DId, FromBytes) {}
		TReadOnlyConfig(DataItemIds DId, __u8 offset): TDataItem(DId), offset{offset}{}
		std::string  AsString(bool bAsReply)
		{
			if (bAsReply)
				return DIdDict.find(this->DId)->second.desc + " → " + to_hex<T>(this->config);
			else
				return DIdDict.find(this->DId)->second.desc;
		}
		virtual TReadOnlyConfig &Go() {
			config = in(offset);
			return *this;
		}
};


class TBRD_FpgaId : public TReadOnlyConfig<__u32> {
public:
	//TBRD_FpgaId(){ setDId(BRD_FpgaID); }
	TBRD_FpgaId() : TReadOnlyConfig(DataItemIds::BRD_FpgaID, ofsFpgaID){Debug("ReadOnlyConfig(DId,ofsFpgaID);");};
	explicit TBRD_FpgaId(TBytes FromBytes) : TReadOnlyConfig(FromBytes){DId = DataItemIds::BRD_FpgaID; offset = ofsFpgaID; }
	TBRD_FpgaId(DataItemIds DId, TBytes FromBytes) : TReadOnlyConfig<__u32>(DId, FromBytes) {offset = ofsFpgaID;}
	virtual std::string AsString(bool bAsReply = false);
	virtual TBRD_FpgaId &Go();
	virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u32 fpgaID = 0x00010005;
};

class TBRD_DeviceID : public TReadOnlyConfig<__u16> {
public:
	TBRD_DeviceID() : TReadOnlyConfig(DataItemIds::BRD_DeviceID, ofsDeviceID){};
	explicit TBRD_DeviceID(TBytes FromBytes) : TReadOnlyConfig<__u16>(FromBytes){DId = DataItemIds::BRD_DeviceID; offset = ofsDeviceID; }
	TBRD_DeviceID(DataItemIds DId, TBytes FromBytes) : TReadOnlyConfig<__u16>(DId, FromBytes) {offset = ofsDeviceID;}
	virtual std::string AsString(bool bAsReply = false);
	virtual TBRD_DeviceID &Go();
	virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u16 deviceID = 0;
};

class TBRD_Features : public TReadOnlyConfig<__u8> {
public:
	TBRD_Features() : TReadOnlyConfig<__u8>( DataItemIds::BRD_Features, ofsFeatures){};
	explicit TBRD_Features(TBytes FromBytes) : TReadOnlyConfig<__u8>(FromBytes) { DId = DataItemIds::BRD_Features, offset = ofsFeatures; }
	TBRD_Features(DataItemIds DId, TBytes FromBytes) : TReadOnlyConfig<__u8>(DId, FromBytes) {offset = ofsFeatures;}
	virtual std::string AsString(bool bAsReply = false);
	virtual TBRD_Features &Go();
	virtual TBytes calcPayload(bool bAsReply=false);
protected:
	__u8 features = 0;
};



// class TBRD_FpgaId : public TReadOnlyConfig<__u32> {
// public:
// 	TBRD_FpgaId() : TReadOnlyConfig(BRD_FpgaID, 0x68){}
// 	TBRD_FpgaId(TBytes FromBytes) : TReadOnlyConfig<__u32>(FromBytes){DId = BRD_FpgaID; offset = ofsFpgaID; }
// };
