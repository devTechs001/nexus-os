/*
 * NEXUS OS - IoT Framework
 * iot/protocols/coap.c
 *
 * CoAP Protocol Implementation
 *
 * This module implements the CoAP (Constrained Application Protocol)
 * for resource-constrained IoT devices. Supports RFC 7252.
 */

#include "../iot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*/
/*                         COAP CONSTANTS                                    */
/*===========================================================================*/

/* CoAP Version */
#define COAP_VERSION            1

/* CoAP Message Types */
#define COAP_CON                0   /* Confirmable */
#define COAP_NON                1   /* Non-confirmable */
#define COAP_ACK                2   /* Acknowledgment */
#define COAP_RST                3   /* Reset */

/* CoAP Request Methods */
#define COAP_GET                1
#define COAP_POST               2
#define COAP_PUT                3
#define COAP_DELETE             4

/* CoAP Response Codes */
#define COAP_CREATED                    65  /* 2.01 */
#define COAP_DELETED                    66  /* 2.02 */
#define COAP_VALID                      67  /* 2.03 */
#define COAP_CHANGED                    68  /* 2.04 */
#define COAP_CONTENT                    69  /* 2.05 */
#define COAP_BAD_REQUEST                128 /* 4.00 */
#define COAP_UNAUTHORIZED               129 /* 4.01 */
#define COAP_BAD_OPTION                 130 /* 4.02 */
#define COAP_FORBIDDEN                  131 /* 4.03 */
#define COAP_NOT_FOUND                  132 /* 4.04 */
#define COAP_METHOD_NOT_ALLOWED         133 /* 4.05 */
#define COAP_NOT_ACCEPTABLE             134 /* 4.06 */
#define COAP_PRECONDITION_FAILED        140 /* 4.12 */
#define COAP_REQUEST_ENTITY_TOO_LARGE   141 /* 4.13 */
#define COAP_UNSUPPORTED_CONTENT_FORMAT 143 /* 4.15 */
#define COAP_INTERNAL_SERVER_ERROR      160 /* 5.00 */
#define COAP_NOT_IMPLEMENTED            161 /* 5.01 */
#define COAP_BAD_GATEWAY                162 /* 5.02 */
#define COAP_SERVICE_UNAVAILABLE        163 /* 5.03 */
#define COAP_GATEWAY_TIMEOUT            164 /* 5.04 */
#define COAP_PROXYING_NOT_SUPPORTED     165 /* 5.05 */

/* CoAP Content Formats */
#define COAP_FORMAT_TEXT_PLAIN          0
#define COAP_FORMAT_LINK_FORMAT         40
#define COAP_FORMAT_OCTET_STREAM        42
#define COAP_FORMAT_EXI                 47
#define COAP_FORMAT_JSON                50
#define COAP_FORMAT_CBOR                60

/* CoAP Options */
#define COAP_OPTION_IF_MATCH            1
#define COAP_OPTION_URI_HOST            3
#define COAP_OPTION_ETAG                4
#define COAP_OPTION_IF_NONE_MATCH       5
#define COAP_OPTION_URI_PORT            7
#define COAP_OPTION_LOCATION_PATH       8
#define COAP_OPTION_URI_PATH            11
#define COAP_OPTION_CONTENT_FORMAT      12
#define COAP_OPTION_MAX_AGE             14
#define COAP_OPTION_URI_QUERY           15
#define COAP_OPTION_ACCEPT              17
#define COAP_OPTION_LOCATION_QUERY      20
#define COAP_OPTION_BLOCK2              23
#define COAP_OPTION_BLOCK1              27
#define COAP_OPTION_SIZE2               28
#define COAP_OPTION_PROXY_URI           35
#define COAP_OPTION_PROXY_SCHEME        39
#define COAP_OPTION_SIZE1               60

/* CoAP header size */
#define COAP_HEADER_SIZE        4

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int coap_send_message(coap_context_t *ctx, coap_message_t *msg);
static int coap_receive_message(coap_context_t *ctx, coap_message_t *msg);
static int coap_handle_request(coap_context_t *ctx, coap_message_t *req, coap_message_t *resp);
static int coap_create_response(coap_message_t *req, coap_message_t *resp, u8 code);
static int coap_encode_message(coap_message_t *msg, u8 *buffer, size_t *len);
static int coap_decode_message(const u8 *buffer, size_t len, coap_message_t *msg);
static int coap_add_option(coap_message_t *msg, u16 option_num, const u8 *value, size_t len);
static u16 coap_get_option(coap_message_t *msg, u16 option_num, u8 *value, size_t max_len);

