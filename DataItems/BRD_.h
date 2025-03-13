#pragma once

#include "TDataItem.h"
#include "../apci.h"
#include "../eNET-AIO16-16F.h"
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
    TReadOnlyConfig(DataItemIds dId, __u8 offset);

    // (2) Constructor: DId + raw bytes (from factory)
    TReadOnlyConfig(DataItemIds dId, const TBytes &FromBytes);

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
    TBRD_FpgaId(DataItemIds DId, const TBytes &FromBytes);

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
    TBRD_DeviceID(DataItemIds DId, const TBytes &FromBytes);

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
    TBRD_Features(DataItemIds DId, const TBytes &FromBytes);

    virtual std::string AsString(bool bAsReply) override;
    virtual TBRD_Features &Go() override;
    virtual TBytes calcPayload(bool bAsReply=false) override;
};

