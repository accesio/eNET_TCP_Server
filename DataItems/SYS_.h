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
	explicit TSYS_UploadFileName(DataItemIds DId, TBytes buf);
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
	explicit TSYS_UploadFileData(DataItemIds DId, TBytes buf);

	// returns the sanitized file name
	virtual TBytes calcPayload(bool bAsReply = false);
	virtual std::string AsString(bool bAsReply = false);
	virtual TSYS_UploadFileData &Go();
};
