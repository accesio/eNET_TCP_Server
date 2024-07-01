#include "mongoose.h"

// Function to serve static files
static void serve_static(struct mg_connection *nc, struct http_message *hm) {
    char path[100];
    snprintf(path, sizeof(path), "www%s", hm->uri.p);
    mg_serve_http(nc, hm, mg_mk_str(path), mg_mk_str("."), mg_mk_str("text/html"));
}

// Function to handle API requests
static void handle_api(struct mg_connection *nc, struct http_message *hm) {
    if (mg_vcmp(&hm->uri, "/api/data") == 0) {
        // Example response with dynamic data
        mg_send_head(nc, 200, hm->body.len, "Content-Type: application/json");
        mg_printf(nc, "{\"adc\": [1.23, 2.34, 3.45]}");
    } else {
        mg_send_head(nc, 404, 0, "Content-Type: text/plain");
        mg_printf(nc, "Not Found");
    }
}

// Event handler for Mongoose
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *hm = (struct http_message *) ev_data;
    switch (ev) {
        case MG_EV_HTTP_REQUEST:
            if (mg_vcmp(&hm->uri, "/api/") == 0) {
                handle_api(nc, hm);
            } else {
                serve_static(nc, hm);
            }
            break;
        default:
            break;
    }
}

int main(void) {
    struct mg_mgr mgr;
    struct mg_connection *nc;

    mg_mgr_init(&mgr, NULL);
    nc = mg_bind(&mgr, "80", ev_handler);
    mg_set_protocol_http_websocket(nc);

    printf("Starting Mongoose web server on port 80\n");
    for (;;) {
        mg_mgr_poll(&mgr, 1000);
    }
    mg_mgr_free(&mgr);

    return 0;
}
