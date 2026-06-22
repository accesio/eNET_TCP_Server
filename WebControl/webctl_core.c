#include "webctl.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define WEBCTL_CT_JSON       "application/json"
#define WEBCTL_CT_HTML       "text/html; charset=utf-8"
#define WEBCTL_API_HEADERS   "Cache-Control: no-store\r\nPragma: no-cache\r\nX-Content-Type-Options: nosniff\r\n"
#define WEBCTL_STATIC_HEADERS "Cache-Control: no-cache, max-age=0\r\nPragma: no-cache\r\nX-Content-Type-Options: nosniff\r\n"

static size_t appendf(char *buf, size_t size, size_t off, const char *fmt, ...)
{
    if (off >= size) {
        return size;
    }

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(&buf[off], size - off, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return size;
    }
    if ((size_t)n >= (size - off)) {
        return size;
    }
    return off + (size_t)n;
}

static size_t append_json_string(char *buf, size_t size, size_t off, const char *text)
{
    if (text == NULL) {
        text = "";
    }

    off = appendf(buf, size, off, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
        switch (*p) {
        case '\\': off = appendf(buf, size, off, "\\\\"); break;
        case '"':  off = appendf(buf, size, off, "\\\""); break;
        case '\b': off = appendf(buf, size, off, "\\b"); break;
        case '\f': off = appendf(buf, size, off, "\\f"); break;
        case '\n': off = appendf(buf, size, off, "\\n"); break;
        case '\r': off = appendf(buf, size, off, "\\r"); break;
        case '\t': off = appendf(buf, size, off, "\\t"); break;
        default:
            if (*p < 0x20u) {
                off = appendf(buf, size, off, "\\u%04X", (unsigned)*p);
            } else {
                off = appendf(buf, size, off, "%c", (char)*p);
            }
            break;
        }
    }
    return appendf(buf, size, off, "\"");
}

static const char *json_bool(bool v)
{
    return v ? "true" : "false";
}

static unsigned nibbles_for_bits(unsigned bits)
{
    unsigned n = (bits + 3u) / 4u;
    return (n == 0u) ? 1u : n;
}

static void format_hex(uint64_t value, unsigned min_nibbles, char *out, size_t out_size)
{
    static const char h[] = "0123456789ABCDEF";
    char tmp[2u + 16u + 1u];
    unsigned n = min_nibbles;

    if (n == 0u) {
        n = 1u;
    }
    if (n > 16u) {
        n = 16u;
    }

    tmp[0] = '0';
    tmp[1] = 'x';
    for (unsigned i = 0u; i < n; ++i) {
        const unsigned shift = (n - 1u - i) * 4u;
        tmp[2u + i] = h[(unsigned)((value >> shift) & 0xFu)];
    }
    tmp[2u + n] = '\0';

    if (out_size > 0u) {
        (void)snprintf(out, out_size, "%s", tmp);
    }
}

static void format_u64_dec(uint64_t value, char *out, size_t out_size)
{
    char tmp[20u + 1u];
    unsigned pos = sizeof(tmp) - 1u;

    if (out_size == 0u) {
        return;
    }

    tmp[pos] = '\0';
    do {
        const uint64_t q = value / 10u;
        const unsigned digit = (unsigned)(value - (q * 10u));
        tmp[--pos] = (char)('0' + digit);
        value = q;
    } while (value != 0u && pos > 0u);

    (void)snprintf(out, out_size, "%s", &tmp[pos]);
}

static size_t append_u64_dec(char *buf, size_t size, size_t off, uint64_t value)
{
    char tmp[20u + 1u];
    format_u64_dec(value, tmp, sizeof(tmp));
    return appendf(buf, size, off, "%s", tmp);
}

const char *WebCtl_StatusText(unsigned status)
{
    switch (status) {
    case 200u: return "200 OK";
    case 400u: return "400 Bad Request";
    case 401u: return "401 Unauthorized";
    case 403u: return "403 Forbidden";
    case 404u: return "404 Not Found";
    case 405u: return "405 Method Not Allowed";
    case 409u: return "409 Conflict";
    case 413u: return "413 Request Entity Too Large";
    case 500u: return "500 Internal Server Error";
    case 501u: return "501 Not Implemented";
    case 503u: return "503 Service Unavailable";
    default: return "500 Internal Server Error";
    }
}

static void response_set(WebCtlResponse *response,
                         unsigned status,
                         const char *content_type,
                         const char *headers,
                         const unsigned char *body,
                         size_t body_length,
                         bool omit_body)
{
    response->status = status;
    response->status_text = WebCtl_StatusText(status);
    response->content_type = content_type;
    response->headers = headers;
    response->body = body;
    response->body_length = body_length;
    response->omit_body = omit_body;
}

static int error_response(WebCtlResponse *response,
                          char *scratch,
                          size_t scratch_size,
                          unsigned status,
                          const char *code,
                          const char *detail,
                          bool omit_body)
{
    if (scratch == NULL || scratch_size == 0u) {
        return -1;
    }
    size_t off = 0u;
    off = appendf(scratch, scratch_size, off, "{\r\n  \"error\": ");
    off = append_json_string(scratch, scratch_size, off, code ? code : "error");
    off = appendf(scratch, scratch_size, off, ",\r\n  \"detail\": ");
    off = append_json_string(scratch, scratch_size, off, detail ? detail : "");
    off = appendf(scratch, scratch_size, off, "\r\n}\r\n");

    if (off >= scratch_size) {
        static const unsigned char fallback[] = "{\"error\":\"response_too_large\"}\r\n";
        response_set(response, 500u, WEBCTL_CT_JSON, WEBCTL_API_HEADERS,
                     fallback, sizeof(fallback) - 1u, omit_body);
        return 0;
    }

    response_set(response, status, WEBCTL_CT_JSON, WEBCTL_API_HEADERS,
                 (const unsigned char *)scratch, off, omit_body);
    return 0;
}

