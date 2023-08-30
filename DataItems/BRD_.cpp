#include "TDataItem.h"
#include "BRD_.h"
#include "../apci.h"
#include "../eNET-AIO16-16F.h"

std::string TBRD_FpgaId::AsString(bool bAsReply){
	if (bAsReply)
		return "BRD_FpgaId() → " + to_hex<__u32>(this->fpgaID);
	else
		return "BRD_FpgaId()";
}
TBRD_FpgaId &TBRD_FpgaId::Go() {
	this->fpgaID = in(ofsFpgaID);
	Log("Offset = "+to_hex<__u8>(ofsFpgaID)+" > " + to_hex<__u32>(this->fpgaID));
	return *this;
}
TBytes TBRD_FpgaId::calcPayload(bool bAsReply) {
	TBytes bytes;
	stuff<__u32>(bytes, this->fpgaID);
	return bytes;
}


std::string TBRD_DeviceID::AsString(bool bAsReply) {
	if (bAsReply)
		return "BRD_DeviceID() → " + to_hex<__u16>(this->deviceID);
	else
		return "BRD_DeviceID()";
}
TBRD_DeviceID &TBRD_DeviceID::Go() {
	this->deviceID = static_cast<__u16>(in(ofsDeviceID));
	Log("Offset = "+to_hex<__u8>(ofsDeviceID)+" > " + to_hex<__u16>(this->deviceID));
	return *this;
}
TBytes TBRD_DeviceID::calcPayload(bool bAsReply) {
	TBytes bytes;
	stuff<__u16>(bytes, this->deviceID);
	return bytes;
}


std::string TBRD_Features::AsString(bool bAsReply) {
	if (bAsReply)
		return "BRD_Features() → " + to_hex<__u8>(this->features);
	else
		return "BRD_Features()";
}
TBRD_Features &TBRD_Features::Go() {
	this->features = static_cast<__u8>(in(ofsFeatures));
    Log("Offset = "+to_hex<__u8>(ofsFeatures)+" > " + to_hex<__u8>(this->features));
	return *this;
}
TBytes TBRD_Features::calcPayload(bool bAsReply){
	TBytes bytes;
	stuff<__u32>(bytes, this->features);
	return bytes;
}

#pragma endregion
