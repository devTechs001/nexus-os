/*
 * NEXUS OS - Storage Management UI Implementation
 * gui/storage/storage-manager.c
 *
 * Advanced storage management with real-time monitoring and SMART visualization
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "storage-manager.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/config.h"
#include "../../drivers/storage/nvme.h"
#include "../../drivers/storage/ahci.h"
#include "../../drivers/storage/sd.h"

/*===========================================================================*/
/*                         GLOBAL STORAGE MANAGER INSTANCE                   */
/*===========================================================================*/

static storage_manager_t g_storage_manager;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* UI Creation */
static int create_main_window(storage_manager_t *manager);
static int create_sidebar(storage_manager_t *manager);
static int create_content_area(storage_manager_t *manager);
static int create_device_list(storage_manager_t *manager);
static int create_device_info_panel(storage_manager_t *manager);
static int create_smart_panel(storage_manager_t *manager);
static int create_stats_panel(storage_manager_t *manager);
static int create_partition_panel(storage_manager_t *manager);
static int create_toolbar(storage_manager_t *manager);
static int create_status_bar(storage_manager_t *manager);

/* Device Operations */
static int enumerate_devices(storage_manager_t *manager);
static int update_device_info(storage_manager_t *manager, u32 device_id);
static int select_device(storage_manager_t *manager, u32 device_id);

/* SMART Operations */
static int update_smart_data(storage_manager_t *manager, u32 device_id);
static int display_smart_attributes(storage_manager_t *manager);

/* Statistics */
static int update_statistics(storage_manager_t *manager, u32 device_id);
static int update_graphs(storage_manager_t *manager);

/* Partition Operations */
static int enumerate_partitions(storage_manager_t *manager);
static int display_partitions(storage_manager_t *manager);

/* Event Handlers */
static void on_device_selected(widget_t *widget);
static void on_refresh_clicked(widget_t *button);
static void on_format_clicked(widget_t *button);
static void on_mount_clicked(widget_t *button);
static void on_unmount_clicked(widget_t *button);
static void on_smart_test_clicked(widget_t *button);
static void on_trim_clicked(widget_t *button);

/*===========================================================================*/
/*                         MANAGER INITIALIZATION                            */
/*===========================================================================*/

/**
 * storage_manager_init - Initialize storage manager
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int storage_manager_init(storage_manager_t *manager)
{
    if (!manager) {
        return -EINVAL;
    }
    
    printk("[STORAGE] ========================================\n");
    printk("[STORAGE] NEXUS OS Storage Manager v%s\n", STORAGE_MANAGER_VERSION);
    printk("[STORAGE] ========================================\n");
    printk("[STORAGE] Initializing storage manager...\n");
    
    /* Clear structure */
    memset(manager, 0, sizeof(storage_manager_t));
    manager->initialized = true;
    manager->device_count = 0;
    manager->partition_count = 0;
    manager->selected_device = 0;
    manager->update_interval = STORAGE_UPDATE_INTERVAL_MS;
    manager->auto_refresh = true;
    
    /* Default display options */
    manager->show_smart = true;
    manager->show_stats = true;
    manager->show_partitions = true;
    manager->show_graphs = true;
    manager->graph_type = 0;  /* IOPS graph */
    
    /* Default alert thresholds */
    manager->alerts_enabled = true;
    manager->temp_warning_threshold = 50;   /* 50°C */
    manager->temp_critical_threshold = 65;  /* 65°C */
    manager->health_warning_threshold = 70; /* 70% */
    manager->space_warning_threshold = 90;  /* 90% */
    
    /* Initialize storage drivers */
    printk("[STORAGE] Initializing storage drivers...\n");
    nvme_init();
    ahci_init();
    sd_init();
    
    /* Enumerate devices */
    printk("[STORAGE] Enumerating storage devices...\n");
    enumerate_devices(manager);
    
    /* Enumerate partitions */
    enumerate_partitions(manager);
    
    /* Create UI */
    printk("[STORAGE] Creating UI...\n");
    if (create_main_window(manager) != 0) {
        printk("[STORAGE] Failed to create main window\n");
        return -1;
    }
    
    if (create_sidebar(manager) != 0) {
        printk("[STORAGE] Failed to create sidebar\n");
        return -1;
    }
    
    if (create_content_area(manager) != 0) {
        printk("[STORAGE] Failed to create content area\n");
        return -1;
    }
    
    if (create_device_list(manager) != 0) {
        printk("[STORAGE] Failed to create device list\n");
        return -1;
    }
    
    if (create_device_info_panel(manager) != 0) {
        printk("[STORAGE] Failed to create device info panel\n");
        return -1;
    }
    
    if (create_toolbar(manager) != 0) {
        printk("[STORAGE] Failed to create toolbar\n");
        return -1;
    }
    
    if (create_status_bar(manager) != 0) {
        printk("[STORAGE] Failed to create status bar\n");
        return -1;
    }
    
    /* Select first device if available */
    if (manager->device_count > 0) {
        select_device(manager, 0);
    }
    
    /* Update display */
    storage_manager_update_ui(manager);
    
    printk("[STORAGE] Storage manager initialized\n");
    printk("[STORAGE] Found %d device(s) and %d partition(s)\n", 
           manager->device_count, manager->partition_count);
    printk("[STORAGE] ========================================\n");
    
    return 0;
}

