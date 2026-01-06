#pragma GCC push_options
#pragma GCC sanitize ("")   // empty disables all -fsanitize flags for this TU
#pragma GCC optimize ("O0")

#include "BRD_.h"
#include "../config.h"
#include "../utilities.h"   // For stuff<>, etc.

// =================== TReadOnlyConfig<T> ===================

// (1) Constructor: DId + offset
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(DataItemIds id, __u8 offset)
    : TDataItem<ReadOnlyConfigParams<T>>(id, {})
{
    this->params.offset = offset;
}

// (2) Constructor: DId + raw bytes
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(DataItemIds id, const TBytes &FromBytes)
    : TDataItem<ReadOnlyConfigParams<T>>(id, FromBytes)
{
    // By default offset=0. Subclasses can override (like TBRD_DeviceID) if needed
    this->params.offset = 0;
    Debug("TReadOnlyConfig(DataItemIds, TBytes) constructor");
}

// (3) Constructor: just TBytes
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(const TBytes &FromBytes)
    : TDataItem<ReadOnlyConfigParams<T>>(DataItemIds::INVALID, FromBytes)
{
    this->params.offset = 0;
    Debug("TReadOnlyConfig(TBytes) constructor, set offset=0");
}

template <typename T>
TReadOnlyConfig<T>::~TReadOnlyConfig() {}

// Default .Go() → reads hardware
template <typename T>
TReadOnlyConfig<T> &TReadOnlyConfig<T>::Go()
{
    T raw = static_cast<T>(in(this->params.offset));
    this->params.config = raw;
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<T>(this->params.config));
    return *this;
}

// Default .calcPayload() → returns the config
template <typename T>
TBytes TReadOnlyConfig<T>::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff<T>(bytes, this->params.config);
    return bytes;
}

// Default .AsString() → uses DIdDict
template <typename T>
std::string TReadOnlyConfig<T>::AsString(bool bAsReply)
{
    auto it = DIdDict.find(this->DId);
    if (it == DIdDict.end())
    {
        return "Unknown TReadOnlyConfig DId=" + std::to_string((int)this->DId);
    }
    if (bAsReply)
    {
        return it->second.desc + " → " + to_hex<T>(this->params.config);
    }
    else
    {
        return it->second.desc;
    }
}

// Explicit instantiations to avoid linker issues if needed:
template class TReadOnlyConfig<__u8>;
template class TReadOnlyConfig<__u16>;
template class TReadOnlyConfig<__u32>;

// =================== TBRD_FpgaId ===================
TBRD_FpgaId::TBRD_FpgaId()
    : TReadOnlyConfig<__u32>(DataItemIds::BRD_FpgaID, ofsFpgaID)
{
    Debug("TBRD_FpgaId() default constructor with offset=ofsFpgaID");
}

TBRD_FpgaId::TBRD_FpgaId(const TBytes &FromBytes)
    : TReadOnlyConfig<__u32>(FromBytes)
{
    this->DId = DataItemIds::BRD_FpgaID;
    this->params.offset = ofsFpgaID;
}

TBRD_FpgaId::TBRD_FpgaId(DataItemIds id, const TBytes &FromBytes)
    : TReadOnlyConfig<__u32>(id, FromBytes)
{
    this->params.offset = ofsFpgaID;
}

// Override AsString, Go, calcPayload as needed

std::string TBRD_FpgaId::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_FpgaId() → " + to_hex<__u32>(this->params.config);
    }
    else {
        return "BRD_FpgaId() ? " + to_hex<__u32>(this->params.config);
    }
}

TBRD_FpgaId &TBRD_FpgaId::Go()
{
    this->params.config = in(this->params.offset);
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<__u32>(this->params.config));
    return *this;
}

TBytes TBRD_FpgaId::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff<__u32>(bytes, this->params.config);
    return bytes;
}

// =================== TBRD_DeviceID ===================
TBRD_DeviceID::TBRD_DeviceID()
    : TReadOnlyConfig<__u16>(DataItemIds::BRD_DeviceID, ofsDeviceID)
{}

