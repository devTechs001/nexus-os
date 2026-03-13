/*
 * NEXUS OS - CPU Cooler & Thermal Management
 * system/thermal/cpu_cooler.c
 *
 * CPU/GPU cooling, fan control, thermal throttling
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         THERMAL CONFIGURATION                             */
/*===========================================================================*/

#define THERMAL_MAX_ZONES         16
#define THERMAL_MAX_COOLERS       16
#define THERMAL_MAX_TRIPS         8

/* Thermal Trip Types */
#define THERMAL_TRIP_PASSIVE      0
#define THERMAL_TRIP_ACTIVE       1
#define THERMAL_TRIP_CRITICAL     2
#define THERMAL_TRIP_HOT          3

/* Fan Control Modes */
#define FAN_MODE_AUTO             0
#define FAN_MODE_SILENT           1
#define FAN_MODE_BALANCED         2
#define FAN_MODE_PERFORMANCE      3
#define FAN_MODE_MANUAL           4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 trip_id;
    u32 type;
    s32 temperature;            /* millidegrees C */
    u32 hysteresis;
    u32 cooling_device;
    bool is_triggered;
} thermal_trip_t;

typedef struct {
    u32 zone_id;
    char name[64];
    s32 temperature;            /* Current temp (millidegrees C) */
    s32 max_temp;
    s32 critical_temp;
    thermal_trip_t trips[THERMAL_MAX_TRIPS];
    u32 trip_count;
    bool is_enabled;
    void *private_data;
} thermal_zone_t;

typedef struct {
    u32 cooler_id;
    char name[64];
    u32 type;                   /* FAN, CPU_FREQ, GPU_FREQ, etc. */
    u32 max_state;
    u32 current_state;
    u32 target_state;
    bool is_enabled;
    void *private_data;
} cooling_device_t;

typedef struct {
    u32 fan_id;
    char name[64];
    u32 current_rpm;
    u32 target_rpm;
    u32 max_rpm;
    u32 min_rpm;
    u32 pwm_value;              /* 0-255 */
    u32 mode;
    bool is_enabled;
    u64 total_runtime;
} fan_controller_t;

typedef struct {
    bool initialized;
    thermal_zone_t zones[THERMAL_MAX_ZONES];
    u32 zone_count;
    cooling_device_t coolers[THERMAL_MAX_COOLERS];
    u32 cooler_count;
    fan_controller_t fans[8];
    u32 fan_count;
    u32 global_fan_mode;
    s32 cpu_temperature;
    s32 gpu_temperature;
    s32 motherboard_temperature;
    u32 thermal_throttling_events;
    spinlock_t lock;
} thermal_manager_t;

static thermal_manager_t g_thermal;

/*===========================================================================*/
/*                         THERMAL CORE FUNCTIONS                            */
/*===========================================================================*/

