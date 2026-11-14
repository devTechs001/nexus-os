/*
 * NEXUS OS - IoT Framework
 * iot/protocols/mqtt.c
 *
 * MQTT Protocol Implementation
 *
 * This module implements the MQTT (Message Queuing Telemetry Transport)
 * protocol client for IoT messaging. Supports MQTT 3.1.1 and 5.0.
 */

#include "../iot.h"
/*
 * These were <stdio.h>, <stdlib.h> and <string.h>, none of which exist for a
 * freestanding kernel build. Without a declaration, allocations were assumed
 * to return int and their pointers were truncated to 32 bits.
 */
#include "../../kernel/mm/mm.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         MQTT CONSTANTS                                    */
/*===========================================================================*/

/* MQTT Control Packet Types */
#define MQTT_CONNECT      0x10
#define MQTT_CONNACK      0x20
#define MQTT_PUBLISH      0x30
#define MQTT_PUBACK       0x40
#define MQTT_PUBREC       0x50
#define MQTT_PUBREL       0x60
#define MQTT_PUBCOMP      0x70
#define MQTT_SUBSCRIBE    0x80
#define MQTT_SUBACK       0x90
#define MQTT_UNSUBSCRIBE  0xA0
#define MQTT_UNSUBACK     0xB0
#define MQTT_PINGREQ      0xC0
#define MQTT_PINGRESP     0xD0
#define MQTT_DISCONNECT   0xE0

/* MQTT Connect Flags */
#define MQTT_CONNECT_CLEAN_SESSION  0x02
#define MQTT_CONNECT_WILL_FLAG      0x04
#define MQTT_CONNECT_WILL_QOS       0x18
#define MQTT_CONNECT_WILL_RETAIN    0x20
#define MQTT_CONNECT_PASSWORD_FLAG  0x40
#define MQTT_CONNECT_USERNAME_FLAG  0x80

/* MQTT Connection Return Codes */
#define MQTT_CONNACK_ACCEPTED           0
#define MQTT_CONNACK_UNACCEPTABLE_PROTO 1
#define MQTT_CONNACK_IDENTIFIER_REJECTED 2
#define MQTT_CONNACK_SERVER_UNAVAILABLE 3
#define MQTT_CONNACK_BAD_USERNAME_PASS  4
#define MQTT_CONNACK_NOT_AUTHORIZED     5

/* Fixed header size */
#define MQTT_FIXED_HEADER_MAX   5

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int mqtt_send_packet(mqtt_client_t *client, u8 packet_type, const void *payload, size_t payload_len);
static int mqtt_receive_packet(mqtt_client_t *client);
static int mqtt_handle_connack(mqtt_client_t *client, const u8 *data, size_t len);
static int mqtt_handle_publish(mqtt_client_t *client, const u8 *data, size_t len);
static int mqtt_handle_puback(mqtt_client_t *client, const u8 *data, size_t len);
static int mqtt_create_connect_packet(mqtt_client_t *client, u8 *buffer, size_t *len);
static int mqtt_create_publish_packet(const char *topic, const void *payload, size_t payload_len,
                                       u8 qos, bool retain, u16 message_id, u8 *buffer, size_t *len);
static int mqtt_create_subscribe_packet(const char *topic, u8 qos, u16 message_id,
                                         u8 *buffer, size_t *len);
static u16 mqtt_get_next_message_id(mqtt_client_t *client);
static int mqtt_write_string(u8 **buffer, const char *str);
static int mqtt_write_byte(u8 **buffer, u8 byte);
static int mqtt_write_int16(u8 **buffer, u16 value);
static int mqtt_read_string(const u8 **buffer, const u8 *end, char *str, size_t max_len);
static u8 mqtt_read_byte(const u8 **buffer);
static u16 mqtt_read_int16(const u8 **buffer);

/*===========================================================================*/
/*                         MQTT CLIENT LIFECYCLE                             */
/*===========================================================================*/

/**
 * mqtt_client_create - Create a new MQTT client
 * @config: Client configuration
 *
 * Creates and initializes a new MQTT client instance.
 *
 * Returns: Pointer to created client, or NULL on failure
 */
