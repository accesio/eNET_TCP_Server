/*
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <map>

#include "../eNET-types.h"
#include "../logging.h"

class TDataItemBase;
using PTDataItemBase = std::shared_ptr<TDataItemBase>;
using PTDataItem = std::shared_ptr<TDataItemBase>;
using TPayload = std::vector<PTDataItem>;
using TDataId = __u16;
using TDataItemLength=__u16;

typedef struct
{
	DataItemIds DId;
	TDataItemLength dataLength;
} TDataItemHeader;

typedef std::shared_ptr<TDataItemBase> DIdConstructor(DataItemIds id, TBytes FromBytes);

typedef struct __DIdDictEntry_inner
{
	TDataItemLength minLen;
	TDataItemLength typLen;
	TDataItemLength maxLen;
	// std::function<std::unique_ptr<TDataItem>(TBytes)> Construct;
	DIdConstructor *Construct;
	std::string desc;
	std::function<void(void *)> go;
} TDIdDictEntry;

//  __DIdDictEntry_inner ;
extern const std::map<DataItemIds, TDIdDictEntry> DIdDict;

class TDataItemBase
{
public:
    // ========== Common Fields ==========push, 1
	TBytes Data;
    DataItemIds DId;      // e.g., DAC_Output1, DAC_Range1
    TError resultCode = ERR_SUCCESS;
    bool bWrite = false;  // Used by many items to indicate write vs. read
    int conn = 0;         // Connection ID or similar
    // ========== Constructors ==========
    explicit TDataItemBase(DataItemIds id) : DId(id)
    {
        Debug("TDataItemBase(id) constructor");
    }

    virtual ~TDataItemBase() {}

	std::string AsStringBase(bool bAsReply) const;

    // ========== Virtual Methods for Polymorphism ==========
    // Called by the main worker thread to execute hardware logic
    virtual TDataItemBase &Go() = 0;

    // Serializes the data item to bytes (for sending over TCP)
    virtual TBytes calcPayload(bool bAsReply = false) = 0;

    // High-level string form for debugging/logging
	virtual std::string AsString(bool bAsReply = false) = 0;

	// ========== Static / Factory Methods ==========
    static std::shared_ptr<TDataItemBase> fromBytes(const TBytes &msg, TError &result);

    static int validateDataItemPayload(DataItemIds DataItemID, const TBytes &Data);
    static int isValidDataItemID(DataItemIds DataItemID);
    static int validateDataItem(const TBytes &msg);
    static __u16 getMinLength(DataItemIds id);
    static __u16 getTargetLength(DataItemIds id);
    static __u16 getMaxLength(DataItemIds id);
    static int getDIdIndex(DataItemIds id);
    static std::string getDIdDesc(DataItemIds id);

    TDataItemBase &addData(__u8 aByte);
    TDataItemBase &setDId(DataItemIds newId);
    DataItemIds getDId() const;
    bool isValidDataLength() const;
    TBytes AsBytes(bool bAsReply);
    std::string getDIdDesc() const;
    TError getResultCode();
    std::shared_ptr<void> getResultValue();
    // These might be used for name lookups, etc.
};
#pragma endregion



*/