int thermal_init(void)
{
    printk("[THERMAL] ========================================\n");
    printk("[THERMAL] NEXUS OS Thermal Management\n");
    printk("[THERMAL] ========================================\n");
    
    memset(&g_thermal, 0, sizeof(thermal_manager_t));
    spinlock_init(&g_thermal.lock);
    
    g_thermal.global_fan_mode = FAN_MODE_AUTO;
    
    /* Register thermal zones */
    thermal_zone_t *zone;
    
    /* CPU Zone */
    zone = &g_thermal.zones[g_thermal.zone_count++];
    zone->zone_id = 1;
    strcpy(zone->name, "CPU");
    zone->max_temp = 85000;       /* 85°C */
    zone->critical_temp = 100000; /* 100°C */
    zone->is_enabled = true;
    
    /* Add trip points */
    zone->trips[0] = (thermal_trip_t){1, THERMAL_TRIP_PASSIVE, 60000, 5000, 0, false};
    zone->trips[1] = (thermal_trip_t){2, THERMAL_TRIP_ACTIVE, 75000, 5000, 0, false};
    zone->trips[2] = (thermal_trip_t){3, THERMAL_TRIP_CRITICAL, 95000, 2000, 0, false};
    zone->trip_count = 3;
    
    /* GPU Zone */
    zone = &g_thermal.zones[g_thermal.zone_count++];
    zone->zone_id = 2;
    strcpy(zone->name, "GPU");
    zone->max_temp = 90000;
    zone->critical_temp = 105000;
    zone->is_enabled = true;
    zone->trip_count = 2;
    
    /* Motherboard Zone */
    zone = &g_thermal.zones[g_thermal.zone_count++];
    zone->zone_id = 3;
    strcpy(zone->name, "Motherboard");
    zone->max_temp = 70000;
    zone->critical_temp = 85000;
    zone->is_enabled = true;
    zone->trip_count = 2;
    
    /* Register cooling devices */
    cooling_device_t *cooler;
    
    cooler = &g_thermal.coolers[g_thermal.cooler_count++];
    cooler->cooler_id = 1;
    strcpy(cooler->name, "CPU Fan");
    cooler->type = 0;  /* FAN */
    cooler->max_state = 255;
    cooler->is_enabled = true;
    
    cooler = &g_thermal.coolers[g_thermal.cooler_count++];
    cooler->cooler_id = 2;
    strcpy(cooler->name, "GPU Fan");
    cooler->type = 0;
    cooler->max_state = 255;
    cooler->is_enabled = true;
    
    cooler = &g_thermal.coolers[g_thermal.cooler_count++];
    cooler->cooler_id = 3;
    strcpy(cooler->name, "CPU Freq Scaling");
    cooler->type = 1;  /* CPU_FREQ */
    cooler->max_state = 100;
    cooler->is_enabled = true;
    
    /* Register fan controllers */
    fan_controller_t *fan;
    
    fan = &g_thermal.fans[g_thermal.fan_count++];
    fan->fan_id = 1;
    strcpy(fan->name, "CPU Fan");
    fan->max_rpm = 3000;
    fan->min_rpm = 500;
    fan->mode = FAN_MODE_AUTO;
    fan->is_enabled = true;
    
    fan = &g_thermal.fans[g_thermal.fan_count++];
    fan->fan_id = 2;
    strcpy(fan->name, "Case Fan");
    fan->max_rpm = 2000;
    fan->min_rpm = 400;
    fan->mode = FAN_MODE_AUTO;
    fan->is_enabled = true;
    
    printk("[THERMAL] Thermal manager initialized\n");
    printk("[THERMAL]   Zones: %d\n", g_thermal.zone_count);
    printk("[THERMAL]   Coolers: %d\n", g_thermal.cooler_count);
    printk("[THERMAL]   Fans: %d\n", g_thermal.fan_count);
    
    return 0;
}

/*===========================================================================*/
/*                         TEMPERATURE MONITORING                            */
/*===========================================================================*/

int thermal_get_cpu_temp(s32 *temp)
{
    if (!temp) return -EINVAL;
    
    /* In real implementation, would read from CPU thermal sensor */
    /* Mock: return typical temperature */
    g_thermal.cpu_temperature = 55000;  /* 55°C */
    *temp = g_thermal.cpu_temperature;
    
    return 0;
}

int thermal_get_gpu_temp(s32 *temp)
{
    if (!temp) return -EINVAL;
    
    g_thermal.gpu_temperature = 60000;  /* 60°C */
    *temp = g_thermal.gpu_temperature;
    
    return 0;
}

int thermal_get_all_temps(s32 *cpu, s32 *gpu, s32 *mobo)
{
    thermal_get_cpu_temp(cpu);
    thermal_get_gpu_temp(gpu);
    
    if (mobo) {
        g_thermal.motherboard_temperature = 45000;  /* 45°C */
        *mobo = g_thermal.motherboard_temperature;
    }
    
    return 0;
}

/*===========================================================================*/
/*                         FAN CONTROL                                       */
/*===========================================================================*/

int fan_set_mode(u32 fan_id, u32 mode)
{
    if (fan_id >= g_thermal.fan_count) return -EINVAL;
    if (mode > FAN_MODE_MANUAL) return -EINVAL;
    
    fan_controller_t *fan = &g_thermal.fans[fan_id];
    fan->mode = mode;
    
    const char *mode_names[] = {"Auto", "Silent", "Balanced", "Performance", "Manual"};
    printk("[FAN] %s mode set to %s\n", fan->name, mode_names[mode]);
    
    return 0;
}

int fan_set_speed(u32 fan_id, u32 rpm)
{
    if (fan_id >= g_thermal.fan_count) return -EINVAL;
    
    fan_controller_t *fan = &g_thermal.fans[fan_id];
    
    if (rpm < fan->min_rpm) rpm = fan->min_rpm;
    if (rpm > fan->max_rpm) rpm = fan->max_rpm;
    
    fan->target_rpm = rpm;
    fan->pwm_value = (rpm * 255) / fan->max_rpm;
    
    printk("[FAN] %s speed set to %d RPM (PWM: %d)\n", 
           fan->name, rpm, fan->pwm_value);
    
    return 0;
}

