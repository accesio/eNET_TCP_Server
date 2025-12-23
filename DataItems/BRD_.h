#pragma once

#include "TDataItem.h"
#include "../apci.h"
#include "../eNET-AIO16-16F.h"
#include "../config.h"
#include "../logging.h"

// We’ll store both the read value (config) and the hardware offset here.
template <typename T>
struct ReadOnlyConfigParams {
    T config;
    __u8 offset;
};

// TReadOnlyConfig<T>: a templated class for read-only registers.
// Inherits from TDataItem<ReadOnlyConfigParams<T>>
template <typename T>
class TReadOnlyConfig : public TDataItem<ReadOnlyConfigParams<T>>
{
public:
    // (1) Constructor: DId + offset
    TReadOnlyConfig(DataItemIds id, __u8 offset);

    // (2) Constructor: DId + raw bytes (from factory)
    TReadOnlyConfig(DataItemIds id, const TBytes &FromBytes);

    // (3) Constructor: just TBytes
    explicit TReadOnlyConfig(const TBytes &FromBytes);

    virtual ~TReadOnlyConfig();

    // Overridden from TDataItemBase
    virtual TReadOnlyConfig &Go();                            // Reads hardware
    virtual TBytes calcPayload(bool bAsReply=false) override; // Serializes config
    virtual std::string AsString(bool bAsReply=false) override;

}; // end class TReadOnlyConfig

// =================== Subclasses ===================

// TBRD_FpgaId: specialized for a 32-bit register
class TBRD_FpgaId : public TReadOnlyConfig<__u32>
{
public:
    TBRD_FpgaId();
    TBRD_FpgaId(const TBytes &FromBytes);
    TBRD_FpgaId(DataItemIds id, const TBytes &FromBytes);

    // Override if needed
    virtual std::string AsString(bool bAsReply) override;
    virtual TBRD_FpgaId &Go() override;
    virtual TBytes calcPayload(bool bAsReply=false) override;
};

// TBRD_DeviceID: specialized for a 16-bit register
class TBRD_DeviceID : public TReadOnlyConfig<__u16>
{
public:
    TBRD_DeviceID();
    TBRD_DeviceID(const TBytes &FromBytes);
    TBRD_DeviceID(DataItemIds id, const TBytes &FromBytes);

    virtual std::string AsString(bool bAsReply) override;
    virtual TBRD_DeviceID &Go() override;
    virtual TBytes calcPayload(bool bAsReply=false) override;
};

// TBRD_Features: specialized for an 8-bit register
class TBRD_Features : public TReadOnlyConfig<__u8>
{
public:
    TBRD_Features();
    TBRD_Features(const TBytes &FromBytes);
    TBRD_Features(DataItemIds id, const TBytes &FromBytes);

    virtual std::string AsString(bool bAsReply) override;
    virtual TBRD_Features &Go() override;
    virtual TBytes calcPayload(bool bAsReply=false) override;
};


// Set the device’s Model string (ASCII, 12–40 bytes)
class TBRD_Model : public TDataItem<GenericParams>
{
public:
    explicit TBRD_Model(DataItemIds id, const TBytes &buf)
        : TDataItem<GenericParams>(id, buf) {}

    // no zero-arg ctor
    TBRD_Model() = delete;

    // parse rawBytes, update Config.Model, persist
    TDataItemBase &Go() override;

    // reply payload is the new model string
    TBytes calcPayload(bool bAsReply = false) override;

    // for logging/debug
    std::string AsString(bool bAsReply = false) override;
};

// Read back the current Model
class TBRD_GetModel : public TDataItem<GenericParams>
{
public:
    explicit TBRD_GetModel(DataItemIds id, const TBytes &buf)
        : TDataItem<GenericParams>(id, buf) {}

    TBRD_GetModel() = delete;

    // nothing to do on Go
    TDataItemBase &Go() override { return *this; }

    // payload is the ASCII model
    TBytes calcPayload(bool bAsReply = false) override;

    std::string AsString(bool bAsReply = false) override;
};

// Set the device’s SerialNumber (ASCII, 12–14 bytes)
class TBRD_SerialNumber : public TDataItem<GenericParams>
{
public:
    explicit TBRD_SerialNumber(DataItemIds id, const TBytes &buf)
        : TDataItem<GenericParams>(id, buf) {}

    TBRD_SerialNumber() = delete;

    TDataItemBase &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};

// Read back the current SerialNumber
class TBRD_GetSerialNumber : public TDataItem<GenericParams>
{
public:
    explicit TBRD_GetSerialNumber(DataItemIds id, const TBytes &buf)
        : TDataItem<GenericParams>(id, buf) {}

    TBRD_GetSerialNumber() = delete;

    TDataItemBase &Go() override { return *this; }
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};

struct NumberOfSubmuxParams {
    __u8 value;
};
class TBRD_GetNumberOfSubmuxes : public TDataItem<NumberOfSubmuxParams> {
    public:
        // ctor from raw bytes (ignored)
        TBRD_GetNumberOfSubmuxes(DataItemIds id, const TBytes &buf);
        // no-arg ctor
        TBRD_GetNumberOfSubmuxes(DataItemIds id);

