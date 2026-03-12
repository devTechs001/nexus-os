/*
 * NEXUS OS - IoT Framework
 * Copyright (c) 2024 NEXUS Development Team
 *
 * iot.h - IoT Framework Header
 *
 * This header defines the core IoT framework interfaces for NEXUS OS.
 * It provides support for IoT protocols, device management, and edge computing.
 */

#ifndef _NEXUS_IOT_H
#define _NEXUS_IOT_H

#include "../kernel/include/types.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         IOT FRAMEWORK VERSION                             */
/*===========================================================================*/

#define IOT_VERSION_MAJOR       1
#define IOT_VERSION_MINOR       0
#define IOT_VERSION_PATCH       0
#define IOT_VERSION_STRING      "1.0.0"
#define IOT_CODENAME            "EdgeCore"

/*===========================================================================*/
/*                         FRAMEWORK CONFIGURATION                           */
/*===========================================================================*/

/* Maximum supported devices */
#define MAX_IOT_DEVICES         1024
#define MAX_DEVICE_GROUPS       256
#define MAX_DEVICE_PROPERTIES   64

/* Protocol configuration */
#define MQTT_DEFAULT_PORT       1883
#define MQTT_SECURE_PORT        8883
#define COAP_DEFAULT_PORT       5683
#define COAP_SECURE_PORT        5684

/* Message sizes */
#define MAX_TOPIC_LENGTH        256
#define MAX_PAYLOAD_SIZE        (64 * 1024)
#define MAX_MESSAGE_QUEUE       1024

/* Edge computing */
#define MAX_EDGE_NODES          64
#define MAX_EDGE_FUNCTIONS      256
#define MAX_EDGE_WORKFLOWS      128

/* Device types */
#define DEVICE_TYPE_SENSOR      0x01
#define DEVICE_TYPE_ACTUATOR    0x02
#define DEVICE_TYPE_GATEWAY     0x04
#define DEVICE_TYPE_CONTROLLER  0x08
#define DEVICE_TYPE_DISPLAY     0x10
#define DEVICE_TYPE_SWITCH      0x20
#define DEVICE_TYPE_LOCK        0x40
#define DEVICE_TYPE_CAMERA      0x80

/* Device status */
#define DEVICE_STATUS_OFFLINE   0
#define DEVICE_STATUS_ONLINE    1
#define DEVICE_STATUS_ERROR     2
#define DEVICE_STATUS_SLEEPING  3
#define DEVICE_STATUS_UPDATING  4

/* QoS levels */
#define QOS_AT_MOST_ONCE        0
#define QOS_AT_LEAST_ONCE       1
#define QOS_EXACTLY_ONCE        2

/* Connection states */
#define CONN_STATE_DISCONNECTED 0
#define CONN_STATE_CONNECTING   1
#define CONN_STATE_CONNECTED    2
#define CONN_STATE_RECONNECTING 3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * IoT Device Information
 */
typedef struct iot_device {
    u32 device_id;                  /* Unique device identifier */
    char name[64];                  /* Device name */
    char type[32];                  /* Device type */
    char manufacturer[64];          /* Manufacturer */
    char model[64];                 /* Model number */
    char firmware_version[32];      /* Firmware version */
    char hardware_version[32];      /* Hardware version */
    
    /* Connection info */
    char connection_type[16];       /* Connection type (WiFi, BLE, Zigbee, etc.) */
    char ip_address[64];            /* IP address */
    u16 port;                       /* Port number */
    u32 status;                     /* Device status */
    u64 last_seen;                  /* Last seen timestamp */
    
    /* Properties */
    char properties[MAX_DEVICE_PROPERTIES][64];
    char property_values[MAX_DEVICE_PROPERTIES][128];
    u32 num_properties;
    
    /* Capabilities */
    u32 capabilities;               /* Capability bitmask */
    u32 device_type;                /* Device type bitmask */
    
    /* Security */
    char device_cert[256];          /* Device certificate */
    char auth_token[128];           /* Authentication token */
    
    /* Groups */
    u32 group_ids[8];               /* Group memberships */
    u32 num_groups;
    
    /* Callbacks */
    void (*on_connect)(struct iot_device *);
    void (*on_disconnect)(struct iot_device *);
    void (*on_message)(struct iot_device *, const char *, const void *, size_t);
    void (*on_error)(struct iot_device *, int);
    
    /* Links */
    struct iot_device *next;
    struct iot_device *prev;
} iot_device_t;

/**
 * MQTT Client Configuration
 */
typedef struct mqtt_config {
    char broker_host[256];          /* Broker hostname */
    u16 broker_port;                /* Broker port */
    char client_id[64];             /* Client identifier */
    char username[64];              /* Username */
    char password[64];              /* Password */
    bool use_ssl;                   /* Use SSL/TLS */
    bool clean_session;             /* Clean session flag */
    u16 keep_alive;                 /* Keep-alive interval (seconds) */
    u32 reconnect_delay;            /* Reconnect delay (ms) */
    u32 max_reconnect_attempts;     /* Maximum reconnect attempts */
    char will_topic[256];           /* Last will topic */
    char will_message[256];         /* Last will message */
    u8 will_qos;                    /* Last will QoS */
    bool will_retain;               /* Last will retain flag */
} mqtt_config_t;