int fan_get_speed(u32 fan_id, u32 *rpm)
{
    if (fan_id >= g_thermal.fan_count || !rpm) return -EINVAL;
    
    *rpm = g_thermal.fans[fan_id].current_rpm;
    return 0;
}

static void thermal_update_fans(void)
{
    s32 cpu_temp, gpu_temp;
    thermal_get_all_temps(&cpu_temp, &gpu_temp, NULL);
    
    for (u32 i = 0; i < g_thermal.fan_count; i++) {
        fan_controller_t *fan = &g_thermal.fans[i];
        
        if (!fan->is_enabled || fan->mode != FAN_MODE_AUTO) continue;
        
        /* Calculate target RPM based on temperature */
        u32 target_rpm;
        
        if (cpu_temp > 80000 || gpu_temp > 85000) {
            /* High temp - max performance */
            target_rpm = fan->max_rpm;
        } else if (cpu_temp > 65000 || gpu_temp > 70000) {
            /* Medium temp - balanced */
            target_rpm = fan->min_rpm + (fan->max_rpm - fan->min_rpm) * 70 / 100;
        } else if (cpu_temp > 50000 || gpu_temp > 55000) {
            /* Normal temp - quiet */
            target_rpm = fan->min_rpm + (fan->max_rpm - fan->min_rpm) * 40 / 100;
        } else {
            /* Low temp - silent */
            target_rpm = fan->min_rpm;
        }
        
        fan->target_rpm = target_rpm;
        fan->pwm_value = (target_rpm * 255) / fan->max_rpm;
    }
}

/*===========================================================================*/
/*                         THERMAL THROTTLING                                */
/*===========================================================================*/

int thermal_check_throttling(void)
{
    s32 cpu_temp, gpu_temp;
    thermal_get_all_temps(&cpu_temp, &gpu_temp, NULL);
    
    bool throttling_needed = false;
    
    /* Check CPU thermal trips */
    if (cpu_temp >= 95000) {
        printk("[THERMAL] WARNING: CPU critical temperature! (%d°C)\n", cpu_temp / 1000);
        throttling_needed = true;
        g_thermal.thermal_throttling_events++;
    }
    
    /* Check GPU thermal trips */
    if (gpu_temp >= 100000) {
        printk("[THERMAL] WARNING: GPU critical temperature! (%d°C)\n", gpu_temp / 1000);
        throttling_needed = true;
        g_thermal.thermal_throttling_events++;
    }
    
    if (throttling_needed) {
        /* Reduce CPU/GPU frequencies */
        /* Increase fan speeds */
        thermal_update_fans();
    }
    
    return throttling_needed ? 1 : 0;
}

/*===========================================================================*/
/*                         COOLING PROFILES                                  */
/*===========================================================================*/

int thermal_set_profile(const char *profile)
{
    if (strcmp(profile, "silent") == 0) {
        g_thermal.global_fan_mode = FAN_MODE_SILENT;
        for (u32 i = 0; i < g_thermal.fan_count; i++) {
            g_thermal.fans[i].target_rpm = g_thermal.fans[i].min_rpm;
        }
        printk("[THERMAL] Profile: Silent\n");
    } else if (strcmp(profile, "balanced") == 0) {
        g_thermal.global_fan_mode = FAN_MODE_BALANCED;
        printk("[THERMAL] Profile: Balanced\n");
    } else if (strcmp(profile, "performance") == 0) {
        g_thermal.global_fan_mode = FAN_MODE_PERFORMANCE;
        for (u32 i = 0; i < g_thermal.fan_count; i++) {
            g_thermal.fans[i].target_rpm = g_thermal.fans[i].max_rpm;
        }
        printk("[THERMAL] Profile: Performance\n");
    } else {
        return -EINVAL;
    }
    
    thermal_update_fans();
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

int thermal_get_stats(u32 *throttling_events, u64 *fan_runtime)
{
    if (throttling_events) *throttling_events = g_thermal.thermal_throttling_events;
    
    if (fan_runtime) {
        *fan_runtime = 0;
        for (u32 i = 0; i < g_thermal.fan_count; i++) {
            *fan_runtime += g_thermal.fans[i].total_runtime;
        }
    }
    
    return 0;
}

thermal_manager_t *thermal_get_manager(void)
{
    return &g_thermal;
}