TBRD_DeviceID::TBRD_DeviceID(const TBytes &FromBytes)
    : TReadOnlyConfig<__u16>(FromBytes)
{
    this->DId = DataItemIds::BRD_DeviceID;
    this->params.offset = ofsDeviceID;
}

TBRD_DeviceID::TBRD_DeviceID(DataItemIds id, const TBytes &FromBytes)
    : TReadOnlyConfig<__u16>(id, FromBytes)
{
    this->params.offset = ofsDeviceID;
}

std::string TBRD_DeviceID::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_DeviceID() → " + to_hex<__u16>(this->params.config);
    }
    else {
        return "BRD_DeviceID() ? " + to_hex<__u16>(this->params.config);
    }
}

TBRD_DeviceID &TBRD_DeviceID::Go()
{
    this->params.config = static_cast<__u16>(in(this->params.offset));
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<__u16>(this->params.config));
    return *this;
}

TBytes TBRD_DeviceID::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff<__u16>(bytes, this->params.config);
    return bytes;
}

// =================== TBRD_Features ===================
TBRD_Features::TBRD_Features()
    : TReadOnlyConfig<__u8>(DataItemIds::BRD_Features, ofsFeatures)
{}

TBRD_Features::TBRD_Features(const TBytes &FromBytes)
    : TReadOnlyConfig<__u8>(FromBytes)
{
    this->DId = DataItemIds::BRD_Features;
    this->params.offset = ofsFeatures;
}

TBRD_Features::TBRD_Features(DataItemIds id, const TBytes &FromBytes)
    : TReadOnlyConfig<__u8>(id, FromBytes)
{
    this->params.offset = ofsFeatures;
}

std::string TBRD_Features::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_Features() → " + to_hex<__u8>(this->params.config);
    }
    else {
        return "BRD_Features() ? " + to_hex<__u8>(this->params.config);
    }
}

TBRD_Features &TBRD_Features::Go()
{
    this->params.config = static_cast<__u8>(in(this->params.offset));
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<__u8>(this->params.config));
    Config.features = this->params.config;
    return *this;
}

TBytes TBRD_Features::calcPayload(bool bAsReply)
{
    TBytes bytes;
    // If your code previously stuffed 4 bytes, do stuff<__u32>.
    // Otherwise, do 1 byte:
    stuff<__u32>(bytes, this->params.config);
    return bytes;
}
// ——— TBRD_Model ———————————————————————————————————————————————————————

static void trim_whitespace(char *s)
{
    char *p = s;
    char *q;

    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    q = s + strlen(s);
    while (q > s && isspace((unsigned char)q[-1])) q--;
    *q = '\0';
}