static bool path_match(const char *request_path, const char *route)
{
    if (request_path == NULL || route == NULL) {
        return false;
    }
    size_t route_len = strlen(route);
    if (strncmp(request_path, route, route_len) != 0) {
        return false;
    }
    char next = request_path[route_len];
    return next == '\0' || next == '?';
}

static void path_without_query(const char *path, char *out, size_t out_size)
{
    if (out_size == 0u) {
        return;
    }
    if (path == NULL || path[0] == '\0') {
        path = "/";
    }
    size_t n = 0u;
    while (path[n] != '\0' && path[n] != '?' && n + 1u < out_size) {
        out[n] = path[n];
        ++n;
    }
    out[n] = '\0';
}

bool WebCtl_RouteRequiresAuth(WebCtlHttpMethod method, const char *path)
{
    if (method != WEBCTL_HTTP_POST) {
        return false;
    }
    return path_match(path, "/api/v1/io/outputs") ||
           path_match(path, "/api/v1/io/direction") ||
           path_match(path, "/api/v1/network/pending") ||
           path_match(path, "/api/v1/network/apply") ||
           path_match(path, "/api/v1/network/revert") ||
           path_match(path, "/api/v1/certificates");
}

int WebCtl_DefaultAssetLookup(const char *path,
                              WebCtlAsset *out,
                              const WebCtlAsset *assets,
                              unsigned asset_count)
{
    char normalized[160];
    path_without_query(path, normalized, sizeof(normalized));
    if (strcmp(normalized, "/") == 0) {
        (void)snprintf(normalized, sizeof(normalized), "%s", "/index.html");
    }

    if (strstr(normalized, "..") != NULL) {
        return 0;
    }

    for (unsigned i = 0u; i < asset_count; ++i) {
        if (assets[i].path != NULL && strcmp(normalized, assets[i].path) == 0) {
            if (out != NULL) {
                *out = assets[i];
            }
            return 1;
        }
    }
    return 0;
}

static int lookup_asset(const WebCtlAssetProvider *provider, const char *path, WebCtlAsset *out)
{
    if (provider == NULL) {
        return 0;
    }
    if (provider->lookup != NULL) {
        return provider->lookup(path, out, provider->context);
    }
    return WebCtl_DefaultAssetLookup(path, out, provider->assets, provider->asset_count);
}

static size_t append_capabilities_object(char *buf, size_t size, size_t off, const WebCtlCapabilities *c, const char *indent)
{
    off = appendf(buf, size, off, "%s{\r\n", indent);
    off = appendf(buf, size, off, "%s  \"dio\": { \"present\": %s, \"bits\": %u, \"group_count\": %u, \"max_bits_per_group\": %u, \"direction_configurable\": %s, \"output_writes\": %s },\r\n",
                  indent, json_bool(c->dio_present), c->dio_bits, c->dio_group_count,
                  c->dio_max_bits_per_group, json_bool(c->dio_direction_configurable), json_bool(c->dio_output_writes));
    off = appendf(buf, size, off, "%s  \"network\": { \"present\": %s, \"apply\": %s, \"usb_present\": %s },\r\n",
                  indent, json_bool(c->network_present), json_bool(c->network_apply), json_bool(c->usb_present));
    off = appendf(buf, size, off, "%s  \"http\": { \"api_version\": %u, \"https\": %s, \"certificate_storage\": %s },\r\n",
                  indent, WEBCTL_API_VERSION, json_bool(c->https_present), json_bool(c->certificate_storage));
    off = appendf(buf, size, off, "%s  \"adc\": { \"present\": %s, \"snapshot\": %s, \"streaming\": %s, \"channels\": %u, \"resolution_bits\": %u },\r\n",
                  indent, json_bool(c->adc_present), json_bool(c->adc_snapshot), json_bool(c->adc_streaming), c->adc_channels, c->adc_resolution_bits);
    off = appendf(buf, size, off, "%s  \"dac\": { \"present\": %s, \"channels\": %u },\r\n",
                  indent, json_bool(c->dac_present), c->dac_channels);
    off = appendf(buf, size, off, "%s  \"diagnostics\": { \"present\": %s },\r\n",
                  indent, json_bool(c->diagnostics_present));
    off = appendf(buf, size, off, "%s  \"firmware_update\": %s\r\n", indent, json_bool(c->firmware_update));
    off = appendf(buf, size, off, "%s}", indent);
    return off;
}

static size_t build_capabilities_json(char *buf, size_t size, const WebCtlCapabilities *c)
{
    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"capabilities\": ");
    off = append_capabilities_object(buf, size, off, c, "  ");
    off = appendf(buf, size, off, "\r\n}\r\n");
    return off;
}

