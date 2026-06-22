#ifndef WEBCTL_H_
#define WEBCTL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WEBCTL_API_VERSION              1u
#ifndef WEBCTL_MAX_DIO_GROUPS
#define WEBCTL_MAX_DIO_GROUPS           16u
#endif
#define WEBCTL_MAX_DIO_NOTES            4u
#define WEBCTL_MAX_ENDPOINTS            24u
#define WEBCTL_MAX_TEXT                 96u
#define WEBCTL_MAX_DETAIL               192u

#ifndef WEBCTL_RESPONSE_BUFFER_SIZE
#define WEBCTL_RESPONSE_BUFFER_SIZE      4096u
#endif

#ifndef WEBCTL_POST_BODY_MAX
#define WEBCTL_POST_BODY_MAX             1024u
#endif

typedef enum {
    WEBCTL_HTTP_GET = 0,
    WEBCTL_HTTP_HEAD = 1,
    WEBCTL_HTTP_POST = 2,
    WEBCTL_HTTP_OTHER = 255
} WebCtlHttpMethod;

typedef struct {
    WebCtlHttpMethod method;
    const char *path;
    const char *body;
    size_t body_length;
    bool authenticated;
} WebCtlRequest;

typedef struct {
    unsigned status;
    const char *status_text;
    const char *content_type;
    const char *headers;
    const unsigned char *body;
    size_t body_length;
    bool omit_body;
} WebCtlResponse;

typedef struct {
    const char *path;
    const char *content_type;
    const unsigned char *data;
    unsigned long length;
    const char *etag;
} WebCtlAsset;

typedef int (*WebCtlAssetLookupFn)(const char *path, WebCtlAsset *out, void *context);

typedef struct {
    const WebCtlAsset *assets;
    unsigned asset_count;
    WebCtlAssetLookupFn lookup;
    void *context;
} WebCtlAssetProvider;

typedef struct {
    unsigned http_status;
    char code[WEBCTL_MAX_TEXT];
    char detail[WEBCTL_MAX_DETAIL];
} WebCtlError;

typedef struct {
    char uid[32];
    char model[48];
    unsigned model_code;
    char revision[32];
    char firmware[32];
    unsigned firmware_major;
    unsigned firmware_minor;
    char comm_mode[24];
    uint64_t uptime_ms;
    uint64_t uptime_ticks;
} WebCtlUnitInfo;

typedef struct {
    uint64_t active_mask;
    bool opt[8];
    char network_mode[32];
    bool dhcp_enabled;
    bool discovery_enabled;
    bool console_enabled;
    bool update_enabled;
    uint64_t generation;
} WebCtlJumperInfo;

typedef struct {
    unsigned version;
    char phase[64];
    bool writes_enabled;
    bool auth_enabled;
} WebCtlApiInfo;

typedef struct {
    WebCtlUnitInfo unit;
    WebCtlJumperInfo jumpers;
    WebCtlApiInfo api;
} WebCtlStatusSnapshot;

typedef struct {
    bool dio_present;
    unsigned dio_bits;
    unsigned dio_group_count;
    unsigned dio_max_bits_per_group;
    bool dio_direction_configurable;
    bool dio_output_writes;

    bool network_present;
    bool network_apply;
    bool usb_present;
    bool firmware_update;
    bool https_present;
    bool certificate_storage;

    bool adc_present;
    bool adc_snapshot;
    bool adc_streaming;
    unsigned adc_channels;
    unsigned adc_resolution_bits;

    bool dac_present;
    unsigned dac_channels;

    bool diagnostics_present;
} WebCtlCapabilities;

typedef struct {
    char architecture[48];
    char mcu[48];
    char board_family[48];
    char rtos[48];
    char tcpip_stack[48];
    char crypto_stack[48];
    char filesystem[48];
    char compiler[96];
    char build_date[32];
    char build_time[32];
    char web_control[48];
    char device_shim[48];
    char transport_shim[48];
    char extra_json[512];       /* optional object members under system, no surrounding braces */
} WebCtlSystemSnapshot;

typedef struct {
    unsigned group;
    unsigned first_bit;
    unsigned bit_count;
    bool input;
    uint64_t pins;
    uint64_t output_latch;
    uint64_t bit_mask;
} WebCtlDioGroup;

typedef struct {
    unsigned group_count;
    unsigned bits;
    uint64_t input_mask;        /* I/O Group mask: 1=input */
    uint64_t output_mask;       /* I/O Group mask: 1=output */
    uint64_t physical_state;    /* DIO bit mask */
    uint64_t output_latch;      /* DIO bit mask */
    WebCtlDioGroup groups[WEBCTL_MAX_DIO_GROUPS];
} WebCtlDioSnapshot;

typedef struct {
    uint64_t mask;              /* DIO bit mask */
    uint64_t value;             /* DIO bit value; bits outside mask are ignored */
} WebCtlDioWriteRequest;

typedef struct {
    char code[WEBCTL_MAX_TEXT];
    char detail[WEBCTL_MAX_DETAIL];
    uint64_t mask;
} WebCtlNote;

typedef struct {
    uint64_t requested_mask;
    uint64_t requested_value;
    uint64_t applied_mask;
    uint64_t applied_value;
    uint64_t ignored_mask;
    uint64_t prior_physical_state;
    uint64_t after_physical_state;
    WebCtlNote notes[WEBCTL_MAX_DIO_NOTES];
    unsigned note_count;
} WebCtlDioWriteResult;