static void str_to_upper(char *s)
{
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

static bool is_valid_dac_range(const char *code)
{
    // code must already be uppercase, trimmed
    return strcmp(code, "5B")  == 0 ||
           strcmp(code, "10B") == 0 ||
           strcmp(code, "5U")  == 0 ||
           strcmp(code, "10U") == 0;
}

/**
 * Parse eNET-AI* model string to determine ADC channel count.
 *
 * Returns 0 on success, -1 on error.
 * On success, *adc_channels is set to the numeric prefix of the 2nd component:
 *   eNET-AIO16-16F-...   => 16
 *   eNET-AIO16-64MA      => 64
 *   eNET-AI12-128E       => 128
 *
 * On error, *adc_channels is set to 0 (safe "unknown" default).
 */
int enet_ai_parse_adc_channel_count(const char *model_in, __u8 *adc_channels,
                                    char *errbuf, size_t errbuf_sz)
{
    char buf[256];
    char *p;
    char *code_body;
    char *saveptr = NULL;
    char *parts0, *parts1;
    char tmp[64];
    char *end = NULL;
    long n;

    if (!model_in || !adc_channels) return -1;
    *adc_channels = 0;

    if (errbuf && errbuf_sz) errbuf[0] = '\0';

    // Copy and normalize into local buffer
    snprintf(buf, sizeof(buf), "%s", model_in);
    trim_whitespace(buf);

    p = buf;

    // Optional prefix "\000027"
    if (strncmp(p, "\\000027", 7) == 0) p += 7;

    trim_whitespace(p);

    // Must start with "eNET-AI" (case-insensitive)
    if (strncasecmp(p, "eNET-AI", 7) != 0) {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz, "Model does not start with 'eNET-AI': '%s'", p);
        return -1;
    }

    code_body = p + 7;  // after "eNET-AI"
    if (*code_body == '\0') {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz, "Model missing core after 'eNET-AI': '%s'", p);
        return -1;
    }

    // Split on '-' : parts0 = core (e.g. O16 or 16), parts1 = channel/rate block (e.g. 16F, 64MA, 128E)
    parts0 = strtok_r(code_body, "-", &saveptr);
    parts1 = strtok_r(NULL, "-", &saveptr);
    if (!parts0 || !parts1) {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz, "Not enough model components after 'eNET-AI' in '%s'", p);
        return -1;
    }

    (void)parts0; // not used here

    snprintf(tmp, sizeof(tmp), "%s", parts1);
    trim_whitespace(tmp);

    // Parse the leading integer in parts1
    n = strtol(tmp, &end, 10);
    if (end == tmp) {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz,
                     "Model missing channel count in 2nd component '%s' (expected like '16F', '64MA', '128E')",
                     tmp);
        return -1;
    }

    // Hardware can address up to 0..127 channels with submuxes (=> 128 channels max).:contentReference[oaicite:2]{index=2}
    if (n <= 0 || n > 128) {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz,
                     "Parsed channel count %ld out of range (1..128) from component '%s'",
                     n, tmp);
        return -1;
    }

    *adc_channels = (__u8)n;
    return 0;
}

/**
 * Parse eNET-AI* model string to determine DAC count.
 *
 * Returns 0 on success, -1 on error.
 * On success, *num_dacs is 0, 2, or 4.
 * On error, *num_dacs is set to -1 and errbuf (if non-NULL) gets a message.
 */