mqtt_client_t *mqtt_client_create(const mqtt_config_t *config)
{
    mqtt_client_t *client;
    
    if (!config) {
        pr_err("MQTT config required\n");
        return NULL;
    }

    client = (mqtt_client_t *)kzalloc(sizeof(mqtt_client_t));
    if (!client) {
        pr_err("Failed to allocate MQTT client\n");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&client->config, config, sizeof(mqtt_config_t));

    /* Initialize message queue */
    client->queue_size = MAX_MESSAGE_QUEUE;
    client->message_queue = (mqtt_message_t *)kzalloc(client->queue_size * sizeof(mqtt_message_t));
    if (!client->message_queue) {
        kfree(client);
        pr_err("Failed to allocate MQTT message queue\n");
        return NULL;
    }
    client->queue_head = 0;
    client->queue_tail = 0;

    /* Initialize lock */
    client->lock.lock = 0;

    client->connected = false;
    client->connection_state = CONN_STATE_DISCONNECTED;

    pr_info("MQTT client created: %s\n", config->client_id);

    return client;
}

/**
 * mqtt_client_destroy - Destroy an MQTT client
 * @client: Client to destroy
 *
 * Releases all resources associated with the MQTT client.
 */
void mqtt_client_destroy(mqtt_client_t *client)
{
    if (!client) {
        return;
    }

    spin_lock(&client->lock);

    /* Disconnect if connected */
    if (client->connected) {
        mqtt_client_disconnect(client);
    }

    /* Free message queue */
    if (client->message_queue) {
        for (u32 i = 0; i < client->queue_size; i++) {
            if (client->message_queue[i].payload) {
                kfree(client->message_queue[i].payload);
            }
        }
        kfree(client->message_queue);
    }

    /* Free subscriptions */
    if (client->subscriptions) {
        for (u32 i = 0; i < client->num_subscriptions; i++) {
            kfree(client->subscriptions[i]);
        }
        kfree(client->subscriptions);
        kfree(client->sub_qos);
    }

    spin_unlock(&client->lock);

    kfree(client);

    pr_debug("MQTT client destroyed\n");
}

/*===========================================================================*/
/*                         CONNECTION MANAGEMENT                             */
/*===========================================================================*/

/**
 * mqtt_client_connect - Connect to MQTT broker
 * @client: MQTT client
 *
 * Establishes a connection to the configured MQTT broker.
 *
 * Returns: 0 on success, error code on failure
 */
int mqtt_client_connect(mqtt_client_t *client)
{
    u8 buffer[512];
    size_t buffer_len;
    int ret;

    if (!client) {
        return -1;
    }

    if (client->connected) {
        pr_debug("MQTT client already connected\n");
        return 0;
    }

    spin_lock(&client->lock);

    pr_info("Connecting to MQTT broker: %s:%u\n",
            client->config.broker_host, client->config.broker_port);

    client->connection_state = CONN_STATE_CONNECTING;

    /* Create CONNECT packet */
    ret = mqtt_create_connect_packet(client, buffer, &buffer_len);
    if (ret != 0) {
        pr_err("Failed to create MQTT CONNECT packet\n");
        spin_unlock(&client->lock);
        return ret;
    }

    /* In a real implementation, this would:
     * 1. Create TCP/TLS connection to broker
     * 2. Send CONNECT packet
     * 3. Wait for CONNACK response
     * 4. Handle connection result
     */
    
    /* Simulated connection success */
    client->socket_fd = 1;  /* Simulated socket */
    client->connected = true;
    client->connection_state = CONN_STATE_CONNECTED;

    pr_info("MQTT client connected\n");

    /* Call connect callback */
    if (client->on_connect) {
        client->on_connect(client);
    }

    spin_unlock(&client->lock);

    return 0;
}

/**
 * mqtt_client_disconnect - Disconnect from MQTT broker
 * @client: MQTT client
 *
 * Gracefully disconnects from the MQTT broker.
 *
 * Returns: 0 on success, error code on failure
 */
int mqtt_client_disconnect(mqtt_client_t *client)
{
    int ret;

    if (!client) {
        return -1;
    }

    if (!client->connected) {
        return 0;
    }

    spin_lock(&client->lock);

    pr_info("Disconnecting MQTT client...\n");

    /*
     * Send the DISCONNECT packet.
     *
     * The local two-byte packet built here was never transmitted - the send
     * goes through mqtt_send_packet(), which constructs the header itself - so
     * it was dead and drew a 'set but not used' warning.
     */
    ret = mqtt_send_packet(client, MQTT_DISCONNECT, NULL, 0);
    if (ret != 0) {
        pr_warn("Failed to send MQTT DISCONNECT packet\n");
    }

    /* Close connection */
    client->connected = false;
    client->connection_state = CONN_STATE_DISCONNECTED;

    pr_info("MQTT client disconnected\n");

    /* Call disconnect callback */
    if (client->on_disconnect) {
        client->on_disconnect(client, 0);
    }

    spin_unlock(&client->lock);

    return 0;
}