static size_t build_status_json(char *buf, size_t size, const WebCtlStatusSnapshot *s, const WebCtlCapabilities *c)
{
    char active_mask[20];
    format_hex(s->jumpers.active_mask, 2u, active_mask, sizeof(active_mask));

    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"unit\": {\r\n");
    off = appendf(buf, size, off, "    \"uid\": "); off = append_json_string(buf, size, off, s->unit.uid); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"model\": "); off = append_json_string(buf, size, off, s->unit.model); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"model_code\": %u,\r\n", s->unit.model_code);
    off = appendf(buf, size, off, "    \"revision\": "); off = append_json_string(buf, size, off, s->unit.revision); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"firmware\": "); off = append_json_string(buf, size, off, s->unit.firmware); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"firmware_major\": %u,\r\n", s->unit.firmware_major);
    off = appendf(buf, size, off, "    \"firmware_minor\": %u,\r\n", s->unit.firmware_minor);
    off = appendf(buf, size, off, "    \"comm_mode\": "); off = append_json_string(buf, size, off, s->unit.comm_mode); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"uptime_ms\": "); off = append_u64_dec(buf, size, off, s->unit.uptime_ms); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"uptime_ticks\": "); off = append_u64_dec(buf, size, off, s->unit.uptime_ticks); off = appendf(buf, size, off, "\r\n");
    off = appendf(buf, size, off, "  },\r\n  \"jumpers\": {\r\n");
    off = appendf(buf, size, off, "    \"active_mask\": \"%s\",\r\n", active_mask);
    for (unsigned i = 0u; i < 4u; ++i) {
        off = appendf(buf, size, off, "    \"opt%u\": %s,\r\n", i, json_bool(s->jumpers.opt[i]));
    }
    off = appendf(buf, size, off, "    \"network_mode\": "); off = append_json_string(buf, size, off, s->jumpers.network_mode); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"dhcp_enabled\": %s,\r\n", json_bool(s->jumpers.dhcp_enabled));
    off = appendf(buf, size, off, "    \"discovery_enabled\": %s,\r\n", json_bool(s->jumpers.discovery_enabled));
    off = appendf(buf, size, off, "    \"console_enabled\": %s,\r\n", json_bool(s->jumpers.console_enabled));
    off = appendf(buf, size, off, "    \"update_enabled\": %s,\r\n", json_bool(s->jumpers.update_enabled));
    off = appendf(buf, size, off, "    \"generation\": "); off = append_u64_dec(buf, size, off, s->jumpers.generation); off = appendf(buf, size, off, "\r\n");
    off = appendf(buf, size, off, "  },\r\n  \"api\": {\r\n");
    off = appendf(buf, size, off, "    \"version\": %u,\r\n", WEBCTL_API_VERSION);
    off = appendf(buf, size, off, "    \"phase\": "); off = append_json_string(buf, size, off, s->api.phase); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"writes_enabled\": %s,\r\n", json_bool(s->api.writes_enabled));
    off = appendf(buf, size, off, "    \"auth_enabled\": %s,\r\n", json_bool(s->api.auth_enabled));
    off = appendf(buf, size, off, "    \"endpoints\": [\"/api/v1/status\", \"/api/v1/capabilities\", \"/api/v1/system\", \"/api/v1/io\", \"/api/v1/network\", \"/api/v1/https\", \"/api/v1/certificates\"]\r\n");
    off = appendf(buf, size, off, "  },\r\n  \"capabilities\": ");
    off = append_capabilities_object(buf, size, off, c, "  ");
    off = appendf(buf, size, off, "\r\n}\r\n");
    return off;
}

static size_t build_system_json(char *buf, size_t size, const WebCtlSystemSnapshot *s)
{
    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"system\": {\r\n");
    off = appendf(buf, size, off, "    \"architecture\": "); off = append_json_string(buf, size, off, s->architecture); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"mcu\": "); off = append_json_string(buf, size, off, s->mcu); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"board_family\": "); off = append_json_string(buf, size, off, s->board_family); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"rtos\": "); off = append_json_string(buf, size, off, s->rtos); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"tcpip_stack\": "); off = append_json_string(buf, size, off, s->tcpip_stack); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"crypto_stack\": "); off = append_json_string(buf, size, off, s->crypto_stack); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"filesystem\": "); off = append_json_string(buf, size, off, s->filesystem); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"compiler\": "); off = append_json_string(buf, size, off, s->compiler); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"build_date\": "); off = append_json_string(buf, size, off, s->build_date); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"build_time\": "); off = append_json_string(buf, size, off, s->build_time); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"web_control\": "); off = append_json_string(buf, size, off, s->web_control); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"device_shim\": "); off = append_json_string(buf, size, off, s->device_shim); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"transport_shim\": "); off = append_json_string(buf, size, off, s->transport_shim);
    if (s->extra_json[0] != '\0') {
        off = appendf(buf, size, off, ",\r\n%s", s->extra_json);
    }
    off = appendf(buf, size, off, "\r\n  }\r\n}\r\n");
    return off;
}