/**
 * MQTT Message
 */
typedef struct mqtt_message {
    char topic[MAX_TOPIC_LENGTH];   /* Topic */
    void *payload;                  /* Payload data */
    size_t payload_len;             /* Payload length */
    u8 qos;                         /* QoS level */
    bool retain;                    /* Retain flag */
    bool dup;                       /* Duplicate flag */
    u16 message_id;                 /* Message ID (for QoS 1/2) */
    u64 timestamp;                  /* Message timestamp */
} mqtt_message_t;

/**
 * MQTT Client State
 */
typedef struct mqtt_client {
    bool connected;                 /* Connection state */
    u32 connection_state;           /* Detailed connection state */
    mqtt_config_t config;           /* Configuration */
    
    /* Network */
    int socket_fd;                  /* Socket file descriptor */
    bool use_ssl;                   /* SSL enabled */
    void *ssl_ctx;                  /* SSL context */
    void *ssl_conn;                 /* SSL connection */
    
    /* Message handling */
    mqtt_message_t *message_queue;  /* Message queue */
    u32 queue_head;                 /* Queue head index */
    u32 queue_tail;                 /* Queue tail index */
    u32 queue_size;                 /* Queue size */
    
    /* Subscriptions */
    char **subscriptions;           /* Subscribed topics */
    u8 *sub_qos;                    /* Subscription QoS levels */
    u32 num_subscriptions;          /* Number of subscriptions */
    
    /* Callbacks */
    void (*on_connect)(struct mqtt_client *);
    void (*on_disconnect)(struct mqtt_client *, int);
    void (*on_message)(struct mqtt_client *, const mqtt_message_t *);
    void (*on_publish)(struct mqtt_client *, u16);
    void (*on_error)(struct mqtt_client *, int);
    
    /* Statistics */
    u64 messages_sent;              /* Messages sent count */
    u64 messages_received;          /* Messages received count */
    u64 bytes_sent;                 /* Bytes sent */
    u64 bytes_received;             /* Bytes received */
    u64 reconnect_count;            /* Reconnect count */
    
    /* Synchronization */
    spinlock_t lock;                /* Client lock */
} mqtt_client_t;

/**
 * CoAP Message
 */
typedef struct coap_message {
    u8 version;                     /* CoAP version */
    u8 type;                        /* Message type (CON, NON, ACK, RST) */
    u8 code;                        /* Request/Response code */
    u16 message_id;                 /* Message ID */
    char *token;                    /* Token */
    size_t token_len;               /* Token length */
    
    /* Options */
    u16 *options;                   /* Option numbers */
    u8 **option_values;             /* Option values */
    size_t *option_lengths;         /* Option lengths */
    u32 num_options;                /* Number of options */
    
    /* Payload */
    void *payload;                  /* Payload */
    size_t payload_len;             /* Payload length */
    
    /* Content format */
    u16 content_format;             /* Content format (JSON, CBOR, etc.) */
    
    /* Timing */
    u64 timestamp;                  /* Message timestamp */
    u64 timeout;                    /* Response timeout */
} coap_message_t;

/**
 * CoAP Client/Server State
 */
typedef struct coap_context {
    bool is_server;                 /* Server mode */
    char host[256];                 /* Host address */
    u16 port;                       /* Port number */
    bool use_dtls;                  /* DTLS enabled */
    void *dtls_ctx;                 /* DTLS context */
    
    /* Pending requests */
    coap_message_t *pending_requests;
    u32 num_pending;
    
    /* Resources (server mode) */
    char **resources;               /* Resource paths */
    void **resource_handlers;       /* Resource handlers */
    u32 num_resources;
    
    /* Callbacks */
    void (*on_request)(struct coap_context *, const coap_message_t *, coap_message_t *);
    void (*on_response)(struct coap_context *, const coap_message_t *);
    void (*on_error)(struct coap_context *, int);
    
    /* Statistics */
    u64 requests_sent;
    u64 requests_received;
    u64 responses_sent;
    u64 responses_received;
    
    spinlock_t lock;
} coap_context_t;

/**
 * Edge Computing Function
 */
typedef struct edge_function {
    u32 function_id;                /* Function identifier */
    char name[64];                  /* Function name */
    char description[256];          /* Function description */
    char runtime[32];               /* Runtime (JavaScript, Python, etc.) */
    char *code;                     /* Function code */
    size_t code_size;               /* Code size */
    
    /* Triggers */
    char **triggers;                /* Trigger conditions */
    u32 num_triggers;
    
    /* Execution */
    u32 timeout_ms;                 /* Execution timeout */
    u32 memory_limit;               /* Memory limit (KB) */
    u32 max_instances;              /* Maximum concurrent instances */
    
    /* State */
    bool enabled;                   /* Function enabled */
    u32 execution_count;            /* Execution count */
    u64 last_execution;             /* Last execution time */
    u64 total_execution_time;       /* Total execution time */
    
    /* Links */
    struct edge_function *next;
} edge_function_t;