/*===========================================================================*/
/*                         PUBLISH/SUBSCRIBE                                 */
/*===========================================================================*/

/**
 * mqtt_client_publish - Publish a message to a topic
 * @client: MQTT client
 * @topic: Topic to publish to
 * @payload: Message payload
 * @len: Payload length
 * @qos: Quality of Service level
 *
 * Publishes a message to the specified topic with the given QoS level.
 *
 * Returns: 0 on success, error code on failure
 */
int mqtt_client_publish(mqtt_client_t *client, const char *topic,
                        const void *payload, size_t len, u8 qos)
{
    u8 buffer[MAX_PAYLOAD_SIZE + 256];
    size_t buffer_len;
    u16 message_id = 0;
    int ret;

    if (!client || !topic) {
        return -1;
    }

    if (!client->connected) {
        pr_err("MQTT client not connected\n");
        return -1;
    }

    if (len > MAX_PAYLOAD_SIZE) {
        pr_err("Payload too large: %zu > %d\n", len, MAX_PAYLOAD_SIZE);
        return -1;
    }

    spin_lock(&client->lock);

    /* Get message ID for QoS 1 and 2 */
    if (qos > 0) {
        message_id = mqtt_get_next_message_id(client);
    }

    /* Create PUBLISH packet */
    ret = mqtt_create_publish_packet(topic, payload, len, qos, false, message_id,
                                      buffer, &buffer_len);
    if (ret != 0) {
        pr_err("Failed to create MQTT PUBLISH packet\n");
        spin_unlock(&client->lock);
        return ret;
    }

    /* Send packet */
    ret = mqtt_send_packet(client, MQTT_PUBLISH, buffer, buffer_len);
    if (ret != 0) {
        pr_err("Failed to send MQTT PUBLISH packet\n");
        spin_unlock(&client->lock);
        return ret;
    }

    /* Update statistics */
    client->messages_sent++;
    client->bytes_sent += buffer_len;

    pr_debug("MQTT published to %s (%zu bytes, QoS %u)\n", topic, len, qos);

    spin_unlock(&client->lock);

    return 0;
}

/**
 * mqtt_client_subscribe - Subscribe to a topic
 * @client: MQTT client
 * @topic: Topic to subscribe to
 * @qos: Requested QoS level
 *
 * Subscribes to the specified topic with the given QoS level.
 *
 * Returns: 0 on success, error code on failure
 */
int mqtt_client_subscribe(mqtt_client_t *client, const char *topic, u8 qos)
{
    u8 buffer[512];
    size_t buffer_len;
    u16 message_id;
    char **new_subs;
    u8 *new_qos;
    int ret;

    if (!client || !topic) {
        return -1;
    }

    if (!client->connected) {
        pr_err("MQTT client not connected\n");
        return -1;
    }

    spin_lock(&client->lock);

    message_id = mqtt_get_next_message_id(client);

    /* Create SUBSCRIBE packet */
    ret = mqtt_create_subscribe_packet(topic, qos, message_id, buffer, &buffer_len);
    if (ret != 0) {
        pr_err("Failed to create MQTT SUBSCRIBE packet\n");
        spin_unlock(&client->lock);
        return ret;
    }

    /* Send packet */
    ret = mqtt_send_packet(client, MQTT_SUBSCRIBE, buffer, buffer_len);
    if (ret != 0) {
        pr_err("Failed to send MQTT SUBSCRIBE packet\n");
        spin_unlock(&client->lock);
        return ret;
    }

    /* Add to subscription list */
    new_subs = (char **)krealloc(client->subscriptions,
                                  (client->num_subscriptions + 1) * sizeof(char *));
    if (!new_subs) {
        spin_unlock(&client->lock);
        return -1;
    }
    client->subscriptions = new_subs;

    new_qos = (u8 *)krealloc(client->sub_qos,
                              (client->num_subscriptions + 1) * sizeof(u8));
    if (!new_qos) {
        spin_unlock(&client->lock);
        return -1;
    }
    client->sub_qos = new_qos;

    client->subscriptions[client->num_subscriptions] = kstrdup(topic);
    client->sub_qos[client->num_subscriptions] = qos;
    client->num_subscriptions++;

    /* Update statistics */
    client->messages_sent++;
    client->bytes_sent += buffer_len;

    pr_info("MQTT subscribed to %s (QoS %u)\n", topic, qos);

    spin_unlock(&client->lock);

    return 0;
}

