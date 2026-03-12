/*
 * NEXUS OS - IoT Framework
 * iot/device_management/device_registry.c
 *
 * IoT Device Registry
 *
 * This module implements the device registry for managing IoT devices,
 * including device discovery, registration, property management, and
 * group organization.
 */

#include "../iot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================*/
/*                         DEVICE REGISTRY STATE                             */
/*===========================================================================*/

static struct {
    bool initialized;
    iot_device_t *devices;          /* Linked list of devices */
    u32 num_devices;
    u32 next_device_id;
    
    /* Device groups */
    struct {
        u32 group_id;
        char name[64];
        char description[256];
        u32 *device_ids;
        u32 num_devices;
    } groups[MAX_DEVICE_GROUPS];
    u32 num_groups;
    
    /* Discovery cache */
    iot_device_t *discovered_devices;
    u32 num_discovered;
    u64 last_discovery;
    
    /* Synchronization */
    spinlock_t lock;
} g_device_registry = {
    .initialized = false,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static u32 generate_device_id(void);
static iot_device_t *find_device_in_list(iot_device_t *list, u32 device_id);
static iot_device_t *find_device_by_name_in_list(iot_device_t *list, const char *name);
static int notify_device_event(iot_device_t *device, const char *event);

/*===========================================================================*/
/*                         REGISTRY INITIALIZATION                           */
/*===========================================================================*/

/**
 * device_registry_init - Initialize the device registry
 *
 * Returns: 0 on success, error code on failure
 */
int device_registry_init(void)
{
    if (g_device_registry.initialized) {
        pr_debug("Device registry already initialized\n");
        return 0;
    }

    spin_lock(&g_device_registry.lock);

    g_device_registry.next_device_id = 1;
    g_device_registry.num_devices = 0;
    g_device_registry.num_groups = 0;
    g_device_registry.num_discovered = 0;
    g_device_registry.last_discovery = 0;

    g_device_registry.initialized = true;

    pr_info("Device registry initialized\n");

    spin_unlock(&g_device_registry.lock);

    return 0;
}

/**
 * device_registry_shutdown - Shutdown the device registry
 *
 * Releases all device registry resources.
 */
void device_registry_shutdown(void)
{
    if (!g_device_registry.initialized) {
        return;
    }

    spin_lock(&g_device_registry.lock);

    /* Destroy all devices */
    iot_device_t *device = g_device_registry.devices;
    while (device) {
        iot_device_t *next = device->next;
        device_destroy(device);
        device = next;
    }
    g_device_registry.devices = NULL;
    g_device_registry.num_devices = 0;

    /* Clear groups */
    for (u32 i = 0; i < g_device_registry.num_groups; i++) {
        if (g_device_registry.groups[i].device_ids) {
            kfree(g_device_registry.groups[i].device_ids);
        }
    }
    g_device_registry.num_groups = 0;

    g_device_registry.initialized = false;

    pr_info("Device registry shutdown complete\n");

    spin_unlock(&g_device_registry.lock);
}

/*===========================================================================*/
/*                         DEVICE CREATION/DESTRUCTION                       */
/*===========================================================================*/

/**
 * device_create - Create a new IoT device
 * @name: Device name
 * @type: Device type
 *
 * Creates a new IoT device with the specified name and type.
 *
 * Returns: Pointer to created device, or NULL on failure
 */
iot_device_t *device_create(const char *name, const char *type)
{
    iot_device_t *device;

    if (!name) {
        pr_err("Device name required\n");
        return NULL;
    }

    device = (iot_device_t *)kzalloc(sizeof(iot_device_t));
    if (!device) {
        pr_err("Failed to allocate IoT device\n");
        return NULL;
    }

    /* Initialize device */
    device->device_id = generate_device_id();
    strncpy(device->name, name, sizeof(device->name) - 1);
    
    if (type) {
        strncpy(device->type, type, sizeof(device->type) - 1);
    } else {
        strncpy(device->type, "generic", sizeof(device->type) - 1);
    }

    device->status = DEVICE_STATUS_OFFLINE;
    device->num_properties = 0;
    device->num_groups = 0;
    device->capabilities = 0;
    device->device_type = 0;

    /* Initialize callbacks */
    device->on_connect = NULL;
    device->on_disconnect = NULL;
    device->on_message = NULL;
    device->on_error = NULL;

    device->next = NULL;
    device->prev = NULL;

    pr_debug("Device created: %s (ID: %u, Type: %s)\n",
             name, device->device_id, device->type);

    return device;
}

/**
 * device_destroy - Destroy an IoT device
 * @device: Device to destroy
 *
 * Releases all resources associated with the device.
 */
void device_destroy(iot_device_t *device)
{
    if (!device) {
        return;
    }

    /* Unregister from registry if registered */
    if (g_device_registry.initialized) {
        device_unregister(device);
    }

    /* Free any allocated strings */
    /* (Most strings are fixed-size arrays, so no freeing needed) */

    kfree(device);

    pr_debug("Device destroyed: %u\n", device->device_id);
}

/*===========================================================================*/
/*                         DEVICE REGISTRATION                               */
/*===========================================================================*/

/**
 * device_register - Register a device with the registry
 * @device: Device to register
 *
 * Adds the device to the global device registry.
 *
 * Returns: 0 on success, error code on failure
 */
int device_register(iot_device_t *device)
{
    if (!device) {
        return -1;
    }

    if (!g_device_registry.initialized) {
        pr_err("Device registry not initialized\n");
        return -1;
    }

    spin_lock(&g_device_registry.lock);

    /* Check for duplicate */
    if (find_device_in_list(g_device_registry.devices, device->device_id)) {
        pr_err("Device %u already registered\n", device->device_id);
        spin_unlock(&g_device_registry.lock);
        return -1;
    }

    /* Add to list */
    device->next = g_device_registry.devices;
    if (g_device_registry.devices) {
        g_device_registry.devices->prev = device;
    }
    g_device_registry.devices = device;
    g_device_registry.num_devices++;

    pr_info("Device registered: %s (ID: %u)\n", device->name, device->device_id);

    spin_unlock(&g_device_registry.lock);

    /* Notify registration */
    notify_device_event(device, "registered");

    return 0;
}

/**
 * device_unregister - Unregister a device from the registry
 * @device: Device to unregister
 *
 * Removes the device from the global device registry.
 *
 * Returns: 0 on success, error code on failure
 */
int device_unregister(iot_device_t *device)
{
    if (!device) {
        return -1;
    }

    if (!g_device_registry.initialized) {
        return 0;  /* Registry not initialized, nothing to do */
    }

    spin_lock(&g_device_registry.lock);

    /* Find device in list */
    iot_device_t *found = find_device_in_list(g_device_registry.devices, device->device_id);
    if (!found) {
        spin_unlock(&g_device_registry.lock);
        return -1;  /* Not registered */
    }

    /* Remove from list */
    if (found->prev) {
        found->prev->next = found->next;
    } else {
        g_device_registry.devices = found->next;
    }
    if (found->next) {
        found->next->prev = found->prev;
    }

    g_device_registry.num_devices--;

    pr_info("Device unregistered: %s (ID: %u)\n", device->name, device->device_id);

    spin_unlock(&g_device_registry.lock);

    /* Notify unregistration */
    notify_device_event(device, "unregistered");

    return 0;
}

/**
 * device_find_by_id - Find a device by ID
 * @device_id: Device ID
 *
 * Returns: Pointer to device, or NULL if not found
 */
iot_device_t *device_find_by_id(u32 device_id)
{
    iot_device_t *device;

    if (!g_device_registry.initialized) {
        return NULL;
    }

    spin_lock(&g_device_registry.lock);

    device = find_device_in_list(g_device_registry.devices, device_id);

    spin_unlock(&g_device_registry.lock);

    return device;
}

/**
 * device_find_by_name - Find a device by name
 * @name: Device name
 *
 * Returns: Pointer to device, or NULL if not found
 */
iot_device_t *device_find_by_name(const char *name)
{
    iot_device_t *device;

    if (!name || !g_device_registry.initialized) {
        return NULL;
    }

    spin_lock(&g_device_registry.lock);

    device = find_device_by_name_in_list(g_device_registry.devices, name);

    spin_unlock(&g_device_registry.lock);

    return device;
}

/*===========================================================================*/
/*                         DEVICE PROPERTIES                                 */
/*===========================================================================*/

/**
 * device_set_property - Set a device property
 * @device: Device
 * @key: Property key
 * @value: Property value
 *
 * Sets or updates a property on the device.
 *
 * Returns: 0 on success, error code on failure
 */
int device_set_property(iot_device_t *device, const char *key, const char *value)
{
    if (!device || !key || !value) {
        return -1;
    }

    spin_lock(&g_device_registry.lock);

    /* Check if property exists */
    for (u32 i = 0; i < device->num_properties; i++) {
        if (strcmp(device->properties[i], key) == 0) {
            /* Update existing property */
            strncpy(device->property_values[i], value,
                    sizeof(device->property_values[i]) - 1);
            spin_unlock(&g_device_registry.lock);
            pr_debug("Device %u property updated: %s=%s\n",
                     device->device_id, key, value);
            return 0;
        }
    }

    /* Add new property */
    if (device->num_properties >= MAX_DEVICE_PROPERTIES) {
        pr_err("Device %u property limit reached\n", device->device_id);
        spin_unlock(&g_device_registry.lock);
        return -1;
    }

    strncpy(device->properties[device->num_properties], key,
            sizeof(device->properties[device->num_properties]) - 1);
    strncpy(device->property_values[device->num_properties], value,
            sizeof(device->property_values[device->num_properties]) - 1);
    device->num_properties++;

    spin_unlock(&g_device_registry.lock);

    pr_debug("Device %u property added: %s=%s\n",
             device->device_id, key, value);

    return 0;
}

/**
 * device_get_property - Get a device property
 * @device: Device
 * @key: Property key
 *
 * Returns: Property value, or NULL if not found
 */
const char *device_get_property(iot_device_t *device, const char *key)
{
    if (!device || !key) {
        return NULL;
    }

    for (u32 i = 0; i < device->num_properties; i++) {
        if (strcmp(device->properties[i], key) == 0) {
            return device->property_values[i];
        }
    }

    return NULL;
}

/**
 * device_remove_property - Remove a device property
 * @device: Device
 * @key: Property key
 *
 * Returns: 0 on success, error code on failure
 */
int device_remove_property(iot_device_t *device, const char *key)
{
    if (!device || !key) {
        return -1;
    }

    for (u32 i = 0; i < device->num_properties; i++) {
        if (strcmp(device->properties[i], key) == 0) {
            /* Shift remaining properties */
            for (u32 j = i; j < device->num_properties - 1; j++) {
                strncpy(device->properties[j], device->properties[j + 1],
                        sizeof(device->properties[j]) - 1);
                strncpy(device->property_values[j], device->property_values[j + 1],
                        sizeof(device->property_values[j]) - 1);
            }
            device->num_properties--;
            return 0;
        }
    }

    return -1;  /* Property not found */
}

/*===========================================================================*/
/*                         DEVICE COMMANDS                                   */
/*===========================================================================*/

/**
 * device_send_command - Send a command to a device
 * @device: Target device
 * @command: Command name
 * @data: Command data
 * @len: Data length
 *
 * Sends a command to the device. The actual delivery mechanism
 * depends on the device's connection type.
 *
 * Returns: 0 on success, error code on failure
 */
int device_send_command(iot_device_t *device, const char *command,
                        const void *data, size_t len)
{
    if (!device || !command) {
        return -1;
    }

    if (device->status != DEVICE_STATUS_ONLINE) {
        pr_err("Device %u is not online (status: %u)\n",
               device->device_id, device->status);
        return -1;
    }

    pr_info("Sending command '%s' to device %u (%zu bytes)\n",
            command, device->device_id, len);

    /* In a real implementation, this would:
     * - Format the command according to device protocol
     * - Send via appropriate transport (MQTT, CoAP, etc.)
     * - Wait for acknowledgment
     */

    /* Simulate command delivery */
    (void)data;
    (void)len;

    return 0;
}

/**
 * device_set_status - Set device status
 * @device: Device
 * @status: New status
 */
void device_set_status(iot_device_t *device, u32 status)
{
    if (!device) {
        return;
    }

    u32 old_status = device->status;
    device->status = status;
    device->last_seen = iot_get_timestamp();

    /* Notify status change */
    if (old_status != status) {
        const char *event;
        switch (status) {
            case DEVICE_STATUS_ONLINE:
                event = "online";
                if (device->on_connect) {
                    device->on_connect(device);
                }
                break;
            case DEVICE_STATUS_OFFLINE:
                event = "offline";
                if (device->on_disconnect) {
                    device->on_disconnect(device);
                }
                break;
            case DEVICE_STATUS_ERROR:
                event = "error";
                break;
            case DEVICE_STATUS_SLEEPING:
                event = "sleeping";
                break;
            case DEVICE_STATUS_UPDATING:
                event = "updating";
                break;
            default:
                event = "unknown";
                break;
        }
        notify_device_event(device, event);
    }
}

/*===========================================================================*/
/*                         DEVICE GROUPS                                     */
/*===========================================================================*/

/**
 * device_group_create - Create a device group
 * @name: Group name
 * @description: Group description
 *
 * Returns: Group ID, or 0 on failure
 */
u32 device_group_create(const char *name, const char *description)
{
    u32 group_id;

    if (!name || !g_device_registry.initialized) {
        return 0;
    }

    if (g_device_registry.num_groups >= MAX_DEVICE_GROUPS) {
        pr_err("Group limit reached\n");
        return 0;
    }

    spin_lock(&g_device_registry.lock);

    group_id = g_device_registry.num_groups + 1;
    struct {
        u32 group_id;
        char name[64];
        char description[256];
        u32 *device_ids;
        u32 num_devices;
    } *group = &g_device_registry.groups[g_device_registry.num_groups];

    group->group_id = group_id;
    strncpy(group->name, name, sizeof(group->name) - 1);
    if (description) {
        strncpy(group->description, description, sizeof(group->description) - 1);
    }
    group->device_ids = NULL;
    group->num_devices = 0;

    g_device_registry.num_groups++;

    pr_info("Device group created: %s (ID: %u)\n", name, group_id);

    spin_unlock(&g_device_registry.lock);

    return group_id;
}

/**
 * device_group_add_device - Add a device to a group
 * @group_id: Group ID
 * @device_id: Device ID
 *
 * Returns: 0 on success, error code on failure
 */
int device_group_add_device(u32 group_id, u32 device_id)
{
    struct {
        u32 group_id;
        char name[64];
        char description[256];
        u32 *device_ids;
        u32 num_devices;
    } *group;
    u32 *new_ids;

    if (!g_device_registry.initialized) {
        return -1;
    }

    if (group_id == 0 || group_id > g_device_registry.num_groups) {
        return -1;
    }

    spin_lock(&g_device_registry.lock);

    group = &g_device_registry.groups[group_id - 1];

    /* Check if already in group */
    for (u32 i = 0; i < group->num_devices; i++) {
        if (group->device_ids[i] == device_id) {
            spin_unlock(&g_device_registry.lock);
            return 0;  /* Already in group */
        }
    }

    /* Reallocate device IDs array */
    new_ids = (u32 *)krealloc(group->device_ids,
                               (group->num_devices + 1) * sizeof(u32));
    if (!new_ids) {
        spin_unlock(&g_device_registry.lock);
        return -1;
    }
    group->device_ids = new_ids;

    group->device_ids[group->num_devices] = device_id;
    group->num_devices++;

    /* Update device's group list */
    iot_device_t *device = device_find_by_id(device_id);
    if (device && device->num_groups < 8) {
        device->group_ids[device->num_groups] = group_id;
        device->num_groups++;
    }

    pr_info("Device %u added to group %u\n", device_id, group_id);

    spin_unlock(&g_device_registry.lock);

    return 0;
}

/**
 * device_group_remove_device - Remove a device from a group
 * @group_id: Group ID
 * @device_id: Device ID
 *
 * Returns: 0 on success, error code on failure
 */
int device_group_remove_device(u32 group_id, u32 device_id)
{
    struct {
        u32 group_id;
        char name[64];
        char description[256];
        u32 *device_ids;
        u32 num_devices;
    } *group;

    if (!g_device_registry.initialized) {
        return -1;
    }

    if (group_id == 0 || group_id > g_device_registry.num_groups) {
        return -1;
    }

    spin_lock(&g_device_registry.lock);

    group = &g_device_registry.groups[group_id - 1];

    /* Find and remove device */
    for (u32 i = 0; i < group->num_devices; i++) {
        if (group->device_ids[i] == device_id) {
            /* Shift remaining IDs */
            for (u32 j = i; j < group->num_devices - 1; j++) {
                group->device_ids[j] = group->device_ids[j + 1];
            }
            group->num_devices--;

            pr_info("Device %u removed from group %u\n", device_id, group_id);
            spin_unlock(&g_device_registry.lock);
            return 0;
        }
    }

    spin_unlock(&g_device_registry.lock);
    return -1;  /* Device not in group */
}

/*===========================================================================*/
/*                         DEVICE DISCOVERY                                  */
/*===========================================================================*/

/**
 * device_discover - Discover devices on the network
 * @timeout_ms: Discovery timeout in milliseconds
 *
 * Performs network discovery to find IoT devices.
 *
 * Returns: Number of devices discovered, or error code on failure
 */
int device_discover(u32 timeout_ms)
{
    /* In a real implementation, this would:
     * - Send discovery packets (mDNS, SSDP, etc.)
     * - Listen for responses
     * - Parse device information
     * - Add to discovered devices list
     */

    (void)timeout_ms;

    if (!g_device_registry.initialized) {
        return -1;
    }

    spin_lock(&g_device_registry.lock);

    g_device_registry.last_discovery = iot_get_timestamp();

    pr_info("Device discovery completed\n");

    spin_unlock(&g_device_registry.lock);

    return g_device_registry.num_discovered;
}

/**
 * device_get_discovered - Get discovered devices
 * @count: Output count
 *
 * Returns: Array of discovered devices
 */
iot_device_t *device_get_discovered(u32 *count)
{
    if (!count) {
        return NULL;
    }

    if (!g_device_registry.initialized) {
        *count = 0;
        return NULL;
    }

    spin_lock(&g_device_registry.lock);
    *count = g_device_registry.num_discovered;
    iot_device_t *devices = g_device_registry.discovered_devices;
    spin_unlock(&g_device_registry.lock);

    return devices;
}

/*===========================================================================*/
/*                         HELPER FUNCTIONS                                  */
/*===========================================================================*/

/**
 * generate_device_id - Generate a unique device ID
 *
 * Returns: Unique device ID
 */
static u32 generate_device_id(void)
{
    return g_device_registry.next_device_id++;
}

/**
 * find_device_in_list - Find device by ID in a linked list
 * @list: List head
 * @device_id: Device ID
 *
 * Returns: Pointer to device, or NULL if not found
 */
static iot_device_t *find_device_in_list(iot_device_t *list, u32 device_id)
{
    iot_device_t *device = list;
    while (device) {
        if (device->device_id == device_id) {
            return device;
        }
        device = device->next;
    }
    return NULL;
}

/**
 * find_device_by_name_in_list - Find device by name in a linked list
 * @list: List head
 * @name: Device name
 *
 * Returns: Pointer to device, or NULL if not found
 */
static iot_device_t *find_device_by_name_in_list(iot_device_t *list, const char *name)
{
    iot_device_t *device = list;
    while (device) {
        if (strcmp(device->name, name) == 0) {
            return device;
        }
        device = device->next;
    }
    return NULL;
}

/**
 * notify_device_event - Notify about a device event
 * @device: Device
 * @event: Event name
 *
 * Returns: 0 on success
 */
static int notify_device_event(iot_device_t *device, const char *event)
{
    pr_debug("Device event: %s - %s\n", device->name, event);

    /* In a real implementation, this would:
     * - Send event to event bus
     * - Notify subscribed handlers
     * - Log event to audit log
     */

    (void)device;
    (void)event;

    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * device_registry_stats - Get registry statistics
 * @num_devices: Output device count
 * @num_groups: Output group count
 * @num_discovered: Output discovered count
 */
void device_registry_stats(u32 *num_devices, u32 *num_groups, u32 *num_discovered)
{
    if (!g_device_registry.initialized) {
        if (num_devices) *num_devices = 0;
        if (num_groups) *num_groups = 0;
        if (num_discovered) *num_discovered = 0;
        return;
    }

    spin_lock(&g_device_registry.lock);

    if (num_devices) *num_devices = g_device_registry.num_devices;
    if (num_groups) *num_groups = g_device_registry.num_groups;
    if (num_discovered) *num_discovered = g_device_registry.num_discovered;

    spin_unlock(&g_device_registry.lock);
}