static size_t append_dio_object(char *buf, size_t size, size_t off, const WebCtlDioSnapshot *d, const char *indent)
{
    char tmp[40];
    unsigned group_nibbles = nibbles_for_bits(d->group_count);
    unsigned bit_nibbles = nibbles_for_bits(d->bits);

    off = appendf(buf, size, off, "%s{\r\n", indent);
    off = appendf(buf, size, off, "%s  \"group_count\": %u,\r\n", indent, d->group_count);
    off = appendf(buf, size, off, "%s  \"bits\": %u,\r\n", indent, d->bits);
    format_hex(d->input_mask, group_nibbles, tmp, sizeof(tmp));
    off = appendf(buf, size, off, "%s  \"input_mask\": \"%s\",\r\n", indent, tmp);
    format_hex(d->output_mask, group_nibbles, tmp, sizeof(tmp));
    off = appendf(buf, size, off, "%s  \"output_mask\": \"%s\",\r\n", indent, tmp);
    format_hex(d->physical_state, bit_nibbles, tmp, sizeof(tmp));
    off = appendf(buf, size, off, "%s  \"physical_state\": \"%s\",\r\n", indent, tmp);
    format_hex(d->output_latch, bit_nibbles, tmp, sizeof(tmp));
    off = appendf(buf, size, off, "%s  \"output_latch\": \"%s\",\r\n", indent, tmp);
    off = appendf(buf, size, off, "%s  \"groups\": [\r\n", indent);

    for (unsigned i = 0u; i < d->group_count && i < WEBCTL_MAX_DIO_GROUPS; ++i) {
        const WebCtlDioGroup *g = &d->groups[i];
        char mask[40];
        char pins[40];
        char latch[40];
        unsigned local_nibbles = nibbles_for_bits(g->bit_count);
        format_hex(g->bit_mask, bit_nibbles, mask, sizeof(mask));
        format_hex(g->pins, local_nibbles, pins, sizeof(pins));
        format_hex(g->output_latch, local_nibbles, latch, sizeof(latch));
        off = appendf(buf, size, off,
                      "%s    { \"group\": %u, \"direction\": \"%s\", \"first_bit\": %u, \"bit_count\": %u, \"mask\": \"%s\", \"pins\": \"%s\", \"output_latch\": \"%s\" }%s\r\n",
                      indent, g->group, g->input ? "input" : "output", g->first_bit, g->bit_count,
                      mask, pins, latch, (i + 1u == d->group_count) ? "" : ",");
    }
    off = appendf(buf, size, off, "%s  ]\r\n", indent);
    off = appendf(buf, size, off, "%s}", indent);
    return off;
}

static size_t build_io_json(char *buf, size_t size, const WebCtlDioSnapshot *d)
{
    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"dio\": ");
    off = append_dio_object(buf, size, off, d, "  ");
    off = appendf(buf, size, off, "\r\n}\r\n");
    return off;
}

static size_t build_write_result_json(char *buf, size_t size, const WebCtlDioWriteResult *r, const WebCtlDioSnapshot *d)
{
    const unsigned bit_nibbles = nibbles_for_bits(d->bits);
    char mask[40], value[40], applied_mask[40], applied_value[40], ignored_mask[40], prior[40], after[40];
    format_hex(r->requested_mask, bit_nibbles, mask, sizeof(mask));
    format_hex(r->requested_value, bit_nibbles, value, sizeof(value));
    format_hex(r->applied_mask, bit_nibbles, applied_mask, sizeof(applied_mask));
    format_hex(r->applied_value, bit_nibbles, applied_value, sizeof(applied_value));
    format_hex(r->ignored_mask, bit_nibbles, ignored_mask, sizeof(ignored_mask));
    format_hex(r->prior_physical_state, bit_nibbles, prior, sizeof(prior));
    format_hex(r->after_physical_state, bit_nibbles, after, sizeof(after));

    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"result\": \"ok\",\r\n");
    off = appendf(buf, size, off, "  \"mask\": \"%s\",\r\n", mask);
    off = appendf(buf, size, off, "  \"value\": \"%s\",\r\n", value);
    off = appendf(buf, size, off, "  \"applied_mask\": \"%s\",\r\n", applied_mask);
    off = appendf(buf, size, off, "  \"applied_value\": \"%s\",\r\n", applied_value);
    off = appendf(buf, size, off, "  \"ignored_mask\": \"%s\",\r\n", ignored_mask);
    off = appendf(buf, size, off, "  \"prior_physical_state\": \"%s\",\r\n", prior);
    off = appendf(buf, size, off, "  \"after_physical_state\": \"%s\",\r\n", after);
    off = appendf(buf, size, off, "  \"notes\": [");
    if (r->note_count != 0u) {
        off = appendf(buf, size, off, "\r\n");
    }
    for (unsigned i = 0u; i < r->note_count && i < WEBCTL_MAX_DIO_NOTES; ++i) {
        char note_mask[40];
        format_hex(r->notes[i].mask, bit_nibbles, note_mask, sizeof(note_mask));
        off = appendf(buf, size, off, "    { \"code\": ");
        off = append_json_string(buf, size, off, r->notes[i].code);
        off = appendf(buf, size, off, ", \"mask\": \"%s\", \"detail\": ", note_mask);
        off = append_json_string(buf, size, off, r->notes[i].detail);
        off = appendf(buf, size, off, " }%s\r\n", (i + 1u == r->note_count) ? "" : ",");
    }
    off = appendf(buf, size, off, "  ],\r\n  \"dio\": ");
    off = append_dio_object(buf, size, off, d, "  ");
    off = appendf(buf, size, off, "\r\n}\r\n");
    return off;
}

static size_t append_ip_config(char *buf, size_t size, size_t off, const char *name, const WebCtlIpConfig *cfg, const char *indent, bool comma)
{
    off = appendf(buf, size, off, "%s\"%s\": {\r\n", indent, name);
    off = appendf(buf, size, off, "%s  \"dhcp\": %s,\r\n", indent, json_bool(cfg->dhcp));
    off = appendf(buf, size, off, "%s  \"ipv4\": ", indent); off = append_json_string(buf, size, off, cfg->ipv4); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"netmask\": ", indent); off = append_json_string(buf, size, off, cfg->netmask); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"gateway\": ", indent); off = append_json_string(buf, size, off, cfg->gateway); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"mac\": ", indent); off = append_json_string(buf, size, off, cfg->mac); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"valid\": %s\r\n", indent, json_bool(cfg->valid));
    off = appendf(buf, size, off, "%s}%s\r\n", indent, comma ? "," : "");
    return off;
}