/**
 * mqtt_client_unsubscribe - Unsubscribe from a topic
 * @client: MQTT client
 * @topic: Topic to unsubscribe from
 *
 * Returns: 0 on success, error code on failure
 */
int mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic)
{
    if (!client || !topic) {
        return -1;
    }

    spin_lock(&client->lock);

    /* Find and remove subscription */
    for (u32 i = 0; i < client->num_subscriptions; i++) {
        if (strcmp(client->subscriptions[i], topic) == 0) {
            kfree(client->subscriptions[i]);

            /* Shift remaining subscriptions */
            for (u32 j = i; j < client->num_subscriptions - 1; j++) {
                client->subscriptions[j] = client->subscriptions[j + 1];
                client->sub_qos[j] = client->sub_qos[j + 1];
            }
            client->num_subscriptions--;

            pr_info("MQTT unsubscribed from %s\n", topic);
            spin_unlock(&client->lock);
            return 0;
        }
    }

    spin_unlock(&client->lock);
    return -1;  /* Topic not found */
}

/*===========================================================================*/
/*                         PACKET CREATION                                   */
/*===========================================================================*/

/**
 * mqtt_create_connect_packet - Create MQTT CONNECT packet
 * @client: MQTT client
 * @buffer: Output buffer
 * @len: Output length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_create_connect_packet(mqtt_client_t *client, u8 *buffer, size_t *len)
{
    u8 *ptr = buffer;
    u8 flags = 0;
    size_t remaining_length;
    const char *protocol_name = "MQTT";
    u8 protocol_level = 0x04;  /* MQTT 3.1.1 */

    /* Calculate remaining length */
    remaining_length = 10;  /* Fixed header */
    remaining_length += strlen(client->config.client_id) + 2;
    if (client->config.will_topic[0]) {
        remaining_length += strlen(client->config.will_topic) + 2;
        remaining_length += strlen(client->config.will_message) + 2;
        flags |= MQTT_CONNECT_WILL_FLAG;
        flags |= (client->config.will_qos << 3) & MQTT_CONNECT_WILL_QOS;
        if (client->config.will_retain) {
            flags |= MQTT_CONNECT_WILL_RETAIN;
        }
    }
    if (client->config.username[0]) {
        remaining_length += strlen(client->config.username) + 2;
        flags |= MQTT_CONNECT_USERNAME_FLAG;
    }
    if (client->config.password[0]) {
        remaining_length += strlen(client->config.password) + 2;
        flags |= MQTT_CONNECT_PASSWORD_FLAG;
    }
    if (client->config.clean_session) {
        flags |= MQTT_CONNECT_CLEAN_SESSION;
    }

    /* Fixed header */
    *ptr++ = MQTT_CONNECT;
    *ptr++ = remaining_length & 0x7F;

    /* Variable header */
    ptr += mqtt_write_string(&ptr, protocol_name);
    *ptr++ = protocol_level;
    *ptr++ = flags;
    *ptr++ = (client->config.keep_alive >> 8) & 0xFF;
    *ptr++ = client->config.keep_alive & 0xFF;

    /* Payload */
    ptr += mqtt_write_string(&ptr, client->config.client_id);
    if (client->config.will_topic[0]) {
        ptr += mqtt_write_string(&ptr, client->config.will_topic);
        ptr += mqtt_write_string(&ptr, client->config.will_message);
    }
    if (client->config.username[0]) {
        ptr += mqtt_write_string(&ptr, client->config.username);
    }
    if (client->config.password[0]) {
        ptr += mqtt_write_string(&ptr, client->config.password);
    }

    *len = ptr - buffer;
    return 0;
}

