#pragma once

#include "TDataItem.h"

void UploadFilesByDataItem(const TDataItemBase& item);
std::string generateBackupFilenameWithBuildTime(std::string base="aioenetd_");
std::error_code update_symlink_atomic(const char *target, const char *linkpath);
std::error_code Update(TBytes newfile);
std::error_code Revert();


class TSYS_UploadFileName : public TDataItemBase
{
public:
	explicit TSYS_UploadFileName(TBytes buf) : TSYS_UploadFileName(DataItemIds::SYS_UploadFileName, buf){};
	explicit TSYS_UploadFileName(DataItemIds id, TBytes buf);
	//TSYS_UploadFileName() : TDataItem(SYS_UploadFileName){}

	// returns the sanitized file name
	virtual TBytes calcPayload(bool bAsReply = false);
	virtual std::string AsString(bool bAsReply = false);
	virtual TSYS_UploadFileName &Go();
};


// SYS_UploadFileName.Go will call UploadFilesByDataItem(*this).


class TSYS_UploadFileData : public TDataItemBase
{
public:
	explicit TSYS_UploadFileData(TBytes buf) : TSYS_UploadFileData(DataItemIds::SYS_UploadFileName, buf){};
	explicit TSYS_UploadFileData(DataItemIds id, TBytes buf);

	// returns the sanitized file name
	virtual TBytes calcPayload(bool bAsReply = false);
	virtual std::string AsString(bool bAsReply = false);
	virtual TSYS_UploadFileData &Go();
};


#pragma pack(push, 1)
struct SYS_ErrorParams
{
	__u32 Stage = 0;
	__u32 ErrorCode = ERR_SUCCESS; // raw TError value (u32); client can do idx = -ErrorCode to index err_msg[]
	__u32 Info = 0;
};

struct SYS_ItemErrorParams
{
	__u16 ItemIndex = 0;
	__u16 DId = 0;               // original DataItemIds value (u16)
	__u32 ErrorCode = ERR_SUCCESS;
	__u32 Info = 0;
};
#pragma pack(pop)

class TSYS_Error : public TDataItem<SYS_ErrorParams>
{
public:
	TSYS_Error(DataItemIds id, const TBytes &FromBytes);
	TSYS_Error(__u32 stage, TError errorCode, __u32 info);

	virtual TSYS_Error &Go() override;
	virtual TBytes calcPayload(bool bAsReply = false) override;
	virtual std::string AsString(bool bAsReply = false) override;
};

class TSYS_ItemError : public TDataItem<SYS_ItemErrorParams>
{
public:
	TSYS_ItemError(DataItemIds id, const TBytes &FromBytes);
	TSYS_ItemError(__u16 itemIndex, DataItemIds originalDid, TError errorCode, __u32 info);

	virtual TSYS_ItemError &Go() override;
	virtual TBytes calcPayload(bool bAsReply = false) override;
	virtual std::string AsString(bool bAsReply = false) override;
};