static size_t append_network_interface(char *buf, size_t size, size_t off, const char *name, const WebCtlNetworkInterface *n, const char *indent, bool comma)
{
    off = appendf(buf, size, off, "%s\"%s\": {\r\n", indent, name);
    off = appendf(buf, size, off, "%s  \"present\": %s,\r\n", indent, json_bool(n->present));
    off = appendf(buf, size, off, "%s  \"active\": %s,\r\n", indent, json_bool(n->active));
    off = appendf(buf, size, off, "%s  \"link\": %s,\r\n", indent, json_bool(n->link));
    off = appendf(buf, size, off, "%s  \"dhcp\": %s,\r\n", indent, json_bool(n->dhcp));
    off = appendf(buf, size, off, "%s  \"ipv4\": ", indent); off = append_json_string(buf, size, off, n->ipv4); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"netmask\": ", indent); off = append_json_string(buf, size, off, n->netmask); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"gateway\": ", indent); off = append_json_string(buf, size, off, n->gateway); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"mac\": ", indent); off = append_json_string(buf, size, off, n->mac); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"hal_state\": ", indent); off = append_u64_dec(buf, size, off, n->hal_state); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "%s  \"hal_error\": ", indent); off = append_u64_dec(buf, size, off, n->hal_error); off = appendf(buf, size, off, "\r\n");
    off = appendf(buf, size, off, "%s}%s\r\n", indent, comma ? "," : "");
    return off;
}

static size_t build_network_json(char *buf, size_t size, const WebCtlNetworkSnapshot *n)
{
    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"network\": {\r\n");
    off = appendf(buf, size, off, "    \"active_interface\": "); off = append_json_string(buf, size, off, n->active_interface); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"policy_mode\": "); off = append_json_string(buf, size, off, n->policy_mode); off = appendf(buf, size, off, ",\r\n");
    off = appendf(buf, size, off, "    \"option_policy_forces_network\": %s,\r\n", json_bool(n->option_policy_forces_network));
    off = appendf(buf, size, off, "    \"apply_scheduled\": %s,\r\n", json_bool(n->apply_scheduled));
    off = appendf(buf, size, off, "    \"reverted\": %s,\r\n", json_bool(n->reverted));
    off = append_network_interface(buf, size, off, "ethernet", &n->ethernet, "    ", true);
    off = append_network_interface(buf, size, off, "usb", &n->usb, "    ", true);
    off = append_ip_config(buf, size, off, "stored", &n->stored, "    ", true);
    if (n->pending_valid) {
        off = append_ip_config(buf, size, off, "pending", &n->pending, "    ", true);
    } else {
        off = appendf(buf, size, off, "    \"pending\": { \"valid\": false },\r\n");
    }
    off = appendf(buf, size, off, "    \"revert_available\": %s", json_bool(n->revert_available));
    if (n->extra_json[0] != '\0') {
        off = appendf(buf, size, off, ",\r\n%s", n->extra_json);
    }
    off = appendf(buf, size, off, "\r\n  }\r\n}\r\n");
    return off;
}

static size_t build_https_json(char *buf, size_t size, const WebCtlHttpsSnapshot *h)
{
    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"https\": {\r\n");
    off = appendf(buf, size, off, "    \"netx_https_compiled\": %s,\r\n", json_bool(h->netx_https_compiled));
    off = appendf(buf, size, off, "    \"configuration_enabled\": %s,\r\n", json_bool(h->configuration_enabled));
    off = appendf(buf, size, off, "    \"http_port\": %u,\r\n", h->http_port);
    off = appendf(buf, size, off, "    \"https_port\": %u,\r\n", h->https_port);
    off = appendf(buf, size, off, "    \"server_running\": %s,\r\n", json_bool(h->server_running));
    off = appendf(buf, size, off, "    \"certificate_installed\": %s,\r\n", json_bool(h->certificate_installed));
    off = appendf(buf, size, off, "    \"private_key_installed\": %s,\r\n", json_bool(h->private_key_installed));
    off = appendf(buf, size, off, "    \"status\": "); off = append_json_string(buf, size, off, h->status); off = appendf(buf, size, off, "\r\n  }\r\n}\r\n");
    return off;
}

static size_t build_certificates_json(char *buf, size_t size, const WebCtlCertificateSnapshot *c)
{
    size_t off = 0u;
    off = appendf(buf, size, off, "{\r\n  \"certificates\": {\r\n");
    off = appendf(buf, size, off, "    \"server_certificate_installed\": %s,\r\n", json_bool(c->server_certificate_installed));
    off = appendf(buf, size, off, "    \"server_private_key_installed\": %s,\r\n", json_bool(c->server_private_key_installed));
    off = appendf(buf, size, off, "    \"trusted_ca_count\": %u,\r\n", c->trusted_ca_count);
    off = appendf(buf, size, off, "    \"upload_enabled\": %s,\r\n", json_bool(c->upload_enabled));
    off = appendf(buf, size, off, "    \"storage\": "); off = append_json_string(buf, size, off, c->storage); off = appendf(buf, size, off, "\r\n  }\r\n}\r\n");
    return off;
}

static int send_built_json(WebCtlResponse *response, char *scratch, size_t scratch_size, size_t length, bool omit_body)
{
    if (length >= scratch_size) {
        return error_response(response, scratch, scratch_size, 500u, "response_too_large", "JSON response buffer overflow", omit_body);
    }
    response_set(response, 200u, WEBCTL_CT_JSON, WEBCTL_API_HEADERS, (const unsigned char *)scratch, length, omit_body);
    return 0;
}