int enet_ai_parse_dac_count(const char *model_in, __u8 *num_dacs, char *errbuf, size_t errbuf_sz)
{
    char buf[256];
    char *p;
    char *code_body;
    char *saveptr = NULL;
    char *parts0, *parts1, *tok;
    bool has_dacs = false;
    bool seen_4ao = false;
    char found_range[16] = {0};

    if (!model_in || !num_dacs) return -1;
    *num_dacs = -1;

    if (errbuf && errbuf_sz) errbuf[0] = '\0';

    // Copy and normalize into local buffer
    snprintf(buf, sizeof(buf), "%s", model_in);
    trim_whitespace(buf);

    p = buf;

    // Optional prefix "\000027"
    if (strncmp(p, "\\000027", 7) == 0) p += 7;

    trim_whitespace(p);

    // Must start with "eNET-AI" (case-insensitive)
    if (strncasecmp(p, "eNET-AI", 7) != 0) {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz, "Model does not start with 'eNET-AI': '%s'", p);
        return -1;
    }

    code_body = p + 7;  // after "eNET-AI"
    if (*code_body == '\0') {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz, "Model missing core after 'eNET-AI': '%s'", p);
        return -1;
    }

    // Work in-place: split code_body on '-'
    // Example code_body: "O16-16F-5U-4AO" or "16-16A"
    parts0 = strtok_r(code_body, "-", &saveptr);
    parts1 = strtok_r(NULL, "-", &saveptr);
    if (!parts0 || !parts1) {
        if (errbuf && errbuf_sz)
            snprintf(errbuf, errbuf_sz, "Not enough model components after 'eNET-AI' in '%s'", p);
        return -1;
    }

    // DAC presence: "O" prefix => AIO (has DACs), otherwise AI-only
    // core is parts0, e.g. "O16" or "16"
    if (toupper((unsigned char)parts0[0]) == 'O') has_dacs = true;

    // Scan unordered option codes starting at parts[2]
    while ((tok = strtok_r(NULL, "-", &saveptr)) != NULL) {
        char opt[32];

        snprintf(opt, sizeof(opt), "%s", tok);
        trim_whitespace(opt);
        str_to_upper(opt);
        if (opt[0] == '\0') continue;

        if (strcmp(opt, "4AO") == 0) {
            seen_4ao = true;
            continue;
        }

        if (is_valid_dac_range(opt)) {
            if (found_range[0] == '\0') {
                snprintf(found_range, sizeof(found_range), "%s", opt);
            } else {
                if (errbuf && errbuf_sz)
                    snprintf(errbuf, errbuf_sz, "Duplicate DAC range codes '%s' and '%s'", found_range, opt);
                *num_dacs = -1;
                return -1;
            }
            continue;
        }

        // All other option codes are ignored here
    }

    // Now check for internal consistency and derive DAC count
    if (has_dacs) {
        // AIO: must have a DAC range, and 2 or 4 DACs depending on 4AO
        if (found_range[0] == '\0') {
            if (errbuf && errbuf_sz)
                snprintf(errbuf, errbuf_sz, "AIO model missing DAC range (expected -5B/-10B/-5U/-10U) in '%s'", model_in);
            *num_dacs = -1;
            return -1;
        }

        *num_dacs = seen_4ao ? 4 : 2;
        return 0;
    } else {
        // AI-only core: any DAC-specific option is a contradiction
        if (found_range[0] != '\0' || seen_4ao) {
            if (errbuf && errbuf_sz)
                snprintf(errbuf, errbuf_sz, "Model is AI-only (no 'O' in core '%s') but has DAC options (%s%s%s)",
                         parts0,
                         found_range[0] ? "range=" : "",
                         found_range[0] ? found_range : "",
                         seen_4ao ? " 4AO present" : "");
            *num_dacs = -1;
            return -1;
        }

        *num_dacs = 0;
        return 0;
    }
}

TDataItemBase &TBRD_Model::Go()
{
    // parse incoming bytes as ASCII
    std::string val(rawBytes.begin(), rawBytes.end());
    Debug("TBRD_Model::Go() received model = '" + val + "'");

    // update in-memory config
    Config.Model = val;

    // todo: Parse model string and set various cfg.parameters as needed
    char errbuf[256];
    if (enet_ai_parse_dac_count(val.c_str(), &Config.NUM_DACs, errbuf, sizeof(errbuf)) != 0) {
        Error("TBRD_Model::Go() invalid model string re: DACs '" + val + "': " + errbuf);
    }else {
        Debug("TBRD_Model::Go() parsed NUM_DACs = " + std::to_string(Config.NUM_DACs));
    }

    char errbuf2[256];
    if (enet_ai_parse_adc_channel_count(val.c_str(), &Config.adcChannels, errbuf2, sizeof(errbuf2)) != 0) {
        Error("TBRD_Model::Go() invalid model string re: ADC channel count '" + val + "': " + errbuf2);
    } else {
        Debug("TBRD_Model::Go() parsed adcChannels = " + std::to_string(Config.adcChannels));
    }

    // persist
    if (!SaveBrdConfig(CONFIG_CURRENT)) {
        Error("TBRD_Model::Go() failed to SaveConfig");
    }

    return *this;
}

TBytes TBRD_Model::calcPayload(bool bAsReply)
{
    // echo back the stored model as ASCII, no trailing NUL
    return TBytes(Config.Model.begin(), Config.Model.end());
}

std::string TBRD_Model::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_Model() → \"" + Config.Model + "\"";
    } else {
        std::string sent(rawBytes.begin(), rawBytes.end());
        return "BRD_Model(\"" + sent + "\")";
    }
}

// ——— TBRD_GetModel ————————————————————————————————————————————————————

TBytes TBRD_GetModel::calcPayload(bool /*bAsReply*/)
{
    // always reply with the current model
    return TBytes(Config.Model.begin(), Config.Model.end());
}

