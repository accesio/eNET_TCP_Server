/*
TDataItemParent - virtual / interface
	Provides a base class that defines the basic functionality and data associated with every action or query available
	to the eNET- protocol as encapsulated by serialized Data Items.

	Every serialized DataItem has a type code, a payload length, and an optional payload (called "Data").  The
	TDataItemParent deserializes this packet in its constructor, creating an Object of type TDataItem(descendant).

	The private fields in TDataItemParent are virtually identical to the serialized form of the Data Item: a DataItemId
	identical to the type code, and the Data as a TBytes (vector<__u8>) which maintains the payload length and content.

	TDataItemParent should have a factory function that takes a serialized DataItem and parses it for validity then
	constructs the appropriate descendant type as found in the dictionary<DataItemID, struct DataItemInfoStructThingy>

	However, the behavior of all DataItems is structually identical ...
	)	TDataItems descendants have a constructor
	)	TDataItems have a "Go()" function that the ActionThread executes as part of the TMessage execution.
	)	TDataItems have a "calcPayload(bAsReply)" function that the send thread executes to serialize the results into a
		packet for sending to the client.
	... thus it is plausible to define these three functions instead of a descendant class, and pass the function pointers as
	part of the DataItemInfoStructThingy.  Doing so is the non-class/OOP technique.


	The goal of this discussion is to refactor the existing Data Item class hierarchy into a clean, steamlined, and logical
	concept.

	basically, TDataItemParent should act like TObject in the Delphi VCL and each descendant should add minimally.

	First, a bunch of static functions should move from TDataItem to the TDataItemParent.  Others should be deleted.  (A bunch
	of code is conceptually needed for consumers of TMessage et al who act as *clients*, not a concern at the moment, worry
	about it later!)
*/

#pragma once
#include <functional>
#include <map>
#include <memory>
#include <iterator>
#include <fmt/core.h>

#include "TDataItemBase.h"
#include "../logging.h"
#include "../utilities.h"
#include "../eNET-AIO16-16F.h"
#include "../TError.h"
#include "../apci.h"

template<typename> class TDataItem;
struct GenericParams {};
using TPayload = std::vector<PTDataItemBase>;

int validateDataItemPayload(DataItemIds DataItemID, TBytes Data);
// return register width for given offset as defined for eNET-AIO registers
// returns 0 if offset is invalid
int widthFromOffset(int ofs);

#pragma region TDataItemBase


template <class q>
std::shared_ptr<TDataItemBase> construct(DataItemIds id, TBytes FromBytes)
{
    // q must inherit from TDataItemBase
    return std::make_shared<q>(id, FromBytes);
}


#pragma region "class TDataItem" declaration

// Base class for all TDataItems, descendants of which will handle payloads specific to the DId
template <typename ParamStruct>
class TDataItem : public TDataItemBase
{
public:
    ParamStruct params{};  // e.g., DAC_OutputParams
	TBytes rawBytes;

	TBytes calcPayload(bool /*bAsReply*/ = false) override {
        if constexpr (std::is_trivially_copyable_v<ParamStruct>) {
            TBytes bytes(sizeof(ParamStruct));
            std::memcpy(bytes.data(), &params, sizeof(ParamStruct));
            return bytes;                      // ← return what we built (not rawBytes)
        } else {
            // Non-trivial types must serialize themselves in derived classes.
            return rawBytes;
        }
    }

    // Constructor
    TDataItem(DataItemIds id, const TBytes &bytes)
        : TDataItemBase(id), rawBytes(bytes)
    {
        Debug("TDataItem<ParamStruct>(id, bytes) constructor");

        // By default, copy entire `bytes` into `params` if it fits
        if (bytes.size() >= sizeof(ParamStruct)) {
            std::memcpy(&params, bytes.data(), sizeof(ParamStruct));
        }
        else {
            // We'll let the derived class handle partial or custom parsing if it wants
            Debug("ParamStruct is bigger than rawBytes; derived constructor may fix this." + to_hex<size_t>(bytes.size()) + " > " + to_hex<size_t>(sizeof(ParamStruct)));
        }
    }

    virtual ~TDataItem() {}

    // Must implement
    virtual TDataItemBase &Go() override = 0;
    virtual std::string AsString(bool bAsReply = false) override = 0;
};

#pragma endregion TDataItem declaration
#pragma region TDataItemDoc
struct DOC_Params { };

class TDataItemDoc : public TDataItem<DOC_Params> {
public:
    // Constructors: the incoming TBytes payload is ignored.
    TDataItemDoc(DataItemIds id, const TBytes &data);
    TDataItemDoc(DataItemIds id);

    // Assemble documentation payload into this->Data.
    virtual TDataItemBase &Go() override;
    // Simply return the assembled payload.
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

#pragma region TDataItemDocGet
struct DOC_Get_Params { };

class TDataItemDocGet : public TDataItem<DOC_Get_Params> {
public:
    // Constructors – incoming payload is ignored.
    TDataItemDocGet(DataItemIds id, const TBytes &data);
    TDataItemDocGet(DataItemIds id);

    // In Go() we assemble the documentation payload into this->Data.
    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};
#pragma endregion

#pragma region TDataItemNYI declaration
struct NYIParams {
    // no fields
};

class TDataItemNYI : public TDataItem<NYIParams>
{
public:
    // Must match the TDataItem<NYIParams>(DataItemIds id, TBytes bytes) signature
    TDataItemNYI(DataItemIds id, TBytes bytes)
        : TDataItem(id, bytes)
    {
        // Possibly do debug logs
        Debug("TDataItemNYI constructor: DId=" + std::to_string((int)DId));
    }

    // Implement pure virtuals from TDataItemBase
    TDataItemBase &Go() override {
        // do nothing or debug
        Debug("NYI Go()");
        return *this;
    }

	std::string AsString(bool bAsReply) {
		return "TDataItemNYI: Not Yet Implemented";
	}
};

#pragma endregion