static int device_error(WebCtlResponse *response, char *scratch, size_t scratch_size, const WebCtlError *err, const char *fallback_code, const char *fallback_detail, bool omit_body)
{
    unsigned status = 503u;
    const char *code = fallback_code;
    const char *detail = fallback_detail;
    if (err != NULL) {
        if (err->http_status != 0u) {
            status = err->http_status;
        }
        if (err->code[0] != '\0') {
            code = err->code;
        }
        if (err->detail[0] != '\0') {
            detail = err->detail;
        }
    }
    return error_response(response, scratch, scratch_size, status, code, detail, omit_body);
}

static void default_https(WebCtlHttpsSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    out->http_port = 80u;
    out->https_port = 443u;
    (void)snprintf(out->status, sizeof(out->status), "%s", "not_configured");
}

static void default_certs(WebCtlCertificateSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    (void)snprintf(out->storage, sizeof(out->storage), "%s", "not_implemented");
}

static WebCtlWork s_default_work;

int WebCtl_HandleRequestWithWork(const WebCtlRequest *request,
                                 WebCtlResponse *response,
                                 const WebCtlDeviceOps *device,
                                 const WebCtlAssetProvider *assets,
                                 char *scratch,
                                 size_t scratch_size,
                                 WebCtlWork *work)
{
    if (request == NULL || response == NULL || scratch == NULL || scratch_size == 0u) {
        return -1;
    }
    if (work == NULL) {
        work = &s_default_work;
    }

    const bool omit_body = (request->method == WEBCTL_HTTP_HEAD);
    const char *path = (request->path != NULL) ? request->path : "/";

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/status")) {
        if (device == NULL || device->get_status == NULL || device->get_capabilities == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "status endpoint not implemented", omit_body);
        }
        WebCtlStatusSnapshot *st = &work->status;
        WebCtlCapabilities *cap = &work->capabilities;
        memset(st, 0, sizeof(*st));
        memset(cap, 0, sizeof(*cap));
        if (device->get_status(st, device->context) != 0 || device->get_capabilities(cap, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "status_unavailable", "device status unavailable", omit_body);
        }
        if (st->api.version == 0u) {
            st->api.version = WEBCTL_API_VERSION;
        }
        return send_built_json(response, scratch, scratch_size, build_status_json(scratch, scratch_size, st, cap), omit_body);
    }

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/capabilities")) {
        if (device == NULL || device->get_capabilities == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "capabilities endpoint not implemented", omit_body);
        }
        WebCtlCapabilities *cap = &work->capabilities;
        memset(cap, 0, sizeof(*cap));
        if (device->get_capabilities(cap, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "capabilities_unavailable", "device capabilities unavailable", omit_body);
        }
        return send_built_json(response, scratch, scratch_size, build_capabilities_json(scratch, scratch_size, cap), omit_body);
    }

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/system")) {
        if (device == NULL || device->get_system == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "system endpoint not implemented", omit_body);
        }
        WebCtlSystemSnapshot *sys = &work->system;
        memset(sys, 0, sizeof(*sys));
        if (device->get_system(sys, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "system_unavailable", "system information unavailable", omit_body);
        }
        return send_built_json(response, scratch, scratch_size, build_system_json(scratch, scratch_size, sys), omit_body);
    }

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/io")) {
        if (device == NULL || device->dio_get == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "DIO endpoint not implemented", omit_body);
        }
        WebCtlDioSnapshot *dio = &work->dio;
        memset(dio, 0, sizeof(*dio));
        if (device->dio_get(dio, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "dio_unavailable", "DIO snapshot unavailable", omit_body);
        }
        return send_built_json(response, scratch, scratch_size, build_io_json(scratch, scratch_size, dio), omit_body);
    }

    if (request->method == WEBCTL_HTTP_POST && path_match(path, "/api/v1/io/outputs")) {
        if (device == NULL || device->dio_write_outputs == NULL || device->get_capabilities == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "DIO output writes not implemented", false);
        }
        WebCtlCapabilities *cap = &work->capabilities;
        memset(cap, 0, sizeof(*cap));
        if (device->get_capabilities(cap, device->context) != 0 || !cap->dio_output_writes) {
            return error_response(response, scratch, scratch_size, 403u, "writes_disabled", "DIO output writes are disabled", false);
        }
        WebCtlDioWriteRequest *wr = &work->dio_write_request;
        memset(wr, 0, sizeof(*wr));
        if (!WebCtl_JsonGetU64(request->body, "mask", &wr->mask) || !WebCtl_JsonGetU64(request->body, "value", &wr->value)) {
            return error_response(response, scratch, scratch_size, 400u, "invalid_json", "expected JSON fields: mask and value", false);
        }
        if (cap->dio_bits < 64u && (wr->mask & (~UINT64_C(0) << cap->dio_bits)) != 0u) {
            return error_response(response, scratch, scratch_size, 400u, "range_error", "mask selects bits outside this device's DIO range", false);
        }
        WebCtlDioWriteResult *result = &work->dio_write_result;
        WebCtlDioSnapshot *dio = &work->dio;
        memset(result, 0, sizeof(*result));
        memset(dio, 0, sizeof(*dio));
        if (device->dio_write_outputs(wr, result, dio, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "dio_write_failed", "DIO output write failed", false);
        }
        return send_built_json(response, scratch, scratch_size, build_write_result_json(scratch, scratch_size, result, dio), false);
    }

    if (request->method == WEBCTL_HTTP_POST && path_match(path, "/api/v1/io/direction")) {
        if (device == NULL || device->dio_set_direction == NULL || device->get_capabilities == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "DIO direction writes not implemented", false);
        }
        WebCtlCapabilities *cap = &work->capabilities;
        memset(cap, 0, sizeof(*cap));
        if (device->get_capabilities(cap, device->context) != 0 || !cap->dio_direction_configurable) {
            return error_response(response, scratch, scratch_size, 403u, "writes_disabled", "DIO direction writes are disabled", false);
        }
        WebCtlDioDirectionRequest *dr = &work->dio_direction_request;
        memset(dr, 0, sizeof(*dr));
        uint64_t tmp64 = 0u;
        uint32_t tmp32 = 0u;
        char direction[16];
        if (WebCtl_JsonGetU64(request->body, "input_mask", &tmp64)) {
            dr->has_input_mask = true;
            dr->input_mask = tmp64;
        } else if (WebCtl_JsonGetU64(request->body, "output_mask", &tmp64)) {
            dr->has_output_mask = true;
            dr->output_mask = tmp64;
        } else if (WebCtl_JsonGetUint32(request->body, "group", &tmp32) && WebCtl_JsonGetString(request->body, "direction", direction, sizeof(direction))) {
            dr->has_group = true;
            dr->group = tmp32;
            if (strcmp(direction, "input") == 0 || strcmp(direction, "in") == 0) {
                dr->group_input = true;
            } else if (strcmp(direction, "output") == 0 || strcmp(direction, "out") == 0) {
                dr->group_input = false;
            } else {
                return error_response(response, scratch, scratch_size, 400u, "bad_direction", "direction must be input or output", false);
            }
        } else {
            return error_response(response, scratch, scratch_size, 400u, "invalid_json", "expected input_mask, output_mask, or group/direction", false);
        }
        if (dr->has_group && dr->group >= cap->dio_group_count) {
            return error_response(response, scratch, scratch_size, 400u, "bad_group", "group is outside this device's I/O Group range", false);
        }
        if (cap->dio_group_count < 64u) {
            uint64_t valid = (UINT64_C(1) << cap->dio_group_count) - 1u;
            if ((dr->has_input_mask && (dr->input_mask & ~valid) != 0u) ||
                (dr->has_output_mask && (dr->output_mask & ~valid) != 0u)) {
                return error_response(response, scratch, scratch_size, 400u, "bad_group_mask", "direction mask selects unavailable I/O Groups", false);
            }
        }
        WebCtlDioSnapshot *dio = &work->dio;
        memset(dio, 0, sizeof(*dio));
        if (device->dio_set_direction(dr, dio, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "dio_config_failed", "DIO direction update failed", false);
        }
        return send_built_json(response, scratch, scratch_size, build_io_json(scratch, scratch_size, dio), false);
    }

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/network")) {
        if (device == NULL || device->network_get == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "network endpoint not implemented", omit_body);
        }
        WebCtlNetworkSnapshot *net = &work->network;
        memset(net, 0, sizeof(*net));
        if (device->network_get(net, device->context) != 0) {
            return error_response(response, scratch, scratch_size, 503u, "network_unavailable", "network information unavailable", omit_body);
        }
        return send_built_json(response, scratch, scratch_size, build_network_json(scratch, scratch_size, net), omit_body);
    }

    if (request->method == WEBCTL_HTTP_POST && path_match(path, "/api/v1/network/pending")) {
        if (device == NULL || device->network_stage_json == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "network staging not implemented", false);
        }
        WebCtlNetworkSnapshot *net = &work->network;
        WebCtlError *err = &work->error;
        memset(net, 0, sizeof(*net));
        memset(err, 0, sizeof(*err));
        if (device->network_stage_json(request->body, net, err, device->context) != 0) {
            return device_error(response, scratch, scratch_size, err, "network_stage_failed", "network staging failed", false);
        }
        return send_built_json(response, scratch, scratch_size, build_network_json(scratch, scratch_size, net), false);
    }

    if (request->method == WEBCTL_HTTP_POST && path_match(path, "/api/v1/network/apply")) {
        if (device == NULL || device->network_apply == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "network apply not implemented", false);
        }
        WebCtlNetworkSnapshot *net = &work->network;
        WebCtlError *err = &work->error;
        memset(net, 0, sizeof(*net));
        memset(err, 0, sizeof(*err));
        if (device->network_apply(net, err, device->context) != 0) {
            return device_error(response, scratch, scratch_size, err, "network_apply_failed", "network apply failed", false);
        }
        return send_built_json(response, scratch, scratch_size, build_network_json(scratch, scratch_size, net), false);
    }

    if (request->method == WEBCTL_HTTP_POST && path_match(path, "/api/v1/network/revert")) {
        if (device == NULL || device->network_revert == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "not_supported", "network revert not implemented", false);
        }
        WebCtlNetworkSnapshot *net = &work->network;
        WebCtlError *err = &work->error;
        memset(net, 0, sizeof(*net));
        memset(err, 0, sizeof(*err));
        if (device->network_revert(net, err, device->context) != 0) {
            return device_error(response, scratch, scratch_size, err, "network_revert_failed", "network revert failed", false);
        }
        return send_built_json(response, scratch, scratch_size, build_network_json(scratch, scratch_size, net), false);
    }

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/https")) {
        WebCtlHttpsSnapshot *https = &work->https;
        default_https(https);
        if (device != NULL && device->https_get != NULL) {
            (void)device->https_get(https, device->context);
        }
        return send_built_json(response, scratch, scratch_size, build_https_json(scratch, scratch_size, https), omit_body);
    }

    if ((request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) && path_match(path, "/api/v1/certificates")) {
        WebCtlCertificateSnapshot *certs = &work->certificates;
        default_certs(certs);
        if (device != NULL && device->certificates_get != NULL) {
            (void)device->certificates_get(certs, device->context);
        }
        return send_built_json(response, scratch, scratch_size, build_certificates_json(scratch, scratch_size, certs), omit_body);
    }

    if (request->method == WEBCTL_HTTP_POST && path_match(path, "/api/v1/certificates")) {
        if (device == NULL || device->certificates_post_json == NULL) {
            return error_response(response, scratch, scratch_size, 501u, "certificate_storage_not_implemented", "certificate upload endpoints are reserved for the HTTPS implementation phase", false);
        }
        WebCtlCertificateSnapshot *certs = &work->certificates;
        WebCtlError *err = &work->error;
        memset(certs, 0, sizeof(*certs));
        memset(err, 0, sizeof(*err));
        if (device->certificates_post_json(request->body, certs, err, device->context) != 0) {
            return device_error(response, scratch, scratch_size, err, "certificate_storage_failed", "certificate storage failed", false);
        }
        return send_built_json(response, scratch, scratch_size, build_certificates_json(scratch, scratch_size, certs), false);
    }

    if (path_match(path, "/api/v1/status") || path_match(path, "/api/v1/capabilities") || path_match(path, "/api/v1/system") ||
        path_match(path, "/api/v1/io") || path_match(path, "/api/v1/network") || path_match(path, "/api/v1/https") ||
        path_match(path, "/api/v1/certificates")) {
        return error_response(response, scratch, scratch_size, 405u, "method_not_allowed", "method not allowed for this resource", omit_body);
    }

    if (request->method == WEBCTL_HTTP_GET || request->method == WEBCTL_HTTP_HEAD) {
        WebCtlAsset asset;
        memset(&asset, 0, sizeof(asset));
        if (lookup_asset(assets, path, &asset)) {
            response_set(response, 200u,
                         asset.content_type ? asset.content_type : WEBCTL_CT_HTML,
                         WEBCTL_STATIC_HEADERS,
                         asset.data,
                         (size_t)asset.length,
                         omit_body);
            return 0;
        }
        return error_response(response, scratch, scratch_size, 404u, "not_found", "resource not found", omit_body);
    }

    return error_response(response, scratch, scratch_size, 405u, "method_not_allowed", "method not allowed", omit_body);
}