/*===========================================================================*/
/*                         COAP CONTEXT LIFECYCLE                            */
/*===========================================================================*/

/**
 * coap_context_create - Create a CoAP context
 * @is_server: True for server mode, false for client
 * @host: Host address (server: bind address, client: server address)
 * @port: Port number
 *
 * Creates and initializes a CoAP context for client or server operation.
 *
 * Returns: Pointer to created context, or NULL on failure
 */
coap_context_t *coap_context_create(bool is_server, const char *host, u16 port)
{
    coap_context_t *ctx;

    ctx = (coap_context_t *)kzalloc(sizeof(coap_context_t));
    if (!ctx) {
        pr_err("Failed to allocate CoAP context\n");
        return NULL;
    }

    ctx->is_server = is_server;
    if (host) {
        strncpy(ctx->host, host, sizeof(ctx->host) - 1);
    }
    ctx->port = port;
    ctx->use_dtls = false;

    /* Initialize lock */
    ctx->lock.lock = 0;

    pr_info("CoAP context created: %s on %s:%u\n",
            is_server ? "server" : "client", host, port);

    return ctx;
}

/**
 * coap_context_destroy - Destroy a CoAP context
 * @ctx: Context to destroy
 *
 * Releases all resources associated with the CoAP context.
 */
void coap_context_destroy(coap_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    spin_lock(&ctx->lock);

    /* Free pending requests */
    if (ctx->pending_requests) {
        for (u32 i = 0; i < ctx->num_pending; i++) {
            if (ctx->pending_requests[i].token) {
                kfree(ctx->pending_requests[i].token);
            }
            if (ctx->pending_requests[i].payload) {
                kfree(ctx->pending_requests[i].payload);
            }
        }
        kfree(ctx->pending_requests);
    }

    /* Free resources (server mode) */
    if (ctx->resources) {
        for (u32 i = 0; i < ctx->num_resources; i++) {
            kfree(ctx->resources[i]);
        }
        kfree(ctx->resources);
        kfree(ctx->resource_handlers);
    }

    spin_unlock(&ctx->lock);

    kfree(ctx);

    pr_debug("CoAP context destroyed\n");
}

/*===========================================================================*/
/*                         COAP CLIENT OPERATIONS                            */
/*===========================================================================*/

/**
 * coap_send_request - Send a CoAP request
 * @ctx: CoAP context
 * @request: Request message
 * @response: Response message (output)
 *
 * Sends a CoAP request and waits for the response.
 *
 * Returns: 0 on success, error code on failure
 */
int coap_send_request(coap_context_t *ctx, const coap_message_t *request,
                      coap_message_t *response)
{
    u8 buffer[1024];
    size_t buffer_len;
    static u16 message_id = 0;
    int ret;

    if (!ctx || !request || !response) {
        return -1;
    }

    spin_lock(&ctx->lock);

    /* Create a copy of the request with updated message ID */
    coap_message_t req_copy = *request;
    req_copy.message_id = ++message_id;
    req_copy.type = COAP_CON;  /* Confirmable */

    /* Encode request */
    ret = coap_encode_message(&req_copy, buffer, &buffer_len);
    if (ret != 0) {
        pr_err("Failed to encode CoAP message\n");
        spin_unlock(&ctx->lock);
        return ret;
    }

    /* In a real implementation:
     * 1. Send UDP packet to server
     * 2. Wait for response with timeout
     * 3. Handle retransmission if needed
     */

    /* Simulate response */
    response->version = COAP_VERSION;
    response->type = COAP_ACK;
    response->code = COAP_CONTENT;
    response->message_id = req_copy.message_id;
    response->content_format = COAP_FORMAT_JSON;

    /* Simulated payload */
    const char *simulated_response = "{\"status\":\"ok\"}";
    response->payload_len = strlen(simulated_response);
    response->payload = kmalloc(response->payload_len);
    if (response->payload) {
        memcpy(response->payload, simulated_response, response->payload_len);
    }

    /* Update statistics */
    ctx->requests_sent++;
    ctx->responses_received++;

    pr_debug("CoAP request sent, response received\n");

    spin_unlock(&ctx->lock);

    return 0;
}

/**
 * coap_register_resource - Register a resource handler
 * @ctx: CoAP context (server)
 * @path: Resource path
 * @handler: Handler function
 *
 * Registers a resource handler for server mode.
 *
 * Returns: 0 on success, error code on failure
 */