/**
 * mqtt_create_publish_packet - Create MQTT PUBLISH packet
 * @topic: Topic name
 * @payload: Message payload
 * @payload_len: Payload length
 * @qos: QoS level
 * @retain: Retain flag
 * @message_id: Message ID (for QoS > 0)
 * @buffer: Output buffer
 * @len: Output length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_create_publish_packet(const char *topic, const void *payload,
                                       size_t payload_len, u8 qos, bool retain,
                                       u16 message_id, u8 *buffer, size_t *len)
{
    u8 *ptr = buffer;
    u8 flags = 0;
    size_t remaining_length;

    /* Calculate flags */
    flags = MQTT_PUBLISH;
    if (qos > 0) {
        flags |= (qos << 1);
    }
    if (retain) {
        flags |= 0x01;
    }

    /* Calculate remaining length */
    remaining_length = strlen(topic) + 2;
    if (qos > 0) {
        remaining_length += 2;
    }
    remaining_length += payload_len;

    /* Fixed header */
    *ptr++ = flags;
    *ptr++ = remaining_length & 0x7F;

    /* Variable header */
    ptr += mqtt_write_string(&ptr, topic);
    if (qos > 0) {
        ptr += mqtt_write_int16(&ptr, message_id);
    }

    /* Payload */
    memcpy(ptr, payload, payload_len);
    ptr += payload_len;

    *len = ptr - buffer;
    return 0;
}

/**
 * mqtt_create_subscribe_packet - Create MQTT SUBSCRIBE packet
 * @topic: Topic to subscribe to
 * @qos: Requested QoS
 * @message_id: Message ID
 * @buffer: Output buffer
 * @len: Output length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_create_subscribe_packet(const char *topic, u8 qos, u16 message_id,
                                         u8 *buffer, size_t *len)
{
    u8 *ptr = buffer;
    size_t remaining_length;

    /* Calculate remaining length */
    remaining_length = 2 + strlen(topic) + 2 + 1;

    /* Fixed header */
    *ptr++ = MQTT_SUBSCRIBE;
    *ptr++ = remaining_length & 0x7F;

    /* Variable header */
    ptr += mqtt_write_int16(&ptr, message_id);

    /* Payload */
    ptr += mqtt_write_string(&ptr, topic);
    *ptr++ = qos;

    *len = ptr - buffer;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * mqtt_get_next_message_id - Get next available message ID
 * @client: MQTT client
 *
 * Returns: Message ID (1-65535)
 */
static u16 mqtt_get_next_message_id(mqtt_client_t *client)
{
    static u16 next_id = 1;
    
    if (next_id == 0) {
        next_id = 1;
    }
    
    return next_id++;
}

/**
 * mqtt_write_string - Write MQTT string to buffer
 * @buffer: Buffer pointer (updated)
 * @str: String to write
 *
 * Returns: Number of bytes written
 */
static int mqtt_write_string(u8 **buffer, const char *str)
{
    u16 len = strlen(str);
    u8 *ptr = *buffer;

    *ptr++ = (len >> 8) & 0xFF;
    *ptr++ = len & 0xFF;
    memcpy(ptr, str, len);
    ptr += len;

    *buffer = ptr;
    return len + 2;
}

/**
 * mqtt_write_byte - Write byte to buffer
 * @buffer: Buffer pointer (updated)
 * @byte: Byte to write
 *
 * Returns: 1
 */
static int mqtt_write_byte(u8 **buffer, u8 byte)
{
    **buffer = byte;
    (*buffer)++;
    return 1;
}

/**
 * mqtt_write_int16 - Write 16-bit integer to buffer
 * @buffer: Buffer pointer (updated)
 * @value: Value to write
 *
 * Returns: 2
 */
static int mqtt_write_int16(u8 **buffer, u16 value)
{
    u8 *ptr = *buffer;
    mqtt_write_byte(&ptr, (value >> 8) & 0xFF);
    mqtt_write_byte(&ptr, value & 0xFF);
    *buffer = ptr;
    return 2;
}

/**
 * mqtt_read_string - Read MQTT string from buffer
 * @buffer: Buffer pointer (updated)
 * @end: End of buffer
 * @str: Output string
 * @max_len: Maximum string length
 *
 * Returns: Number of bytes read, or -1 on error
 */
static int mqtt_read_string(const u8 **buffer, const u8 *end, char *str, size_t max_len)
{
    if (*buffer + 2 > end) {
        return -1;
    }

    u16 len = mqtt_read_int16(buffer);
    if (*buffer + len > end || len >= max_len) {
        return -1;
    }

    memcpy(str, *buffer, len);
    str[len] = '\0';
    *buffer += len;

    return len + 2;
}