typedef struct {
    bool has_input_mask;
    bool has_output_mask;
    bool has_group;
    uint64_t input_mask;        /* I/O Group mask */
    uint64_t output_mask;       /* I/O Group mask */
    unsigned group;
    bool group_input;
} WebCtlDioDirectionRequest;

typedef struct {
    bool valid;
    bool dhcp;
    char ipv4[24];
    char netmask[24];
    char gateway[24];
    char mac[32];
} WebCtlIpConfig;

typedef struct {
    bool present;
    bool active;
    bool link;
    bool dhcp;
    char ipv4[24];
    char netmask[24];
    char gateway[24];
    char mac[32];
    uint64_t hal_state;
    uint64_t hal_error;
} WebCtlNetworkInterface;

typedef struct {
    char active_interface[24];
    char policy_mode[32];
    bool option_policy_forces_network;
    bool apply_scheduled;
    bool reverted;
    bool pending_valid;
    bool revert_available;
    WebCtlNetworkInterface ethernet;
    WebCtlNetworkInterface usb;
    WebCtlIpConfig stored;
    WebCtlIpConfig pending;
    char extra_json[512];       /* optional object members under network, no surrounding braces */
} WebCtlNetworkSnapshot;

typedef struct {
    bool netx_https_compiled;
    bool configuration_enabled;
    unsigned http_port;
    unsigned https_port;
    bool server_running;
    bool certificate_installed;
    bool private_key_installed;
    char status[48];
} WebCtlHttpsSnapshot;

typedef struct {
    bool server_certificate_installed;
    bool server_private_key_installed;
    unsigned trusted_ca_count;
    bool upload_enabled;
    char storage[48];
} WebCtlCertificateSnapshot;

typedef struct WebCtlDeviceOps {
    void *context;

    int (*get_status)(WebCtlStatusSnapshot *out, void *context);
    int (*get_capabilities)(WebCtlCapabilities *out, void *context);
    int (*get_system)(WebCtlSystemSnapshot *out, void *context);

    int (*dio_get)(WebCtlDioSnapshot *out, void *context);
    int (*dio_write_outputs)(const WebCtlDioWriteRequest *request,
                             WebCtlDioWriteResult *result,
                             WebCtlDioSnapshot *out,
                             void *context);
    int (*dio_set_direction)(const WebCtlDioDirectionRequest *request,
                             WebCtlDioSnapshot *out,
                             void *context);

    int (*network_get)(WebCtlNetworkSnapshot *out, void *context);
    int (*network_stage_json)(const char *json_body,
                              WebCtlNetworkSnapshot *out,
                              WebCtlError *error,
                              void *context);
    int (*network_apply)(WebCtlNetworkSnapshot *out, WebCtlError *error, void *context);
    int (*network_revert)(WebCtlNetworkSnapshot *out, WebCtlError *error, void *context);

    int (*https_get)(WebCtlHttpsSnapshot *out, void *context);
    int (*certificates_get)(WebCtlCertificateSnapshot *out, void *context);
    int (*certificates_post_json)(const char *json_body,
                                  WebCtlCertificateSnapshot *out,
                                  WebCtlError *error,
                                  void *context);
} WebCtlDeviceOps;

typedef struct {
    WebCtlStatusSnapshot status;
    WebCtlCapabilities capabilities;
    WebCtlSystemSnapshot system;
    WebCtlDioSnapshot dio;
    WebCtlDioWriteRequest dio_write_request;
    WebCtlDioWriteResult dio_write_result;
    WebCtlDioDirectionRequest dio_direction_request;
    WebCtlNetworkSnapshot network;
    WebCtlHttpsSnapshot https;
    WebCtlCertificateSnapshot certificates;
    WebCtlError error;
} WebCtlWork;

int WebCtl_HandleRequestWithWork(const WebCtlRequest *request,
                                 WebCtlResponse *response,
                                 const WebCtlDeviceOps *device,
                                 const WebCtlAssetProvider *assets,
                                 char *scratch,
                                 size_t scratch_size,
                                 WebCtlWork *work);

int WebCtl_HandleRequest(const WebCtlRequest *request,
                         WebCtlResponse *response,
                         const WebCtlDeviceOps *device,
                         const WebCtlAssetProvider *assets,
                         char *scratch,
                         size_t scratch_size);

int WebCtl_DefaultAssetLookup(const char *path,
                              WebCtlAsset *out,
                              const WebCtlAsset *assets,
                              unsigned asset_count);

const WebCtlAsset *WebCtl_EmbeddedAssets(unsigned *asset_count);
const char *WebCtl_StatusText(unsigned status);
bool WebCtl_RouteRequiresAuth(WebCtlHttpMethod method, const char *path);

/* Small JSON helpers intentionally support the restricted dashboard POST bodies. */
bool WebCtl_JsonGetBool(const char *json, const char *key, bool *out);
bool WebCtl_JsonGetString(const char *json, const char *key, char *out, size_t out_size);
bool WebCtl_JsonGetUint32(const char *json, const char *key, uint32_t *out);
bool WebCtl_JsonGetU64(const char *json, const char *key, uint64_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WEBCTL_H_ */