int coap_register_resource(coap_context_t *ctx, const char *path, void *handler)
{
    char **new_resources;
    void **new_handlers;

    if (!ctx || !path || !handler) {
        return -1;
    }

    if (!ctx->is_server) {
        pr_err("Cannot register resource in client mode\n");
        return -1;
    }

    spin_lock(&ctx->lock);

    /* Reallocate arrays */
    new_resources = (char **)krealloc(ctx->resources,
                                       (ctx->num_resources + 1) * sizeof(char *));
    if (!new_resources) {
        spin_unlock(&ctx->lock);
        return -1;
    }
    ctx->resources = new_resources;

    new_handlers = (void **)krealloc(ctx->resource_handlers,
                                      (ctx->num_resources + 1) * sizeof(void *));
    if (!new_handlers) {
        spin_unlock(&ctx->lock);
        return -1;
    }
    ctx->resource_handlers = new_handlers;

    /* Add resource */
    ctx->resources[ctx->num_resources] = kstrdup(path);
    ctx->resource_handlers[ctx->num_resources] = handler;
    ctx->num_resources++;

    pr_info("CoAP resource registered: %s\n", path);

    spin_unlock(&ctx->lock);

    return 0;
}

/*===========================================================================*/
/*                         MESSAGE ENCODING/DECODING                         */
/*===========================================================================*/

/**
 * coap_encode_message - Encode CoAP message to binary format
 * @msg: Message to encode
 * @buffer: Output buffer
 * @len: Output length
 *
 * Returns: 0 on success, error code on failure
 */
static int coap_encode_message(coap_message_t *msg, u8 *buffer, size_t *len)
{
    u8 *ptr = buffer;
    u16 prev_option = 0;

    if (!msg || !buffer) {
        return -1;
    }

    /* Fixed header (4 bytes) */
    /* Byte 0: Version (2 bits), Type (2 bits), Token Length (4 bits) */
    u8 token_len = msg->token_len & 0x0F;
    *ptr++ = ((COAP_VERSION << 6) | (msg->type << 4) | token_len);

    /* Byte 1: Code */
    *ptr++ = msg->code;

    /* Bytes 2-3: Message ID (network byte order) */
    *ptr++ = (msg->message_id >> 8) & 0xFF;
    *ptr++ = msg->message_id & 0xFF;

    /* Token */
    if (msg->token && msg->token_len > 0) {
        memcpy(ptr, msg->token, msg->token_len);
        ptr += msg->token_len;
    }

    /* Options */
    /* Options are sorted by option number and delta-encoded */
    /* This is a simplified implementation */
    for (u32 i = 0; i < msg->num_options; i++) {
        u16 option_num = msg->options[i];
        u16 option_delta = option_num - prev_option;
        size_t option_len = msg->option_lengths[i];
        u8 *option_value = msg->option_values[i];

        /* Calculate option header */
        u8 option_header = 0;
        size_t header_size = 1;

        /* Delta encoding */
        if (option_delta < 13) {
            option_header = (option_delta << 4);
        } else if (option_delta < 269) {
            option_header = (13 << 4);
            header_size = 2;
        } else {
            option_header = (14 << 4);
            header_size = 3;
        }

        /* Length encoding */
        if (option_len < 13) {
            option_header |= option_len;
        } else if (option_len < 269) {
            option_header |= 13;
            header_size++;
        } else {
            option_header |= 14;
            header_size += 2;
        }

        /* Write option header */
        if (header_size == 1) {
            *ptr++ = option_header;
        } else if (header_size == 2) {
            *ptr++ = option_header;
            if (option_delta < 13) {
                *ptr++ = option_len;
            } else {
                *ptr++ = option_delta - 13;
            }
        } else if (header_size == 3) {
            *ptr++ = option_header;
            *ptr++ = (option_delta - 269) >> 8;
            *ptr++ = (option_delta - 269) & 0xFF;
        }

        /* Write option value */
        memcpy(ptr, option_value, option_len);
        ptr += option_len;

        prev_option = option_num;
    }

    /* Payload marker (0xFF) if there's a payload */
    if (msg->payload && msg->payload_len > 0) {
        *ptr++ = 0xFF;

        /* Payload */
        memcpy(ptr, msg->payload, msg->payload_len);
        ptr += msg->payload_len;
    }

    *len = ptr - buffer;
    return 0;
}