/**
 * Edge Computing Workflow
 */
typedef struct edge_workflow {
    u32 workflow_id;                /* Workflow identifier */
    char name[64];                  /* Workflow name */
    char description[256];          /* Workflow description */
    
    /* Steps */
    u32 *step_function_ids;         /* Function IDs for each step */
    char **step_conditions;         /* Conditions for each step */
    u32 num_steps;
    
    /* State */
    bool enabled;                   /* Workflow enabled */
    u32 execution_count;            /* Execution count */
    
    /* Links */
    struct edge_workflow *next;
} edge_workflow_t;

/**
 * Edge Node Information
 */
typedef struct edge_node {
    u32 node_id;                    /* Node identifier */
    char name[64];                  /* Node name */
    char location[128];             /* Physical location */
    
    /* Resources */
    u32 cpu_cores;                  /* CPU cores */
    u64 memory_total;               /* Total memory */
    u64 memory_used;                /* Used memory */
    u64 storage_total;              /* Total storage */
    u64 storage_used;               /* Used storage */
    
    /* Status */
    u32 status;                     /* Node status */
    u32 cpu_usage;                  /* CPU usage percentage */
    u32 memory_usage;               /* Memory usage percentage */
    u32 temperature;                /* Temperature */
    
    /* Functions */
    edge_function_t *functions;     /* Deployed functions */
    edge_workflow_t *workflows;     /* Active workflows */
    u32 num_functions;
    u32 num_workflows;
    
    /* Network */
    char ip_address[64];            /* IP address */
    u32 bandwidth_up;               /* Upload bandwidth (Kbps) */
    u32 bandwidth_down;             /* Download bandwidth (Kbps) */
    
    /* Links */
    struct edge_node *next;
} edge_node_t;

/*===========================================================================*/
/*                         IOT FRAMEWORK STATE                               */
/*===========================================================================*/

typedef struct iot_framework {
    bool initialized;               /* Framework initialized */
    u32 version;                    /* Framework version */
    
    /* Device management */
    iot_device_t *devices;          /* Registered devices */
    u32 num_devices;                /* Number of devices */
    
    /* MQTT clients */
    mqtt_client_t *mqtt_clients;    /* MQTT client instances */
    u32 num_mqtt_clients;
    
    /* CoAP contexts */
    coap_context_t *coap_contexts;  /* CoAP contexts */
    u32 num_coap_contexts;
    
    /* Edge computing */
    edge_node_t *edge_nodes;        /* Edge nodes */
    u32 num_edge_nodes;
    
    /* Message broker (local) */
    void *message_broker;           /* Local message broker */
    
    /* Synchronization */
    spinlock_t lock;                /* Framework lock */
} iot_framework_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Framework initialization */
int iot_framework_init(void);
void iot_framework_shutdown(void);
bool iot_framework_is_initialized(void);

/* Device management */
iot_device_t *device_create(const char *name, const char *type);
void device_destroy(iot_device_t *device);
int device_register(iot_device_t *device);
int device_unregister(iot_device_t *device);
iot_device_t *device_find_by_id(u32 device_id);
iot_device_t *device_find_by_name(const char *name);
int device_set_property(iot_device_t *device, const char *key, const char *value);
const char *device_get_property(iot_device_t *device, const char *key);
int device_send_command(iot_device_t *device, const char *command, const void *data, size_t len);

/* MQTT client */
mqtt_client_t *mqtt_client_create(const mqtt_config_t *config);
void mqtt_client_destroy(mqtt_client_t *client);
int mqtt_client_connect(mqtt_client_t *client);
int mqtt_client_disconnect(mqtt_client_t *client);
int mqtt_client_publish(mqtt_client_t *client, const char *topic, const void *payload, size_t len, u8 qos);
int mqtt_client_subscribe(mqtt_client_t *client, const char *topic, u8 qos);
int mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic);

/* CoAP client/server */
coap_context_t *coap_context_create(bool is_server, const char *host, u16 port);
void coap_context_destroy(coap_context_t *ctx);
int coap_send_request(coap_context_t *ctx, const coap_message_t *request, coap_message_t *response);
int coap_register_resource(coap_context_t *ctx, const char *path, void *handler);

/* Edge computing */
edge_node_t *edge_node_create(const char *name, const char *location);
void edge_node_destroy(edge_node_t *node);
int edge_node_register(edge_node_t *node);
edge_function_t *edge_function_create(const char *name, const char *runtime, const char *code);
void edge_function_destroy(edge_function_t *func);
int edge_function_deploy(edge_node_t *node, edge_function_t *func);
int edge_function_execute(edge_function_t *func, const void *input, size_t input_len, void *output, size_t *output_len);
edge_workflow_t *edge_workflow_create(const char *name);
void edge_workflow_destroy(edge_workflow_t *workflow);
int edge_workflow_add_step(edge_workflow_t *workflow, u32 function_id, const char *condition);

/* Utility functions */
const char *iot_get_version(void);
const char *iot_get_codename(void);
u64 iot_get_timestamp(void);
int iot_generate_device_id(void);

#endif /* _NEXUS_IOT_H */