std::string TBRD_GetModel::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_GetModel() → \"" + Config.Model + "\"";
    } else {
        return "BRD_GetModel()";
    }
}


TBRD_GetNumberOfAdcChannels::TBRD_GetNumberOfAdcChannels(DataItemIds id, const TBytes &buf)
    : TDataItem<BRD_GetNumberOfAdcChannelsParams>(id, buf)
{
    // Accept either:
    //   - 0 bytes (typical "get" request)
    //   - 1 byte  (optional: if ever passed as a parsed payload)
    GUARD((buf.size() == 0) || (buf.size() == 1), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);

    if (buf.size() == 1) {
        this->params.numAdcChan = buf[0];
    }
}

// -------------------- TBRD_GetNumberOfAdcChannels --------------------
TBytes TBRD_GetNumberOfAdcChannels::calcPayload(bool bAsReply)
{
    (void)bAsReply;

    TBytes bytes;

    // Return the channel count as a single byte
    stuff(bytes, this->params.numAdcChan);

    Trace("TBRD_GetNumberOfAdcChannels::calcPayload built: ", bytes);
    return bytes;
}

TBRD_GetNumberOfAdcChannels &TBRD_GetNumberOfAdcChannels::Go()
{
    Trace("BRD_GetNumberOfAdcChannels Go() reading Config.adcChannels");

    // Per your note: just return Config.adcChannels
    this->params.numAdcChan = Config.adcChannels;

    return *this;
}

std::string TBRD_GetNumberOfAdcChannels::AsString(bool bAsReply)
{
    std::stringstream dest;

    // Keep it similar to the style in TADC_BaseClock
    dest << "BRD_GetNumberOfAdcChannels()"
         << (bAsReply ? " -> " : " ? ")
         << static_cast<unsigned>(Config.adcChannels);

    return dest.str();
}

// ——— TBRD_SerialNumber —————————————————————————————————————————————

TDataItemBase &TBRD_SerialNumber::Go()
{
    std::string val(rawBytes.begin(), rawBytes.end());
    Debug("TBRD_SerialNumber::Go() received serial = '" + val + "'");

    Config.SerialNumber = val;

    if (!SaveBrdConfig(CONFIG_CURRENT)) {
        Error("TBRD_SerialNumber::Go() failed to SaveConfig");
    }

    return *this;
}

TBytes TBRD_SerialNumber::calcPayload(bool /*bAsReply*/)
{
    return TBytes(Config.SerialNumber.begin(), Config.SerialNumber.end());
}

std::string TBRD_SerialNumber::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_SerialNumber() → \"" + Config.SerialNumber + "\"";
    } else {
        std::string sent(rawBytes.begin(), rawBytes.end());
        return "BRD_SerialNumber(\"" + sent + "\")";
    }
}

// ——— TBRD_GetSerialNumber ————————————————————————————————————————————

TBytes TBRD_GetSerialNumber::calcPayload(bool /*bAsReply*/)
{
    return TBytes(Config.SerialNumber.begin(), Config.SerialNumber.end());
}

std::string TBRD_GetSerialNumber::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_GetSerialNumber() → \"" + Config.SerialNumber + "\"";
    } else {
        return "BRD_GetSerialNumber()";
    }
}


// -----------------------------------------
// TBRD_GetNumberOfSubmuxes
// -----------------------------------------
TBRD_GetNumberOfSubmuxes::TBRD_GetNumberOfSubmuxes(DataItemIds id, const TBytes &buf)
  : TDataItem<NumberOfSubmuxParams>(id, buf)
{
    // ignore buf
}
TBRD_GetNumberOfSubmuxes::TBRD_GetNumberOfSubmuxes(DataItemIds id)
  : TDataItem<NumberOfSubmuxParams>(id, {})
{}

TBRD_GetNumberOfSubmuxes &TBRD_GetNumberOfSubmuxes::Go()
{
    this->params.value = Config.numberOfSubmuxes;
    return *this;
}