/**
 * storage_manager_run - Start storage manager
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int storage_manager_run(storage_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return -EINVAL;
    }
    
    printk("[STORAGE] Starting storage manager...\n");
    
    /* Show main window */
    if (manager->main_window) {
        window_show(manager->main_window);
    }
    
    /* Start monitoring */
    manager->monitoring_active = true;
    manager->last_update = get_time_ms();
    
    return 0;
}

/**
 * storage_manager_shutdown - Shutdown storage manager
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int storage_manager_shutdown(storage_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return -EINVAL;
    }
    
    printk("[STORAGE] Shutting down storage manager...\n");
    
    manager->monitoring_active = false;
    
    /* Hide main window */
    if (manager->main_window) {
        window_hide(manager->main_window);
    }
    
    /* Shutdown storage drivers */
    sd_shutdown();
    ahci_shutdown();
    nvme_shutdown();
    
    manager->initialized = false;
    
    printk("[STORAGE] Storage manager shutdown complete\n");
    
    return 0;
}

/**
 * storage_manager_refresh - Refresh storage information
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int storage_manager_refresh(storage_manager_t *manager)
{
    if (!manager) {
        return -EINVAL;
    }
    
    printk("[STORAGE] Refreshing storage information...\n");
    
    /* Re-enumerate devices */
    enumerate_devices(manager);
    enumerate_partitions(manager);
    
    /* Update UI */
    storage_manager_update_ui(manager);
    
    return 0;
}

/*===========================================================================*/
/*                         DEVICE ENUMERATION                                */
/*===========================================================================*/

/**
 * enumerate_devices - Enumerate all storage devices
 * @manager: Pointer to storage manager structure
 *
 * Returns: Number of devices found, or negative error code
 */