/**
 * coap_decode_message - Decode CoAP message from binary format
 * @buffer: Input buffer
 * @len: Buffer length
 * @msg: Output message
 *
 * Returns: 0 on success, error code on failure
 */
static int coap_decode_message(const u8 *buffer, size_t len, coap_message_t *msg)
{
    const u8 *ptr = buffer;
    const u8 *end = buffer + len;
    u8 token_len;
    u16 prev_option = 0;

    if (!buffer || !msg || len < COAP_HEADER_SIZE) {
        return -1;
    }

    memset(msg, 0, sizeof(coap_message_t));

    /* Fixed header */
    u8 byte0 = *ptr++;
    msg->version = (byte0 >> 6) & 0x03;
    msg->type = (byte0 >> 4) & 0x03;
    token_len = byte0 & 0x0F;

    msg->code = *ptr++;
    msg->message_id = (*ptr++ << 8) | *ptr++;

    /* Token */
    if (token_len > 0) {
        if (ptr + token_len > end) {
            return -1;
        }
        msg->token = kmalloc(token_len);
        if (!msg->token) {
            return -1;
        }
        memcpy(msg->token, ptr, token_len);
        msg->token_len = token_len;
        ptr += token_len;
    }

    /* Options */
    while (ptr < end && *ptr != 0xFF) {
        u8 option_header = *ptr++;
        u8 option_delta = (option_header >> 4) & 0x0F;
        u8 option_len = option_header & 0x0F;

        /* Extended delta */
        if (option_delta == 13) {
            option_delta = *ptr++ + 13;
        } else if (option_delta == 14) {
            option_delta = ((*ptr++ << 8) | *ptr++) + 269;
        }

        /* Extended length */
        if (option_len == 13) {
            option_len = *ptr++ + 13;
        } else if (option_len == 14) {
            option_len = ((*ptr++ << 8) | *ptr++) + 269;
        }

        /* Calculate option number */
        u16 option_num = prev_option + option_delta;

        /* Check bounds */
        if (ptr + option_len > end) {
            return -1;
        }

        /* Add option */
        if (msg->num_options < 32) {
            msg->options[msg->num_options] = option_num;
            msg->option_lengths[msg->num_options] = option_len;
            msg->option_values[msg->num_options] = (u8 *)kmalloc(option_len);
            if (msg->option_values[msg->num_options]) {
                memcpy(msg->option_values[msg->num_options], ptr, option_len);
            }
            msg->num_options++;
        }

        ptr += option_len;
        prev_option = option_num;
    }

    /* Payload */
    if (ptr < end && *ptr == 0xFF) {
        ptr++;  /* Skip payload marker */
        msg->payload_len = end - ptr;
        msg->payload = kmalloc(msg->payload_len);
        if (msg->payload) {
            memcpy(msg->payload, ptr, msg->payload_len);
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         OPTION HANDLING                                   */
/*===========================================================================*/

/**
 * coap_add_option - Add an option to a CoAP message
 * @msg: Message
 * @option_num: Option number
 * @value: Option value
 * @len: Value length
 *
 * Returns: 0 on success, error code on failure
 */
static int coap_add_option(coap_message_t *msg, u16 option_num,
                           const u8 *value, size_t len)
{
    if (!msg || msg->num_options >= 32) {
        return -1;
    }

    msg->options[msg->num_options] = option_num;
    msg->option_lengths[msg->num_options] = len;
    msg->option_values[msg->num_options] = (u8 *)kmalloc(len);
    if (!msg->option_values[msg->num_options]) {
        return -1;
    }
    memcpy(msg->option_values[msg->num_options], value, len);
    msg->num_options++;

    return 0;
}

/**
 * coap_get_option - Get an option value from a CoAP message
 * @msg: Message
 * @option_num: Option number
 * @value: Output value buffer
 * @max_len: Maximum value length
 *
 * Returns: Option value length, or -1 if not found
 */
static u16 coap_get_option(coap_message_t *msg, u16 option_num,
                           u8 *value, size_t max_len)
{
    if (!msg) {
        return -1;
    }

    for (u32 i = 0; i < msg->num_options; i++) {
        if (msg->options[i] == option_num) {
            size_t len = msg->option_lengths[i];
            if (len > max_len) {
                len = max_len;
            }
            memcpy(value, msg->option_values[i], len);
            return msg->option_lengths[i];
        }
    }

    return -1;
}

/*===========================================================================*/
/*                         REQUEST/RESPONSE HANDLING                         */
/*===========================================================================*/

/**
 * coap_handle_request - Handle incoming CoAP request
 * @ctx: CoAP context
 * @req: Request message
 * @resp: Response message (output)
 *
 * Processes an incoming CoAP request and generates a response.
 *
 * Returns: 0 on success, error code on failure
 */
static int coap_handle_request(coap_context_t *ctx, coap_message_t *req,
                               coap_message_t *resp)
{
    char uri_path[256];
    u16 path_len;

    if (!ctx || !req || !resp) {
        return -1;
    }

    /* Get URI path */
    path_len = coap_get_option(req, COAP_OPTION_URI_PATH,
                               (u8 *)uri_path, sizeof(uri_path));
    if (path_len <= 0) {
        uri_path[0] = '\0';
    }

    /* Find matching resource */
    for (u32 i = 0; i < ctx->num_resources; i++) {
        if (strcmp(ctx->resources[i], uri_path) == 0) {
            /* Call resource handler */
            if (ctx->on_request) {
                ctx->on_request(ctx, req, resp);
                return 0;
            }
        }
    }

    /* Resource not found */
    return coap_create_response(req, resp, COAP_NOT_FOUND);
}

/**
 * coap_create_response - Create a CoAP response
 * @req: Request message
 * @resp: Response message (output)
 * @code: Response code
 *
 * Returns: 0 on success, error code on failure
 */
static int coap_create_response(coap_message_t *req, coap_message_t *resp, u8 code)
{
    if (!req || !resp) {
        return -1;
    }

    memset(resp, 0, sizeof(coap_message_t));

    resp->version = COAP_VERSION;
    resp->type = (req->type == COAP_CON) ? COAP_ACK : COAP_NON;
    resp->code = code;
    resp->message_id = req->message_id;

    /* Copy token */
    if (req->token && req->token_len > 0) {
        resp->token = kmalloc(req->token_len);
        if (resp->token) {
            memcpy(resp->token, req->token, req->token_len);
            resp->token_len = req->token_len;
        }
    }

    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * coap_method_to_string - Convert CoAP method code to string
 * @code: Method code
 *
 * Returns: String representation of method
 */
const char *coap_method_to_string(u8 code)
{
    switch (code) {
        case COAP_GET:    return "GET";
        case COAP_POST:   return "POST";
        case COAP_PUT:    return "PUT";
        case COAP_DELETE: return "DELETE";
        default:          return "UNKNOWN";
    }
}

/**
 * coap_code_to_string - Convert CoAP response code to string
 * @code: Response code
 *
 * Returns: String representation of response code
 */
const char *coap_code_to_string(u8 code)
{
    switch (code) {
        case COAP_CREATED:                  return "2.01 Created";
        case COAP_DELETED:                  return "2.02 Deleted";
        case COAP_VALID:                    return "2.03 Valid";
        case COAP_CHANGED:                  return "2.04 Changed";
        case COAP_CONTENT:                  return "2.05 Content";
        case COAP_BAD_REQUEST:              return "4.00 Bad Request";
        case COAP_UNAUTHORIZED:             return "4.01 Unauthorized";
        case COAP_FORBIDDEN:                return "4.03 Forbidden";
        case COAP_NOT_FOUND:                return "4.04 Not Found";
        case COAP_METHOD_NOT_ALLOWED:       return "4.05 Method Not Allowed";
        case COAP_NOT_ACCEPTABLE:           return "4.06 Not Acceptable";
        case COAP_INTERNAL_SERVER_ERROR:    return "5.00 Internal Server Error";
        case COAP_NOT_IMPLEMENTED:          return "5.01 Not Implemented";
        case COAP_SERVICE_UNAVAILABLE:      return "5.03 Service Unavailable";
        default:                            return "Unknown";
    }
}

/**
 * coap_format_to_string - Convert CoAP content format to string
 * @format: Content format
 *
 * Returns: String representation of content format
 */
const char *coap_format_to_string(u16 format)
{
    switch (format) {
        case COAP_FORMAT_TEXT_PLAIN:  return "text/plain";
        case COAP_FORMAT_LINK_FORMAT: return "application/link-format";
        case COAP_FORMAT_OCTET_STREAM: return "application/octet-stream";
        case COAP_FORMAT_JSON:        return "application/json";
        case COAP_FORMAT_CBOR:        return "application/cbor";
        default:                      return "unknown";
    }
}