TBytes TBRD_GetNumberOfSubmuxes::calcPayload(bool /*bAsReply*/)
{
    TBytes out;
    out.push_back(this->params.value);
    return out;
}

std::string TBRD_GetNumberOfSubmuxes::AsString(bool bAsReply)
{
    if (bAsReply)
        return "TBRD_GetNumberOfSubmuxes() → " + std::to_string(this->params.value);
    else
        return "TBRD_GetNumberOfSubmuxes()";
}

// -----------------------------------------
// TBRD_GetSubmuxScale
// -----------------------------------------
TBRD_GetSubmuxScale::TBRD_GetSubmuxScale(DataItemIds id, const TBytes &buf)
  : TDataItem<SubmuxScaleParams>(id, buf)
{
    if (buf.size() == 2) {
        this->params.submuxIndex    = buf[0];
        this->params.gainGroupIndex = buf[1];
    } else if (buf.size() == 4) {
        this->params.value = *(float *)(&buf[0]);
    }
}

TBRD_GetSubmuxScale &TBRD_GetSubmuxScale::Go()
{
    auto s = this->params.submuxIndex;
    auto g = this->params.gainGroupIndex;
    if (s < 4 && g < 4) {
        this->params.value = Config.submuxScaleFactors[s][g];
    } else {
        this->params.value = 0.0f;
        Error("TBRD_GetSubmuxScale: index out of range");
    }
    return *this;
}

TBytes TBRD_GetSubmuxScale::calcPayload(bool bAsReply)
{
    TBytes out;
    if (bAsReply){
        const auto *p = reinterpret_cast<const __u8*>(&this->params.value);
        out.insert(out.end(), p, p + sizeof(float));
    }
    else{
        out.push_back(this->params.submuxIndex);
        out.push_back(this->params.gainGroupIndex);
    }
    return out;
}

std::string TBRD_GetSubmuxScale::AsString(bool bAsReply)
{
    std::ostringstream ss;
    ss << "TBRD_GetSubmuxScale[submux " << int(this->params.submuxIndex)
       << "][group " << int(this->params.gainGroupIndex) << "]";
    if (bAsReply)
        ss << " → " << this->params.value;
    return ss.str();
}

// -----------------------------------------
// TBRD_GetSubmuxOffset
// -----------------------------------------
TBRD_GetSubmuxOffset::TBRD_GetSubmuxOffset(DataItemIds id, const TBytes &buf)
  : TDataItem<SubmuxOffsetParams>(id, buf)
{
    if (buf.size() == 2) {
        this->params.submuxIndex    = buf[0];
        this->params.gainGroupIndex = buf[1];
    } else if (buf.size() == 4) {
        this->params.value = *(float *)(&buf[0]);
    }
}
TBRD_GetSubmuxOffset &TBRD_GetSubmuxOffset::Go()
{
    auto s = this->params.submuxIndex;
    auto g = this->params.gainGroupIndex;
    if (s < 4 &&
        g < 4)
    {
        this->params.value = Config.submuxOffsets[s][g];
    } else {
        this->params.value = 0.0f;
        Error("TBRD_GetSubmuxOffset: index out of range");
    }
    return *this;
}

TBytes TBRD_GetSubmuxOffset::calcPayload(bool bAsReply)
{
    TBytes out;
    if (bAsReply){
        const auto *p = reinterpret_cast<const __u8*>(&this->params.value);
        out.insert(out.end(), p, p + sizeof(float));
    }
    else
    {
        out.push_back(this->params.submuxIndex);
        out.push_back(this->params.gainGroupIndex);
    }
    return out;
}

std::string TBRD_GetSubmuxOffset::AsString(bool bAsReply)
{
    std::ostringstream ss;
    ss << "TBRD_GetSubmuxOffset[submux " << int(this->params.submuxIndex)
       << "][group " << int(this->params.gainGroupIndex) << "]";
    if (bAsReply)
        ss << " → " << this->params.value;
    return ss.str();
}

#pragma GCC pop_options