static int enumerate_devices(storage_manager_t *manager)
{
    if (!manager) {
        return -EINVAL;
    }
    
    manager->device_count = 0;
    
    /* Enumerate NVMe devices */
    nvme_device_t nvme_devices[16];
    u32 nvme_count = 16;
    
    if (nvme_is_initialized()) {
        nvme_list_devices(nvme_devices, &nvme_count);
        
        for (u32 i = 0; i < nvme_count && manager->device_count < STORAGE_MAX_DEVICES; i++) {
            storage_device_t *dev = &manager->devices[manager->device_count++];
            
            dev->device_id = i;
            dev->controller_id = 0;
            snprintf(dev->name, sizeof(dev->name), "nvme%d", i);
            snprintf(dev->device_path, sizeof(dev->device_path), "/dev/nvme%dn1", i);
            dev->device_type = STORAGE_TYPE_NVME;
            
            /* Get device info from NVMe driver */
            nvme_device_t *nvme_dev = nvme_get_device(i);
            if (nvme_dev) {
                strncpy(dev->model, nvme_dev->ctrl.mn, sizeof(dev->model) - 1);
                strncpy(dev->serial, nvme_dev->ctrl.sn, sizeof(dev->serial) - 1);
                strncpy(dev->firmware, nvme_dev->ctrl.fr, sizeof(dev->firmware) - 1);
                dev->total_capacity = nvme_block_get_size(i);
                dev->block_size = nvme_block_get_block_size(i);
                dev->supports_trim = true;
                dev->supports_smart = true;
                
                /* Get SMART data */
                nvme_get_smart_log(nvme_dev);
                dev->smart.supported = true;
                dev->smart.temperature = nvme_dev->smart.temperature - 273;  /* Convert to Celsius */
                dev->smart.power_on_hours = nvme_dev->smart.power_on_hours[0];
                dev->smart.host_reads = nvme_dev->smart.host_reads[0];
                dev->smart.host_writes = nvme_dev->smart.host_writes[0];
                dev->smart.percentage_used = nvme_dev->smart.percent_used;
                dev->smart.available_spare = nvme_dev->smart.avail_spare;
                dev->smart.healthy = nvme_is_healthy(nvme_dev);
                
                /* Calculate health percentage */
                if (nvme_dev->smart.percent_used <= 100) {
                    dev->health_percent = 100 - nvme_dev->smart.percent_used;
                } else {
                    dev->health_percent = 0;
                }
                
                /* Set health status */
                if (dev->health_percent >= 90) {
                    dev->health_status = HEALTH_STATUS_GOOD;
                } else if (dev->health_percent >= 70) {
                    dev->health_status = HEALTH_STATUS_FAIR;
                } else if (dev->health_percent >= 40) {
                    dev->health_status = HEALTH_STATUS_POOR;
                } else if (dev->health_percent >= 10) {
                    dev->health_status = HEALTH_STATUS_CRITICAL;
                } else {
                    dev->health_status = HEALTH_STATUS_FAILED;
                }
                
                dev->temperature = dev->smart.temperature;
                dev->power_on_hours = dev->smart.power_on_hours;
                dev->power_cycle_count = nvme_dev->smart.power_cycles[0];
            }
            
            dev->is_present = true;
            dev->is_active = true;
            
            printk("[STORAGE] Found NVMe: %s - %d MB\n", 
                   dev->name, (u32)(dev->total_capacity / (1024 * 1024)));
        }
    }
    
    /* Enumerate AHCI devices */
    ahci_controller_t ahci_ctrls[8];
    u32 ahci_count = 8;
    
    if (ahci_is_initialized()) {
        ahci_list_controllers(ahci_ctrls, &ahci_count);
        
        for (u32 c = 0; c < ahci_count; c++) {
            for (u32 p = 0; p < ahci_ctrls[c].active_ports && 
                 manager->device_count < STORAGE_MAX_DEVICES; p++) {
                
                storage_device_t *dev = &manager->devices[manager->device_count++];
                u32 dev_idx = manager->device_count - 1;
                
                dev->device_id = dev_idx;
                dev->controller_id = c;
                snprintf(dev->name, sizeof(dev->name), "sd%c", 'a' + dev_idx);
                snprintf(dev->device_path, sizeof(dev->device_path), "/dev/sd%c", 'a' + dev_idx);
                dev->device_type = STORAGE_TYPE_SATA;
                
                /* Get device info from AHCI driver */
                u64 size = ahci_block_get_size(c, p);
                if (size > 0) {
                    dev->total_capacity = size;
                    dev->block_size = ahci_block_get_block_size(c, p);
                    dev->supports_trim = true;
                    dev->supports_smart = true;
                    
                    /* Determine if SSD or HDD */
                    /* In real implementation, would check rotation rate */
                    if (size > 2000ULL * 1024 * 1024 * 1024) {
                        dev->device_type = STORAGE_TYPE_HDD;
                        dev->rotation_rate = 7200;
                    } else {
                        dev->device_type = STORAGE_TYPE_SSD;
                        dev->rotation_rate = 0;
                    }
                    
                    dev->is_present = true;
                    dev->is_active = true;
                    
                    printk("[STORAGE] Found SATA: %s - %d MB\n", 
                           dev->name, (u32)(dev->total_capacity / (1024 * 1024)));
                }
            }
        }
    }
    
    /* Enumerate SD/eMMC devices */
    sd_device_t sd_devices[8];
    u32 sd_count = 8;
    
    if (sd_is_initialized()) {
        sd_list_devices(sd_devices, &sd_count);
        
        for (u32 i = 0; i < sd_count && manager->device_count < STORAGE_MAX_DEVICES; i++) {
            storage_device_t *dev = &manager->devices[manager->device_count++];
            
            dev->device_id = i;
            dev->controller_id = 0;
            snprintf(dev->name, sizeof(dev->name), "mmcblk%d", i);
            snprintf(dev->device_path, sizeof(dev->device_path), "/dev/mmcblk%d", i);
            dev->device_type = sd_devices[i].card_type == CARD_TYPE_EMMC ? 
                              STORAGE_TYPE_EMMC : STORAGE_TYPE_EXTERNAL;
            dev->total_capacity = sd_block_get_size(i);
            dev->block_size = sd_block_get_block_size(i);
            dev->is_present = sd_devices[i].is_present;
            dev->is_active = sd_devices[i].is_initialized;
            
            printk("[STORAGE] Found SD/eMMC: %s - %d MB\n", 
                   dev->name, (u32)(dev->total_capacity / (1024 * 1024)));
        }
    }
    
    return manager->device_count;
}

/**
 * select_device - Select a storage device
 * @manager: Pointer to storage manager structure
 * @device_id: Device ID to select
 *
 * Returns: 0 on success, negative error code on failure
 */