int WebCtl_HandleRequest(const WebCtlRequest *request,
                         WebCtlResponse *response,
                         const WebCtlDeviceOps *device,
                         const WebCtlAssetProvider *assets,
                         char *scratch,
                         size_t scratch_size)
{
    return WebCtl_HandleRequestWithWork(request,
                                        response,
                                        device,
                                        assets,
                                        scratch,
                                        scratch_size,
                                        &s_default_work);
}


static const char *skip_ws(const char *p)
{
    while (p != NULL && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }
    return p;
}

static const char *find_json_field(const char *json, const char *key)
{
    char pattern[80];
    if (json == NULL || key == NULL) {
        return NULL;
    }
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        return NULL;
    }
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return NULL;
    }
    p += (size_t)n;
    p = skip_ws(p);
    if (p == NULL || *p != ':') {
        return NULL;
    }
    ++p;
    return skip_ws(p);
}

bool WebCtl_JsonGetBool(const char *json, const char *key, bool *out)
{
    const char *p = find_json_field(json, key);
    if (p == NULL || out == NULL) {
        return false;
    }
    if (strncmp(p, "true", 4u) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(p, "false", 5u) == 0) {
        *out = false;
        return true;
    }
    if (*p == '1' || *p == '0') {
        *out = (*p == '1');
        return true;
    }
    return false;
}