/**
 * mqtt_read_byte - Read byte from buffer
 * @buffer: Buffer pointer (updated)
 *
 * Returns: Byte value
 */
static u8 mqtt_read_byte(const u8 **buffer)
{
    u8 byte = **buffer;
    (*buffer)++;
    return byte;
}

/**
 * mqtt_read_int16 - Read 16-bit integer from buffer
 * @buffer: Buffer pointer (updated)
 *
 * Returns: Integer value
 */
static u16 mqtt_read_int16(const u8 **buffer)
{
    u16 value = ((u16)(*buffer)[0] << 8) | (*buffer)[1];
    *buffer += 2;
    return value;
}

/**
 * mqtt_send_packet - Send MQTT packet (stub)
 * @client: MQTT client
 * @packet_type: Packet type
 * @payload: Packet payload
 * @payload_len: Payload length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_send_packet(mqtt_client_t *client, u8 packet_type,
                            const void *payload, size_t payload_len)
{
    /* In a real implementation, this would write to the socket */
    (void)client;
    (void)packet_type;
    (void)payload;
    (void)payload_len;
    return 0;
}

/**
 * mqtt_receive_packet - Receive and handle MQTT packet (stub)
 * @client: MQTT client
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_receive_packet(mqtt_client_t *client)
{
    u8 buffer[512];
    int n;

    /*
     * Read one packet from the transport and hand it to the dispatcher.
     *
     * This used to return 0 without reading anything, so a connected client
     * never saw a single inbound packet: CONNACK was never observed, PUBLISH
     * never reached a subscriber callback, and the whole receive half of the
     * protocol was unreachable.
     *
     * There is no socket read entry point in the network layer yet, so the
     * call is guarded and reports the missing capability instead of pretending
     * to have received something.
     */
    extern int mqtt_transport_read(mqtt_client_t *client, void *buf, size_t len);

    if (!client || !client->connected) {
        return -EINVAL;
    }

    n = mqtt_transport_read(client, buffer, sizeof(buffer));
    if (n < 0) {
        return n;
    }
    if (n == 0) {
        return 0;
    }

    return mqtt_process_packet(client, buffer, (size_t)n);
}

/*
 * Weak default for the transport read. The network stack does not expose a
 * socket read path yet, so this reports that instead of silently succeeding.
 * A real implementation overrides it when the socket layer lands.
 */
__attribute__((weak)) int mqtt_transport_read(mqtt_client_t *client,
                                              void *buf, size_t len)
{
    (void)client;
    (void)buf;
    (void)len;
    return -ENOSYS;
}

/**
 * mqtt_client_poll - Process any pending inbound traffic
 *
 * The public entry point for the receive side. An application connected to a
 * broker calls this from its own loop; without it mqtt_receive_packet() had no
 * caller at all and a session could never make progress after connecting.
 *
 * Returns 0 when nothing was pending, a positive count when packets were
 * handled, or a negative error.
 */
int mqtt_client_poll(mqtt_client_t *client, int max_packets)
{
    int handled = 0;

    if (!client) {
        return -EINVAL;
    }

    if (max_packets <= 0) {
        max_packets = 1;
    }

    while (handled < max_packets) {
        int ret = mqtt_receive_packet(client);

        if (ret < 0) {
            return handled > 0 ? handled : ret;
        }
        if (ret == 0) {
            break;
        }
        handled++;
    }

    return handled;
}

/**
 * mqtt_process_packet - Route one inbound packet to its handler
 *
 * The handlers (mqtt_handle_connack, mqtt_handle_publish, mqtt_handle_puback)
 * were all written and none of them had a caller: mqtt_receive_packet()
 * returned 0 without reading anything. An MQTT session could send and never
 * process a reply, so CONNACK was never observed and the client never learned
 * whether the broker accepted it.
 *
 * @client: MQTT client
 * @packet: Complete packet, starting with the fixed header byte
 * @len:    Packet length
 *
 * Returns 0 on success, negative if the packet type is unknown.
 */