        virtual TBRD_GetNumberOfSubmuxes &Go() override;
        virtual TBytes              calcPayload(bool bAsReply = false) override;
        virtual std::string        AsString(bool bAsReply = false) override;
    };

class TBRD_NumberOfSubmuxes : public TDataItem<NumberOfSubmuxParams>
{
public:
    explicit TBRD_NumberOfSubmuxes(DataItemIds id, const TBytes &buf)
        : TDataItem<NumberOfSubmuxParams>(id, buf)
    {
        // Assuming buf contains the value in the first byte
        if (!buf.empty())
            this->params.value = buf[0];
        else
            this->params.value = 0;  // default/fallback value

        Trace("Parsed numberOfSubmuxes: 0x" + to_hex<__u16>(this->params.value));
    }

    virtual TBytes calcPayload(bool bAsReply = false) override
    {
        // Convert __u8 value to byte payload
        TBytes out;
        out.push_back(this->params.value);
        return out;
    }

    virtual std::string AsString(bool bAsReply = false) override
    {
        return "TBRD_NumberOfSubmuxes(" + std::to_string(this->params.value) + ")";
    }

    virtual TBRD_NumberOfSubmuxes &Go() override
    {
        __u8 newVal = this->params.value; // The value this TDataItem is carrying
        // Extract bits 5:4 from Config.Features:
        __u8 submuxType = (Config.features >> 4) & 0x03;
        bool valid = false;

        // Determine valid value ranges based on submux type.
        switch (submuxType)
        {
            case 0: // AIMUX64M: valid number is exactly 1.
                valid = (newVal == 1);
                break;

            case 1: // AIMUX32: valid numbers are 1, 2, 3, or 4.
                valid = (newVal >= 1 && newVal <= 4);
                break;

            case 3: // No submux: valid number is exactly 0.
                valid = (newVal == 0);
                break;

            default:
                // Should not occur if bits 5:4 only ever contain 00, 01, or 11.
                Error("TBRD_NumberOfSubmuxes::Go() ERROR: Unknown submux type ["+to_hex<__u8>( submuxType)+"]");
                valid = false;
                break;
        }

        if (valid)
        {
            Trace("TBRD_NumberOfSubmuxes::Go() setting Config.numberOfSubmuxes to ["+to_hex<__u8>( newVal)+"]");
            Config.numberOfSubmuxes = newVal;
        }
        else
        {
            Error("TBRD_NumberOfSubmuxes::Go() ERROR: Invalid numberOfSubmuxes ["+to_hex<__u8>( newVal) +
                  "] for submux type ["+to_hex<__u8>( submuxType)+"]");
            // Optionally, you can set a default or take other corrective action here.
        }
        return *this;
    }

};

// ---------------- TBRD_GetNumberOfAdcChannels ----------------
struct BRD_GetNumberOfAdcChannelsParams {
    __u8 numAdcChan = 16; // default
};
class TBRD_GetNumberOfAdcChannels : public TDataItem<BRD_GetNumberOfAdcChannelsParams>
{
public:
    TBRD_GetNumberOfAdcChannels()
    : TDataItem<BRD_GetNumberOfAdcChannelsParams>(DataItemIds::BRD_GetNumberOfAdcChannels, {})
    {
        // optional: parse an empty TBytes
    }
    TBRD_GetNumberOfAdcChannels(DataItemIds id, const TBytes &FromBytes);

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TBRD_GetNumberOfAdcChannels &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};


struct SubmuxScaleParams {
    __u8 submuxIndex;
    __u8 gainGroupIndex;
    float value;
};

class TBRD_GetSubmuxScale : public TDataItem<SubmuxScaleParams> {
public:
    TBRD_GetSubmuxScale(DataItemIds id, const TBytes &buf);

    virtual TBRD_GetSubmuxScale &Go() override;
    virtual TBytes               calcPayload(bool bAsReply = false) override;
    virtual std::string         AsString(bool bAsReply = false) override;
};

class TBRD_SubmuxScale : public TDataItem<SubmuxScaleParams>
{
public:
    // For setting the value directly via pointer to buffer, the buffer will be structured as:
    // [submuxIndex (1 byte), gainGroupIndex (1 byte), then float bytes (4 bytes)]
    explicit TBRD_SubmuxScale(DataItemIds id, const TBytes &buf)
        : TDataItem<SubmuxScaleParams>(id, buf)
    {
        if (buf.size() >= 6)
        {
            this->params.submuxIndex = buf[0];
            this->params.gainGroupIndex = buf[1];
            // Copy 4 bytes for the float value (ensure proper alignment)
            std::memcpy(&this->params.value, buf.data() + 2, sizeof(float));
        }
        else
        {
            this->params.submuxIndex = 0;
            this->params.gainGroupIndex = 0;
            this->params.value = 0.0f;
        }
        char *s=NULL;
        if (asprintf(&s, "%.3f", this->params.value) == -1) {
            // allocation failed

        }
        else {
            Trace("Parsed SubmuxScaleFactor: submux ["+to_hex<__u8>(this->params.submuxIndex)+"]"
                  ", group ["+to_hex<__u8>(this->params.gainGroupIndex)+"], value " + std::string(s));
            free(s);
        }
    }