bool WebCtl_JsonGetString(const char *json, const char *key, char *out, size_t out_size)
{
    const char *p = find_json_field(json, key);
    size_t n = 0u;
    if (p == NULL || out == NULL || out_size == 0u || *p != '"') {
        return false;
    }
    ++p;
    while (*p != '\0' && *p != '"') {
        char ch = *p++;
        if (ch == '\\' && *p != '\0') {
            char esc = *p++;
            switch (esc) {
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case '\\': ch = '\\'; break;
            case '"': ch = '"'; break;
            default: ch = esc; break;
            }
        }
        if (n + 1u >= out_size) {
            return false;
        }
        out[n++] = ch;
    }
    if (*p != '"') {
        return false;
    }
    out[n] = '\0';
    return true;
}

bool WebCtl_JsonGetU64(const char *json, const char *key, uint64_t *out)
{
    const char *p = find_json_field(json, key);
    uint64_t value = 0u;
    bool any = false;
    if (p == NULL || out == NULL) {
        return false;
    }
    if (*p == '"') {
        ++p;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
            uint64_t digit = 0u;
            if (*p >= '0' && *p <= '9') digit = (uint64_t)(*p - '0');
            else if (*p >= 'a' && *p <= 'f') digit = (uint64_t)(*p - 'a' + 10);
            else digit = (uint64_t)(*p - 'A' + 10);
            if (value > (UINT64_MAX >> 4)) return false;
            value = (value << 4) | digit;
            any = true;
            ++p;
        }
    } else {
        while (*p >= '0' && *p <= '9') {
            if (value > ((UINT64_MAX - 9u) / 10u)) return false;
            value = (value * 10u) + (uint64_t)(*p - '0');
            any = true;
            ++p;
        }
    }
    if (!any) {
        return false;
    }
    *out = value;
    return true;
}

bool WebCtl_JsonGetUint32(const char *json, const char *key, uint32_t *out)
{
    uint64_t v = 0u;
    if (out == NULL || !WebCtl_JsonGetU64(json, key, &v) || v > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}