int mqtt_process_packet(mqtt_client_t *client, const u8 *packet, size_t len)
{
    u8 type;
    const u8 *body;
    size_t body_len;

    if (!client || !packet || len < 2) {
        return -EINVAL;
    }

    type = packet[0] & 0xF0;

    /*
     * Fixed header is one type/flags byte plus a remaining-length field. This
     * decoder only handles the single-byte form, which is all the outbound
     * side produces.
     */
    if (packet[1] & 0x80) {
        pr_err("MQTT: multi-byte remaining length not supported\n");
        return -ENOTSUP;
    }

    body = packet + 2;
    body_len = packet[1];
    if (body_len > len - 2) {
        body_len = len - 2;
    }

    switch (type) {
    case MQTT_CONNACK:
        return mqtt_handle_connack(client, body, body_len);
    case MQTT_PUBLISH:
        return mqtt_handle_publish(client, body, body_len);
    case MQTT_PUBACK:
        return mqtt_handle_puback(client, body, body_len);
    default:
        pr_debug("MQTT: ignoring packet type 0x%02X\n", type);
        return 0;
    }
}

/**
 * mqtt_handle_connack - Handle CONNACK packet (stub)
 * @client: MQTT client
 * @data: Packet data
 * @len: Data length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_handle_connack(mqtt_client_t *client, const u8 *data, size_t len)
{
    const u8 *p = data;
    const u8 *end = data + len;
    u8 session_present;
    u8 return_code;

    if (len < 2) {
        pr_err("MQTT: CONNACK too short (%zu)\n", len);
        return -EINVAL;
    }

    /*
     * CONNACK payload: acknowledge-flags byte, then the connect return code.
     * This was a stub that ignored the packet entirely, so a refused
     * connection looked exactly like an accepted one.
     */
    session_present = mqtt_read_byte(&p) & 0x01;
    return_code = mqtt_read_byte(&p);
    (void)end;

    if (return_code != MQTT_CONNACK_ACCEPTED) {
        pr_err("MQTT: connection refused by broker (code %u)\n", return_code);
        client->connected = false;
        client->connection_state = CONN_STATE_DISCONNECTED;
        return -EACCES;
    }

    client->connected = true;
    client->connection_state = CONN_STATE_CONNECTED;
    pr_info("MQTT: connected (session_present=%u)\n", session_present);

    if (client->on_connect) {
        client->on_connect(client);
    }

    return 0;
}

/**
 * mqtt_handle_publish - Handle PUBLISH packet (stub)
 * @client: MQTT client
 * @data: Packet data
 * @len: Data length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_handle_publish(mqtt_client_t *client, const u8 *data, size_t len)
{
    const u8 *p = data;
    const u8 *end = data + len;
    char topic[128];
    u16 message_id = 0;
    size_t payload_len;

    /*
     * PUBLISH payload: topic, then (for QoS > 0) a message id, then the
     * payload. This was a stub, so a delivered message was silently dropped
     * and the subscriber callback never fired.
     */
    if (mqtt_read_string(&p, end, topic, sizeof(topic)) != 0) {
        return -EINVAL;
    }

    /* QoS is in the fixed header, which mqtt_process_packet() strips. The
     * message id is present for QoS 1 and 2; treat any trailing two bytes as
     * one only when there is more than the payload buffered. */
    if ((end - p) > 2) {
        message_id = mqtt_read_int16(&p);
    }

    payload_len = (size_t)(end - p);

    pr_debug("MQTT: PUBLISH topic='%s' id=%u len=%zu\n",
             topic, message_id, payload_len);

    if (client->on_message) {
        mqtt_message_t msg;

        memset(&msg, 0, sizeof(msg));
        strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
        msg.payload = (void *)(uintptr_t)p;
        msg.payload_len = payload_len;
        msg.message_id = message_id;
        msg.timestamp = get_time_ms();

        client->on_message(client, &msg);
    }

    return 0;
}

/**
 * mqtt_handle_puback - Handle PUBACK packet (stub)
 * @client: MQTT client
 * @data: Packet data
 * @len: Data length
 *
 * Returns: 0 on success, error code on failure
 */
static int mqtt_handle_puback(mqtt_client_t *client, const u8 *data, size_t len)
{
    const u8 *p = data;
    u16 message_id;

    if (len < 2) {
        return -EINVAL;
    }

    message_id = mqtt_read_int16(&p);

    pr_debug("MQTT: PUBACK for message %u\n", message_id);

    /*
     * Nothing tracks in-flight messages yet, so there is no queue to retire.
     * Report the acknowledgement so a lost delivery is visible rather than
     * being silently dropped, which is what this handler used to do.
     */
    pr_debug("MQTT: in-flight message %u acknowledged\n", message_id);

    return 0;
}