static int select_device(storage_manager_t *manager, u32 device_id)
{
    if (!manager || device_id >= manager->device_count) {
        return -EINVAL;
    }
    
    manager->selected_device = device_id;
    
    /* Update SMART data */
    update_smart_data(manager, device_id);
    
    /* Update statistics */
    update_statistics(manager, device_id);
    
    /* Update display */
    display_smart_attributes(manager);
    display_partitions(manager);
    
    /* Call callback */
    if (manager->on_device_selected) {
        manager->on_device_selected(manager, device_id);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         SMART OPERATIONS                                  */
/*===========================================================================*/

/**
 * update_smart_data - Update SMART data for device
 * @manager: Pointer to storage manager structure
 * @device_id: Device ID
 *
 * Returns: 0 on success, negative error code on failure
 */
static int update_smart_data(storage_manager_t *manager, u32 device_id)
{
    if (!manager || device_id >= manager->device_count) {
        return -EINVAL;
    }
    
    storage_device_t *dev = &manager->devices[device_id];
    
    /* Update SMART from driver */
    if (dev->device_type == STORAGE_TYPE_NVME) {
        nvme_device_t *nvme_dev = nvme_get_device(dev->device_id);
        if (nvme_dev) {
            nvme_get_smart_log(nvme_dev);
            dev->smart.temperature = nvme_dev->smart.temperature - 273;
            dev->smart.power_on_hours = nvme_dev->smart.power_on_hours[0];
            dev->smart.host_reads = nvme_dev->smart.host_reads[0];
            dev->smart.host_writes = nvme_dev->smart.host_writes[0];
            dev->smart.percentage_used = nvme_dev->smart.percent_used;
            dev->smart.available_spare = nvme_dev->smart.avail_spare;
            dev->smart.healthy = nvme_is_healthy(nvme_dev);
            dev->smart.media_errors = nvme_dev->smart.media_errors[0];
            dev->smart.error_count = nvme_dev->errors;
        }
    }
    
    return 0;
}

/**
 * display_smart_attributes - Display SMART attributes
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
static int display_smart_attributes(storage_manager_t *manager)
{
    if (!manager || !manager->smart_panel) {
        return 0;
    }
    
    storage_device_t *dev = &manager->devices[manager->selected_device];
    
    /* Update SMART display widgets */
    /* In real implementation, would update labels and graphs */
    
    printk("[STORAGE] Displaying SMART data for %s\n", dev->name);
    printk("  Temperature: %d°C\n", dev->smart.temperature);
    printk("  Power On Hours: %d\n", dev->smart.power_on_hours);
    printk("  Host Reads: %d\n", (u32)dev->smart.host_reads);
    printk("  Host Writes: %d\n", (u32)dev->smart.host_writes);
    printk("  Available Spare: %d%%\n", dev->smart.available_spare);
    printk("  Percentage Used: %d%%\n", dev->smart.percentage_used);
    printk("  Health: %s\n", dev->smart.healthy ? "Good" : "Warning");
    
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * update_statistics - Update I/O statistics for device
 * @manager: Pointer to storage manager structure
 * @device_id: Device ID
 *
 * Returns: 0 on success
 */
static int update_statistics(storage_manager_t *manager, u32 device_id)
{
    if (!manager || device_id >= manager->device_count) {
        return -EINVAL;
    }
    
    storage_device_t *dev = &manager->devices[device_id];
    u64 now = get_time_ms();
    
    /* Get stats from driver */
    if (dev->device_type == STORAGE_TYPE_NVME) {
        nvme_device_t *nvme_dev = nvme_get_device(dev->device_id);
        if (nvme_dev) {
            dev->stats.reads = nvme_dev->reads;
            dev->stats.writes = nvme_dev->writes;
            dev->stats.read_bytes = nvme_dev->read_bytes;
            dev->stats.write_bytes = nvme_dev->write_bytes;
        }
    }
    
    /* Calculate IOPS and throughput */
    u64 elapsed = now - dev->stats.timestamp;
    if (elapsed > 0) {
        /* Calculate samples for history */
        u32 sample_idx = dev->history_index % STORAGE_HISTORY_SAMPLES;
        
        dev->history[sample_idx].timestamp = now;
        dev->history[sample_idx].temperature = dev->temperature;
        dev->history[sample_idx].utilization = dev->stats.utilization;
        
        dev->history_index++;
    }
    
    dev->stats.timestamp = now;
    
    return 0;
}

/**
 * update_graphs - Update I/O graphs
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
static int update_graphs(storage_manager_t *manager)
{
    if (!manager || !manager->show_graphs) {
        return 0;
    }
    
    /* In real implementation, would update graph widgets */
    /* with IOPS, throughput, temperature history */
    
    return 0;
}

/*===========================================================================*/
/*                         PARTITION OPERATIONS                              */
/*===========================================================================*/

/**
 * enumerate_partitions - Enumerate all partitions
 * @manager: Pointer to storage manager structure
 *
 * Returns: Number of partitions found
 */
static int enumerate_partitions(storage_manager_t *manager)
{
    if (!manager) {
        return -EINVAL;
    }
    
    manager->partition_count = 0;
    
    /* In real implementation, would read partition table */
    /* For now, create mock partitions */
    
    for (u32 d = 0; d < manager->device_count && 
         manager->partition_count < STORAGE_MAX_PARTITIONS; d++) {
        
        storage_device_t *dev = &manager->devices[d];
        
        /* Create root partition mock */
        if (manager->partition_count < STORAGE_MAX_PARTITIONS) {
            partition_info_t *part = &manager->partitions[manager->partition_count++];
            
            part->partition_id = manager->partition_count - 1;
            part->device_id = d;
            snprintf(part->name, sizeof(part->name), "%s1", dev->name);
            snprintf(part->device_path, sizeof(part->device_path), "%s1", dev->device_path);
            strcpy(part->mount_point, "/");
            strcpy(part->label, "ROOT");
            part->start_sector = 2048;
            part->size_bytes = dev->total_capacity * 80 / 100;  /* 80% for root */
            part->used_bytes = part->size_bytes * 40 / 100;  /* 40% used */
            part->free_bytes = part->size_bytes - part->used_bytes;
            part->fs_type = FS_TYPE_EXT4;
            part->partition_type = PARTITION_TYPE_PRIMARY;
            part->is_mounted = true;
            part->is_root = true;
            part->is_boot = true;
            part->usage_percent = 40;
            
            snprintf(part->uuid, sizeof(part->uuid), "12345678-1234-1234-1234-12345678901%d", d);
        }
        
        /* Create home partition mock */
        if (manager->partition_count < STORAGE_MAX_PARTITIONS) {
            partition_info_t *part = &manager->partitions[manager->partition_count++];
            
            part->partition_id = manager->partition_count - 1;
            part->device_id = d;
            snprintf(part->name, sizeof(part->name), "%s2", dev->name);
            snprintf(part->device_path, sizeof(part->device_path), "%s2", dev->device_path);
            strcpy(part->mount_point, "/home");
            strcpy(part->label, "HOME");
            part->start_sector = dev->total_capacity / 512 * 80 / 100;
            part->size_bytes = dev->total_capacity * 20 / 100;  /* 20% for home */
            part->used_bytes = part->size_bytes * 30 / 100;  /* 30% used */
            part->free_bytes = part->size_bytes - part->used_bytes;
            part->fs_type = FS_TYPE_EXT4;
            part->partition_type = PARTITION_TYPE_PRIMARY;
            part->is_mounted = true;
            part->is_boot = false;
            part->is_root = false;
            part->usage_percent = 30;
            
            snprintf(part->uuid, sizeof(part->uuid), "87654321-4321-4321-4321-21098765432%d", d);
        }
    }
    
    return manager->partition_count;
}

/**
 * display_partitions - Display partition information
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
static int display_partitions(storage_manager_t *manager)
{
    if (!manager || !manager->partition_panel) {
        return 0;
    }
    
    printk("[STORAGE] Displaying %d partitions\n", manager->partition_count);
    
    for (u32 i = 0; i < manager->partition_count; i++) {
        partition_info_t *part = &manager->partitions[i];
        printk("  %s: %s mounted at %s (%d%% used)\n",
               part->name, part->label, part->mount_point, part->usage_percent);
    }
    
    return 0;
}

/*===========================================================================*/
/*                         UI CREATION                                       */
/*===========================================================================*/

/**
 * create_main_window - Create main storage manager window
 */
static int create_main_window(storage_manager_t *manager)
{
    window_props_t props;
    memset(&props, 0, sizeof(window_props_t));
    
    strcpy(props.title, "NEXUS Storage Manager");
    props.type = WINDOW_TYPE_NORMAL;
    props.flags = WINDOW_FLAG_RESIZABLE;
    props.bounds.x = 100;
    props.bounds.y = 100;
    props.bounds.width = 1100;
    props.bounds.height = 750;
    props.background = color_from_rgba(30, 30, 40, 255);
    
    manager->main_window = window_create(&props);
    if (!manager->main_window) {
        return -1;
    }
    
    manager->main_widget = panel_create(NULL, 0, 0, 1100, 750);
    if (!manager->main_widget) {
        return -1;
    }
    
    return 0;
}

/**
 * create_sidebar - Create device list sidebar
 */
static int create_sidebar(storage_manager_t *manager)
{
    manager->sidebar = panel_create(manager->main_widget, 0, 40, 250, 670);
    if (!manager->sidebar) {
        return -1;
    }
    
    widget_set_colors(manager->sidebar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(35, 35, 50, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    return 0;
}

/**
 * create_content_area - Create main content area
 */
static int create_content_area(storage_manager_t *manager)
{
    manager->content_area = panel_create(manager->main_widget, 250, 40, 850, 670);
    if (!manager->content_area) {
        return -1;
    }
    
    widget_set_colors(manager->content_area,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(45, 45, 60, 255),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/**
 * create_device_list - Create device list in sidebar
 */
static int create_device_list(storage_manager_t *manager)
{
    manager->device_list = panel_create(manager->sidebar, 0, 0, 250, 670);
    if (!manager->device_list) {
        return -1;
    }
    
    /* Device list items would be created dynamically */
    
    return 0;
}

/**
 * create_device_info_panel - Create device info panel
 */
static int create_device_info_panel(storage_manager_t *manager)
{
    manager->device_info_panel = panel_create(manager->content_area, 10, 10, 830, 150);
    if (!manager->device_info_panel) {
        return -1;
    }
    
    widget_set_colors(manager->device_info_panel,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(50, 50, 70, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Device info labels would be created here */
    
    return 0;
}

/**
 * create_smart_panel - Create SMART data panel
 */
static int create_smart_panel(storage_manager_t *manager)
{
    manager->smart_panel = panel_create(manager->content_area, 10, 170, 410, 250);
    if (!manager->smart_panel) {
        return -1;
    }
    
    widget_set_colors(manager->smart_panel,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(50, 50, 70, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* SMART attribute labels would be created here */
    
    return 0;
}

/**
 * create_stats_panel - Create statistics panel
 */
static int create_stats_panel(storage_manager_t *manager)
{
    manager->stats_panel = panel_create(manager->content_area, 430, 170, 410, 250);
    if (!manager->stats_panel) {
        return -1;
    }
    
    widget_set_colors(manager->stats_panel,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(50, 50, 70, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Statistics labels and graphs would be created here */
    
    return 0;
}

/**
 * create_partition_panel - Create partition list panel
 */
static int create_partition_panel(storage_manager_t *manager)
{
    manager->partition_panel = panel_create(manager->content_area, 10, 430, 830, 200);
    if (!manager->partition_panel) {
        return -1;
    }
    
    widget_set_colors(manager->partition_panel,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(50, 50, 70, 255),
                      color_from_rgba(80, 80, 100, 255));
    
    /* Partition list would be created here */
    
    return 0;
}

/**
 * create_toolbar - Create toolbar
 */
static int create_toolbar(storage_manager_t *manager)
{
    manager->toolbar = panel_create(manager->main_widget, 0, 0, 1100, 40);
    if (!manager->toolbar) {
        return -1;
    }
    
    widget_set_colors(manager->toolbar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(40, 40, 55, 255),
                      color_from_rgba(60, 60, 80, 255));
    
    /* Refresh button */
    widget_t *refresh_btn = button_create(manager->toolbar, "⟳ Refresh", 10, 5, 100, 30);
    if (refresh_btn) {
        refresh_btn->on_click = on_refresh_clicked;
        refresh_btn->user_data = manager;
    }
    
    /* Format button */
    widget_t *format_btn = button_create(manager->toolbar, "Format", 120, 5, 80, 30);
    if (format_btn) {
        format_btn->on_click = on_format_clicked;
        format_btn->user_data = manager;
    }
    
    /* Mount button */
    widget_t *mount_btn = button_create(manager->toolbar, "Mount", 210, 5, 80, 30);
    if (mount_btn) {
        mount_btn->on_click = on_mount_clicked;
        mount_btn->user_data = manager;
    }
    
    /* Unmount button */
    widget_t *unmount_btn = button_create(manager->toolbar, "Unmount", 300, 5, 90, 30);
    if (unmount_btn) {
        unmount_btn->on_click = on_unmount_clicked;
        unmount_btn->user_data = manager;
    }
    
    /* SMART test button */
    widget_t *smart_btn = button_create(manager->toolbar, "SMART Test", 400, 5, 100, 30);
    if (smart_btn) {
        smart_btn->on_click = on_smart_test_clicked;
        smart_btn->user_data = manager;
    }
    
    /* Trim button */
    widget_t *trim_btn = button_create(manager->toolbar, "Trim", 510, 5, 60, 30);
    if (trim_btn) {
        trim_btn->on_click = on_trim_clicked;
        trim_btn->user_data = manager;
    }
    
    return 0;
}

/**
 * create_status_bar - Create status bar
 */
static int create_status_bar(storage_manager_t *manager)
{
    manager->status_bar = panel_create(manager->main_widget, 0, 710, 1100, 40);
    if (!manager->status_bar) {
        return -1;
    }
    
    widget_set_colors(manager->status_bar,
                      color_from_rgba(255, 255, 255, 255),
                      color_from_rgba(25, 25, 35, 255),
                      color_from_rgba(50, 50, 65, 255));
    
    /* Status label */
    widget_t *status_label = label_create(manager->status_bar, "Ready", 10, 10, 400, 20);
    if (!status_label) {
        return -1;
    }
    
    widget_set_font(status_label, 0, 11);
    widget_set_colors(status_label,
                      color_from_rgba(150, 150, 160, 255),
                      color_from_rgba(0, 0, 0, 0),
                      color_from_rgba(0, 0, 0, 0));
    
    return 0;
}

/*===========================================================================*/
/*                         UI UPDATES                                        */
/*===========================================================================*/

/**
 * storage_manager_update_ui - Update entire UI
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_update_ui(storage_manager_t *manager)
{
    if (!manager) {
        return -EINVAL;
    }
    
    /* Update device list */
    storage_manager_update_device_list(manager);
    
    /* Update SMART display */
    if (manager->show_smart) {
        display_smart_attributes(manager);
    }
    
    /* Update statistics display */
    if (manager->show_stats) {
        update_graphs(manager);
    }
    
    /* Update partition display */
    if (manager->show_partitions) {
        display_partitions(manager);
    }
    
    /* Check alerts */
    if (manager->alerts_enabled) {
        storage_manager_check_alerts(manager);
    }
    
    return 0;
}

/**
 * storage_manager_update_device_list - Update device list display
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_update_device_list(storage_manager_t *manager)
{
    if (!manager || !manager->device_list) {
        return -EINVAL;
    }
    
    /* In real implementation, would update device list widgets */
    
    return 0;
}

/**
 * storage_manager_update_smart_display - Update SMART display
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_update_smart_display(storage_manager_t *manager)
{
    if (!manager || !manager->smart_panel) {
        return -EINVAL;
    }
    
    display_smart_attributes(manager);
    
    return 0;
}

/**
 * storage_manager_update_stats_display - Update statistics display
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_update_stats_display(storage_manager_t *manager)
{
    if (!manager || !manager->stats_panel) {
        return -EINVAL;
    }
    
    update_statistics(manager, manager->selected_device);
    
    return 0;
}

/**
 * storage_manager_update_graphs - Update graphs
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_update_graphs(storage_manager_t *manager)
{
    return update_graphs(manager);
}

/*===========================================================================*/
/*                         EVENT HANDLERS                                    */
/*===========================================================================*/

static void on_device_selected(widget_t *widget)
{
    if (!widget || !widget->user_data) return;
    /* Handle device selection */
}

static void on_refresh_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    
    storage_manager_t *manager = (storage_manager_t *)button->user_data;
    storage_manager_refresh(manager);
}

static void on_format_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    /* Handle format operation */
}

static void on_mount_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    /* Handle mount operation */
}

static void on_unmount_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    /* Handle unmount operation */
}

static void on_smart_test_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    /* Handle SMART test */
}

static void on_trim_clicked(widget_t *button)
{
    if (!button || !button->user_data) return;
    /* Handle TRIM operation */
}

/*===========================================================================*/
/*                         ALERT SYSTEM                                      */
/*===========================================================================*/

/**
 * storage_manager_check_alerts - Check for alerts
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_check_alerts(storage_manager_t *manager)
{
    if (!manager || !manager->alerts_enabled) {
        return 0;
    }
    
    for (u32 i = 0; i < manager->device_count; i++) {
        storage_device_t *dev = &manager->devices[i];
        
        /* Check temperature */
        if (dev->temperature >= manager->temp_critical_threshold) {
            storage_manager_add_alert(manager, i, "Critical temperature!");
        } else if (dev->temperature >= manager->temp_warning_threshold) {
            storage_manager_add_alert(manager, i, "High temperature warning");
        }
        
        /* Check health */
        if (dev->health_percent < manager->health_warning_threshold) {
            storage_manager_add_alert(manager, i, "Drive health degraded");
        }
        
        /* Check partition space */
        for (u32 p = 0; p < manager->partition_count; p++) {
            if (manager->partitions[p].device_id == i) {
                if (manager->partitions[p].usage_percent >= manager->space_warning_threshold) {
                    storage_manager_add_alert(manager, i, "Low disk space");
                }
            }
        }
    }
    
    return 0;
}

/**
 * storage_manager_add_alert - Add an alert
 * @manager: Pointer to storage manager structure
 * @device_id: Device ID
 * @message: Alert message
 *
 * Returns: 0 on success
 */
int storage_manager_add_alert(storage_manager_t *manager, u32 device_id, const char *message)
{
    if (!manager || !message) {
        return -EINVAL;
    }
    
    printk("[STORAGE ALERT] Device %d: %s\n", device_id, message);
    
    /* In real implementation, would show notification */
    
    return 0;
}

/**
 * storage_manager_clear_alerts - Clear all alerts
 * @manager: Pointer to storage manager structure
 *
 * Returns: 0 on success
 */
int storage_manager_clear_alerts(storage_manager_t *manager)
{
    (void)manager;
    return 0;
}

/*===========================================================================*/
/*                         PARTITION OPERATIONS                              */
/*===========================================================================*/

/**
 * storage_manager_mount_partition - Mount a partition
 */
int storage_manager_mount_partition(storage_manager_t *manager, u32 partition_id, 
                                    const char *mount_point)
{
    if (!manager || !mount_point || partition_id >= manager->partition_count) {
        return -EINVAL;
    }
    
    partition_info_t *part = &manager->partitions[partition_id];
    part->is_mounted = true;
    strncpy(part->mount_point, mount_point, sizeof(part->mount_point) - 1);
    
    if (manager->on_partition_mounted) {
        manager->on_partition_mounted(manager, partition_id);
    }
    
    return 0;
}

/**
 * storage_manager_unmount_partition - Unmount a partition
 */
int storage_manager_unmount_partition(storage_manager_t *manager, u32 partition_id)
{
    if (!manager || partition_id >= manager->partition_count) {
        return -EINVAL;
    }
    
    partition_info_t *part = &manager->partitions[partition_id];
    part->is_mounted = false;
    part->mount_point[0] = '\0';
    
    if (manager->on_partition_unmounted) {
        manager->on_partition_unmounted(manager, partition_id);
    }
    
    return 0;
}

/**
 * storage_manager_format_partition - Format a partition
 */
int storage_manager_format_partition(storage_manager_t *manager, u32 partition_id, u32 fs_type)
{
    if (!manager || partition_id >= manager->partition_count) {
        return -EINVAL;
    }
    
    partition_info_t *part = &manager->partitions[partition_id];
    part->fs_type = fs_type;
    part->used_bytes = 0;
    part->usage_percent = 0;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * storage_get_device_type_name - Get device type name
 */
const char *storage_get_device_type_name(u32 type)
{
    switch (type) {
        case STORAGE_TYPE_NVME:     return "NVMe";
        case STORAGE_TYPE_SATA:     return "SATA";
        case STORAGE_TYPE_SSD:      return "SSD";
        case STORAGE_TYPE_HDD:      return "HDD";
        case STORAGE_TYPE_EXTERNAL: return "External";
        case STORAGE_TYPE_RAID:     return "RAID";
        case STORAGE_TYPE_VIRTUAL:  return "Virtual";
        default:                    return "Unknown";
    }
}

/**
 * storage_get_fs_type_name - Get filesystem type name
 */
const char *storage_get_fs_type_name(u32 type)
{
    switch (type) {
        case FS_TYPE_NEXFS:   return "NexFS";
        case FS_TYPE_EXT4:    return "ext4";
        case FS_TYPE_EXT3:    return "ext3";
        case FS_TYPE_EXT2:    return "ext2";
        case FS_TYPE_BTRFS:   return "Btrfs";
        case FS_TYPE_XFS:     return "XFS";
        case FS_TYPE_FAT32:   return "FAT32";
        case FS_TYPE_NTFS:    return "NTFS";
        case FS_TYPE_EXFAT:   return "exFAT";
        case FS_TYPE_ZFS:     return "ZFS";
        default:              return "Unknown";
    }
}

/**
 * storage_get_health_status_name - Get health status name
 */
const char *storage_get_health_status_name(u32 status)
{
    switch (status) {
        case HEALTH_STATUS_GOOD:      return "Good";
        case HEALTH_STATUS_FAIR:      return "Fair";
        case HEALTH_STATUS_POOR:      return "Poor";
        case HEALTH_STATUS_CRITICAL:  return "Critical";
        case HEALTH_STATUS_FAILED:    return "Failed";
        default:                      return "Unknown";
    }
}

/**
 * storage_format_size - Format size in human-readable form
 */
const char *storage_format_size(u64 size, char *buffer, u32 buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return NULL;
    }
    
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    u32 unit_index = 0;
    double size_d = (double)size;
    
    while (size_d >= 1024.0 && unit_index < 5) {
        size_d /= 1024.0;
        unit_index++;
    }
    
    snprintf(buffer, buffer_size, "%.2f %s", size_d, units[unit_index]);
    
    return buffer;
}

/**
 * storage_calculate_usage_percent - Calculate usage percentage
 */
u32 storage_calculate_usage_percent(u64 used, u64 total)
{
    if (total == 0) {
        return 0;
    }
    
    return (u32)(used * 100 / total);
}

/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * storage_manager_get_instance - Get global storage manager instance
 */
storage_manager_t *storage_manager_get_instance(void)
{
    return &g_storage_manager;
}