    virtual TBytes calcPayload(bool bAsReply = false) override
    {
        TBytes out;
        out.push_back(this->params.submuxIndex);
        out.push_back(this->params.gainGroupIndex);
        const __u8* pVal = reinterpret_cast<const __u8*>(&this->params.value);
        out.insert(out.end(), pVal, pVal + sizeof(float));
        return out;
    }

    virtual std::string AsString(bool bAsReply = false) override
    {
        return "TBRD_SubmuxScale[submux " + std::to_string(this->params.submuxIndex) +
               "][group " + std::to_string(this->params.gainGroupIndex) +
               "] = " + std::to_string(this->params.value);
    }

    virtual TBRD_SubmuxScale &Go() override
    {
        // Validate indices before applying; assume maxSubmuxes and gainGroupsPerSubmux are defined constants.
        if (this->params.submuxIndex < maxSubmuxes && this->params.gainGroupIndex < gainGroupsPerSubmux)
        {
            char *s=NULL;
            if (asprintf(&s, "%.3f", this->params.value) == -1) {
                // allocation failed

            }
            else {
                Trace("TBRD_SubmuxScale::Go() setting Config.submuxScaleFactors["+to_hex<__u8>(this->params.submuxIndex)+"]"
                    ", group ["+to_hex<__u8>(this->params.gainGroupIndex)+"] to " + std::string(s));
                free(s);
            }
            Config.submuxScaleFactors[this->params.submuxIndex][this->params.gainGroupIndex] = this->params.value;
        }
        else
        {
            Trace("TBRD_SubmuxScale::Go() ERROR: index out of bounds");
        }
        return *this;
    }
};

struct SubmuxOffsetParams {
    __u8 submuxIndex;
    __u8 gainGroupIndex;
    float value;
};

class TBRD_GetSubmuxOffset : public TDataItem<SubmuxOffsetParams> {
    public:
        TBRD_GetSubmuxOffset(DataItemIds id, const TBytes &buf);

        virtual TBRD_GetSubmuxOffset &Go() override;
        virtual TBytes                calcPayload(bool bAsReply = false) override;
        virtual std::string          AsString(bool bAsReply = false) override;
    };

class TBRD_SubmuxOffset : public TDataItem<SubmuxOffsetParams>
{
public:
    explicit TBRD_SubmuxOffset(DataItemIds id, const TBytes &buf)
        : TDataItem<SubmuxOffsetParams>(id, buf)
    {
        if (buf.size() >= 6)
        {
            this->params.submuxIndex = buf[0];
            this->params.gainGroupIndex = buf[1];
            std::memcpy(&this->params.value, buf.data() + 2, sizeof(float));
        }
        else
        {
            this->params.submuxIndex = 0;
            this->params.gainGroupIndex = 0;
            this->params.value = 0.0f;
        }
        char *s = NULL;
        if (asprintf(&s, "%.3f", this->params.value) == -1) {
            // allocation failed

        }
        else {
            Trace("Parsed SubmuxOffset: submux ["+to_hex<__u8>(this->params.submuxIndex)+"]"
                  ", group ["+to_hex<__u8>(this->params.gainGroupIndex)+"], value " + std::string(s));
            free(s);
        }
    }

    virtual TBytes calcPayload(bool bAsReply = false) override
    {
        TBytes out;
        out.push_back(this->params.submuxIndex);
        out.push_back(this->params.gainGroupIndex);
        const __u8* pVal = reinterpret_cast<const __u8*>(&this->params.value);
        out.insert(out.end(), pVal, pVal + sizeof(float));
        return out;
    }

    virtual std::string AsString(bool bAsReply = false) override
    {
        return "TBRD_SubmuxOffset[submux " + std::to_string(this->params.submuxIndex) +
               "][group " + std::to_string(this->params.gainGroupIndex) +
               "] = " + std::to_string(this->params.value);
    }

    virtual TBRD_SubmuxOffset &Go() override
    {
        if (this->params.submuxIndex < maxSubmuxes && this->params.gainGroupIndex < gainGroupsPerSubmux)
        {
            char *s=NULL;
            if (asprintf(&s, "%.3f", this->params.value) == -1) {
                // allocation failed

            }
            else {
                Trace("TBRD_SubmuxOffset::Go() setting Config.submuxOffsets["+to_hex<__u8>(this->params.submuxIndex)+"]"
                    ", group ["+to_hex<__u8>(this->params.gainGroupIndex)+"] to " + std::string(s));
                free(s);
            }
            Config.submuxOffsets[this->params.submuxIndex][this->params.gainGroupIndex] = this->params.value;
        }
        else
        {
            Trace("TBRD_SubmuxOffset::Go() ERROR: index out of bounds");
        }
        return *this;
    }
};
