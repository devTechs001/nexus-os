/*
 * NEXUS OS - Mobile Framework
 * mobile/sensors/sensor_hub.c
 *
 * Sensor Hub
 *
 * This module implements the sensor hub which manages all device sensors,
 * including data collection, fusion, batching, and event distribution.
 */

#include "../mobile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================*/
/*                         SENSOR HUB STATE                                  */
/*===========================================================================*/

static struct {
    bool initialized;
    
    /* Sensors */
    sensor_config_t sensors[MAX_SENSORS];
    u32 num_sensors;
    
    /* Sensor events */
    sensor_event_t events[MAX_SENSORS];
    u32 event_count[MAX_SENSORS];
    
    /* FIFO buffers */
    sensor_event_t *fifo_buffers[MAX_SENSORS];
    u32 fifo_head[MAX_SENSORS];
    u32 fifo_tail[MAX_SENSORS];
    u32 fifo_size[MAX_SENSORS];
    
    /* Listeners */
    struct {
        u32 sensor_id;
        void (*callback)(sensor_event_t *);
        bool active;
    } listeners[MAX_SENSORS];
    
    /* Sensor fusion */
    bool fusion_enabled;
    struct {
        float quaternion[4];      /* Orientation quaternion */
        float gravity[3];         /* Gravity vector */
        float linear_accel[3];    /* Linear acceleration */
        u64 last_update;
    } fusion_state;
    
    /* Wake sensors */
    u32 wake_sensor_mask;         /* Bitmask of wake sensors */
    
    /* Statistics */
    u64 total_events;
    u64 fusion_updates;
    
    /* Synchronization */
    spinlock_t lock;
} g_sensor_hub = {
    .initialized = false,
    .fusion_enabled = true,
    .lock = { .lock = 0 }
};

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

static int sensor_hub_register_sensor(sensor_config_t *config);
static void sensor_hub_process_event(u32 sensor_id, sensor_event_t *event);
static void sensor_hub_update_fusion(void);
static float quaternion_to_euler_yaw(float w, float x, float y, float z);

/*===========================================================================*/
/*                         SENSOR HUB INITIALIZATION                         */
/*===========================================================================*/

/**
 * sensor_hub_init - Initialize the sensor hub
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_hub_init(void)
{
    if (g_sensor_hub.initialized) {
        pr_debug("Sensor hub already initialized\n");
        return 0;
    }

    spin_lock(&g_sensor_hub.lock);

    pr_info("Initializing sensor hub...\n");

    memset(&g_sensor_hub, 0, sizeof(g_sensor_hub));
    g_sensor_hub.fusion_enabled = true;
    g_sensor_hub.lock.lock = 0;

    /* Register default sensors (simulated) */
    sensor_hub_register_default_sensors();

    g_sensor_hub.initialized = true;

    pr_info("Sensor hub initialized with %u sensors\n", g_sensor_hub.num_sensors);

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/**
 * sensor_hub_shutdown - Shutdown the sensor hub
 */
void sensor_hub_shutdown(void)
{
    if (!g_sensor_hub.initialized) {
        return;
    }

    spin_lock(&g_sensor_hub.lock);

    pr_info("Shutting down sensor hub...\n");

    /* Disable all sensors */
    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        sensor_disable(g_sensor_hub.sensors[i].sensor_id);
        
        /* Free FIFO buffers */
        if (g_sensor_hub.fifo_buffers[i]) {
            kfree(g_sensor_hub.fifo_buffers[i]);
            g_sensor_hub.fifo_buffers[i] = NULL;
        }
    }

    g_sensor_hub.initialized = false;

    pr_info("Sensor hub shutdown complete\n");

    spin_unlock(&g_sensor_hub.lock);
}

/**
 * sensor_hub_register_default_sensors - Register default sensors
 */
static void sensor_hub_register_default_sensors(void)
{
    sensor_config_t config;

    /* Accelerometer */
    memset(&config, 0, sizeof(config));
    config.sensor_id = 1;
    strncpy(config.name, "BMI260 Accelerometer", MAX_SENSOR_NAME_LEN - 1);
    strncpy(config.vendor, "Bosch", sizeof(config.vendor) - 1);
    config.type = SENSOR_TYPE_ACCELEROMETER;
    config.version = 1;
    config.max_range = 16.0f * 9.81f;  /* 16g */
    config.resolution = 0.001f;
    config.power_ma = 0.15f;
    config.min_delay_us = 2500;  /* 400 Hz */
    config.fifo_max = 1024;
    g_sensor_hub.sensors[g_sensor_hub.num_sensors++] = config;

    /* Gyroscope */
    memset(&config, 0, sizeof(config));
    config.sensor_id = 2;
    strncpy(config.name, "BMI260 Gyroscope", MAX_SENSOR_NAME_LEN - 1);
    strncpy(config.vendor, "Bosch", sizeof(config.vendor) - 1);
    config.type = SENSOR_TYPE_GYROSCOPE;
    config.version = 1;
    config.max_range = 2000.0f * (M_PI / 180.0f);  /* 2000 deg/s */
    config.resolution = 0.001f;
    config.power_ma = 0.20f;
    config.min_delay_us = 2500;
    config.fifo_max = 1024;
    g_sensor_hub.sensors[g_sensor_hub.num_sensors++] = config;

    /* Magnetometer */
    memset(&config, 0, sizeof(config));
    config.sensor_id = 3;
    strncpy(config.name, "BMM150 Magnetometer", MAX_SENSOR_NAME_LEN - 1);
    strncpy(config.vendor, "Bosch", sizeof(config.vendor) - 1);
    config.type = SENSOR_TYPE_MAGNETOMETER;
    config.version = 1;
    config.max_range = 1300.0f;  /* μT */
    config.resolution = 0.1f;
    config.power_ma = 0.10f;
    config.min_delay_us = 10000;  /* 100 Hz */
    config.fifo_max = 512;
    g_sensor_hub.sensors[g_sensor_hub.num_sensors++] = config;

    /* Proximity */
    memset(&config, 0, sizeof(config));
    config.sensor_id = 4;
    strncpy(config.name, "VCNL4040 Proximity", MAX_SENSOR_NAME_LEN - 1);
    strncpy(config.vendor, "Vishay", sizeof(config.vendor) - 1);
    config.type = SENSOR_TYPE_PROXIMITY;
    config.version = 1;
    config.max_range = 5.0f;  /* cm */
    config.resolution = 1.0f;
    config.power_ma = 0.05f;
    config.min_delay_us = 50000;  /* 20 Hz */
    config.fifo_max = 32;
    g_sensor_hub.sensors[g_sensor_hub.num_sensors++] = config;

    /* Light */
    memset(&config, 0, sizeof(config));
    config.sensor_id = 5;
    strncpy(config.name, "VCNL4040 Light", MAX_SENSOR_NAME_LEN - 1);
    strncpy(config.vendor, "Vishay", sizeof(config.vendor) - 1);
    config.type = SENSOR_TYPE_LIGHT;
    config.version = 1;
    config.max_range = 120000.0f;  /* lux */
    config.resolution = 1.0f;
    config.power_ma = 0.05f;
    config.min_delay_us = 50000;
    config.fifo_max = 32;
    g_sensor_hub.sensors[g_sensor_hub.num_sensors++] = config;

    /* Step Counter */
    memset(&config, 0, sizeof(config));
    config.sensor_id = 6;
    strncpy(config.name, "Step Counter", MAX_SENSOR_NAME_LEN - 1);
    strncpy(config.vendor, "NEXUS", sizeof(config.vendor) - 1);
    config.type = SENSOR_TYPE_STEP_COUNTER;
    config.version = 1;
    config.max_range = 1000000.0f;
    config.resolution = 1.0f;
    config.power_ma = 0.01f;
    config.min_delay_us = 1000000;  /* 1 Hz */
    config.fifo_max = 16;
    g_sensor_hub.sensors[g_sensor_hub.num_sensors++] = config;
}

/*===========================================================================*/
/*                         SENSOR MANAGEMENT                                 */
/*===========================================================================*/

/**
 * sensor_get_count - Get number of available sensors
 *
 * Returns: Number of sensors
 */
int sensor_get_count(void)
{
    if (!g_sensor_hub.initialized) {
        return 0;
    }
    return g_sensor_hub.num_sensors;
}

/**
 * sensor_get_config - Get sensor configuration
 * @sensor_id: Sensor ID
 * @config: Output configuration structure
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_get_config(u32 sensor_id, sensor_config_t *config)
{
    if (!config) {
        return -1;
    }

    if (!g_sensor_hub.initialized) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].sensor_id == sensor_id) {
            memcpy(config, &g_sensor_hub.sensors[i], sizeof(sensor_config_t));
            spin_unlock(&g_sensor_hub.lock);
            return 0;
        }
    }

    spin_unlock(&g_sensor_hub.lock);
    return -1;  /* Sensor not found */
}

/**
 * sensor_get_config_by_type - Get sensor configuration by type
 * @type: Sensor type
 * @config: Output configuration structure
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_get_config_by_type(u32 type, sensor_config_t *config)
{
    if (!config) {
        return -1;
    }

    if (!g_sensor_hub.initialized) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].type == type) {
            memcpy(config, &g_sensor_hub.sensors[i], sizeof(sensor_config_t));
            spin_unlock(&g_sensor_hub.lock);
            return 0;
        }
    }

    spin_unlock(&g_sensor_hub.lock);
    return -1;  /* Sensor not found */
}

/**
 * sensor_hub_register_sensor - Register a sensor
 * @config: Sensor configuration
 *
 * Returns: 0 on success, error code on failure
 */
static int sensor_hub_register_sensor(sensor_config_t *config)
{
    if (!config || g_sensor_hub.num_sensors >= MAX_SENSORS) {
        return -1;
    }

    /* Check for duplicate ID */
    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].sensor_id == config->sensor_id) {
            pr_err("Sensor ID %u already registered\n", config->sensor_id);
            return -1;
        }
    }

    g_sensor_hub.sensors[g_sensor_hub.num_sensors] = *config;
    g_sensor_hub.num_sensors++;

    pr_debug("Sensor registered: %s (ID: %u, Type: %u)\n",
             config->name, config->sensor_id, config->type);

    return 0;
}

/*===========================================================================*/
/*                         SENSOR ENABLE/DISABLE                             */
/*===========================================================================*/

/**
 * sensor_enable - Enable a sensor
 * @sensor_id: Sensor ID
 * @sampling_rate_hz: Sampling rate in Hz
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_enable(u32 sensor_id, s32 sampling_rate_hz)
{
    sensor_config_t *sensor = NULL;

    if (!g_sensor_hub.initialized) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    /* Find sensor */
    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].sensor_id == sensor_id) {
            sensor = &g_sensor_hub.sensors[i];
            break;
        }
    }

    if (!sensor) {
        pr_err("Sensor %u not found\n", sensor_id);
        spin_unlock(&g_sensor_hub.lock);
        return -1;
    }

    /* Validate sampling rate */
    if (sampling_rate_hz > 0) {
        s32 min_delay = 1000000 / sampling_rate_hz;  /* Convert to microseconds */
        if (min_delay < sensor->min_delay_us) {
            pr_warn("Sampling rate too high for sensor %u, using minimum delay\n",
                    sensor_id);
            sampling_rate_hz = 1000000 / sensor->min_delay_us;
        }
    }

    sensor->sampling_rate_hz = sampling_rate_hz;
    sensor->enabled = true;

    /* Allocate FIFO buffer if needed */
    if (sensor->fifo_max > 0 && !g_sensor_hub.fifo_buffers[sensor_id - 1]) {
        u32 fifo_size = MIN(sensor->fifo_max, SENSOR_FIFO_SIZE);
        g_sensor_hub.fifo_buffers[sensor_id - 1] = 
            (sensor_event_t *)kzalloc(fifo_size * sizeof(sensor_event_t));
        g_sensor_hub.fifo_size[sensor_id - 1] = fifo_size;
        g_sensor_hub.fifo_head[sensor_id - 1] = 0;
        g_sensor_hub.fifo_tail[sensor_id - 1] = 0;
    }

    pr_info("Sensor %u enabled (%s) at %d Hz\n",
            sensor_id, sensor->name, sampling_rate_hz);

    /* In a real implementation, this would configure the hardware sensor */

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/**
 * sensor_disable - Disable a sensor
 * @sensor_id: Sensor ID
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_disable(u32 sensor_id)
{
    sensor_config_t *sensor = NULL;

    if (!g_sensor_hub.initialized) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    /* Find sensor */
    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].sensor_id == sensor_id) {
            sensor = &g_sensor_hub.sensors[i];
            break;
        }
    }

    if (!sensor) {
        spin_unlock(&g_sensor_hub.lock);
        return -1;
    }

    sensor->enabled = false;
    sensor->sampling_rate_hz = 0;

    pr_debug("Sensor %u disabled\n", sensor_id);

    /* In a real implementation, this would power down the hardware sensor */

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/*===========================================================================*/
/*                         SENSOR DATA READING                               */
/*===========================================================================*/

/**
 * sensor_read - Read sensor data
 * @sensor_id: Sensor ID
 * @event: Output event structure
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_read(u32 sensor_id, sensor_event_t *event)
{
    sensor_config_t *sensor = NULL;

    if (!event) {
        return -1;
    }

    if (!g_sensor_hub.initialized) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    /* Find sensor */
    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].sensor_id == sensor_id) {
            sensor = &g_sensor_hub.sensors[i];
            break;
        }
    }

    if (!sensor) {
        pr_err("Sensor %u not found\n", sensor_id);
        spin_unlock(&g_sensor_hub.lock);
        return -1;
    }

    if (!sensor->enabled) {
        pr_err("Sensor %u is not enabled\n", sensor_id);
        spin_unlock(&g_sensor_hub.lock);
        return -1;
    }

    /* Read from FIFO if available */
    if (g_sensor_hub.fifo_buffers[sensor_id - 1] &&
        g_sensor_hub.fifo_head[sensor_id - 1] != g_sensor_hub.fifo_tail[sensor_id - 1]) {
        
        u32 tail = g_sensor_hub.fifo_tail[sensor_id - 1];
        *event = g_sensor_hub.fifo_buffers[sensor_id - 1][tail];
        g_sensor_hub.fifo_tail[sensor_id - 1] = (tail + 1) % g_sensor_hub.fifo_size[sensor_id - 1];
    } else {
        /* Generate simulated data */
        generate_simulated_sensor_data(sensor, event);
    }

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/**
 * generate_simulated_sensor_data - Generate simulated sensor data
 * @sensor: Sensor configuration
 * @event: Output event
 */
static void generate_simulated_sensor_data(sensor_config_t *sensor, sensor_event_t *event)
{
    static float sim_accel[3] = {0.0f, 0.0f, 9.81f};
    static float sim_gyro[3] = {0.0f, 0.0f, 0.0f};
    static float sim_mag[3] = {30.0f, 10.0f, 50.0f};
    static u64 sim_steps = 0;

    memset(event, 0, sizeof(sensor_event_t));
    event->sensor_id = sensor->sensor_id;
    event->sensor_type = sensor->type;
    event->timestamp = get_time_ms() * 1000000ULL;  /* Nanoseconds */
    event->accuracy = SENSOR_ACCURACY_HIGH;

    switch (sensor->type) {
        case SENSOR_TYPE_ACCELEROMETER:
            /* Simulate gravity + small noise */
            event->data.accelerometer.x = sim_accel[0] + ((rand() % 100) - 50) * 0.01f;
            event->data.accelerometer.y = sim_accel[1] + ((rand() % 100) - 50) * 0.01f;
            event->data.accelerometer.z = sim_accel[2] + ((rand() % 100) - 50) * 0.01f;
            break;

        case SENSOR_TYPE_GYROSCOPE:
            event->data.gyroscope.x = sim_gyro[0];
            event->data.gyroscope.y = sim_gyro[1];
            event->data.gyroscope.z = sim_gyro[2];
            break;

        case SENSOR_TYPE_MAGNETOMETER:
            event->data.magnetometer.x = sim_mag[0];
            event->data.magnetometer.y = sim_mag[1];
            event->data.magnetometer.z = sim_mag[2];
            break;

        case SENSOR_TYPE_PROXIMITY:
            event->data.proximity.distance = 0.0f;  /* Not near */
            break;

        case SENSOR_TYPE_LIGHT:
            event->data.light.lux = 500.0f;  /* Indoor lighting */
            break;

        case SENSOR_TYPE_STEP_COUNTER:
            event->data.step_counter.steps = sim_steps;
            break;

        default:
            break;
    }
}

/**
 * sensor_hub_process_event - Process a sensor event
 * @sensor_id: Sensor ID
 * @event: Event data
 */
static void sensor_hub_process_event(u32 sensor_id, sensor_event_t *event)
{
    sensor_config_t *sensor = NULL;

    /* Find sensor */
    for (u32 i = 0; i < g_sensor_hub.num_sensors; i++) {
        if (g_sensor_hub.sensors[i].sensor_id == sensor_id) {
            sensor = &g_sensor_hub.sensors[i];
            break;
        }
    }

    if (!sensor) {
        return;
    }

    /* Store event */
    g_sensor_hub.events[sensor_id - 1] = *event;
    g_sensor_hub.event_count[sensor_id - 1]++;
    g_sensor_hub.total_events++;

    /* Add to FIFO if enabled */
    if (g_sensor_hub.fifo_buffers[sensor_id - 1]) {
        u32 head = g_sensor_hub.fifo_head[sensor_id - 1];
        u32 next_head = (head + 1) % g_sensor_hub.fifo_size[sensor_id - 1];
        
        if (next_head != g_sensor_hub.fifo_tail[sensor_id - 1]) {
            g_sensor_hub.fifo_buffers[sensor_id - 1][head] = *event;
            g_sensor_hub.fifo_head[sensor_id - 1] = next_head;
        }
    }

    /* Notify listener */
    if (g_sensor_hub.listeners[sensor_id - 1].active &&
        g_sensor_hub.listeners[sensor_id - 1].callback) {
        g_sensor_hub.listeners[sensor_id - 1].callback(event);
    }

    /* Update sensor fusion */
    if (g_sensor_hub.fusion_enabled &&
        (sensor->type == SENSOR_TYPE_ACCELEROMETER ||
         sensor->type == SENSOR_TYPE_GYROSCOPE ||
         sensor->type == SENSOR_TYPE_MAGNETOMETER)) {
        sensor_hub_update_fusion();
    }
}

/*===========================================================================*/
/*                         SENSOR FUSION                                     */
/*===========================================================================*/

/**
 * sensor_hub_update_fusion - Update sensor fusion state
 *
 * Combines accelerometer, gyroscope, and magnetometer data to
 * compute device orientation using a complementary filter or
 * Kalman filter.
 */
static void sensor_hub_update_fusion(void)
{
    sensor_event_t accel_event, gyro_event, mag_event;
    float ax, ay, az, gx, gy, gz, mx, my, mz;
    float dt;
    u64 now;

    /* Get latest sensor data */
    if (sensor_read(1, &accel_event) != 0) return;  /* Accelerometer */
    if (sensor_read(2, &gyro_event) != 0) return;   /* Gyroscope */
    if (sensor_read(3, &mag_event) != 0) return;    /* Magnetometer */

    ax = accel_event.data.accelerometer.x;
    ay = accel_event.data.accelerometer.y;
    az = accel_event.data.accelerometer.z;

    gx = gyro_event.data.gyroscope.x;
    gy = gyro_event.data.gyroscope.y;
    gz = gyro_event.data.gyroscope.z;

    mx = mag_event.data.magnetometer.x;
    my = mag_event.data.magnetometer.y;
    mz = mag_event.data.magnetometer.z;

    /* Calculate time delta */
    now = get_time_ms() * 1000000ULL;
    dt = (now - g_sensor_hub.fusion_state.last_update) / 1000000000.0f;
    g_sensor_hub.fusion_state.last_update = now;

    if (dt <= 0 || dt > 1.0f) {
        dt = 0.01f;  /* Default 10ms */
    }

    /* Simple complementary filter for orientation */
    /* In a real implementation, this would use a proper IMU fusion algorithm */
    
    /* Calculate pitch and roll from accelerometer */
    float pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
    float roll = atan2f(ay, az);

    /* Integrate gyroscope data */
    float *q = g_sensor_hub.fusion_state.quaternion;
    q[0] += 0.5f * (-q[1] * gx - q[2] * gy - q[3] * gz) * dt;
    q[1] += 0.5f * (q[0] * gx + q[2] * gz - q[3] * gy) * dt;
    q[2] += 0.5f * (q[0] * gy - q[1] * gz + q[3] * gx) * dt;
    q[3] += 0.5f * (q[0] * gz + q[1] * gy - q[2] * gx) * dt;

    /* Normalize quaternion */
    float norm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (norm > 0) {
        q[0] /= norm;
        q[1] /= norm;
        q[2] /= norm;
        q[3] /= norm;
    }

    /* Calculate gravity vector */
    g_sensor_hub.fusion_state.gravity[0] = 2.0f * (q[1]*q[3] - q[0]*q[2]);
    g_sensor_hub.fusion_state.gravity[1] = 2.0f * (q[0]*q[1] + q[2]*q[3]);
    g_sensor_hub.fusion_state.gravity[2] = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];

    /* Calculate linear acceleration (remove gravity) */
    g_sensor_hub.fusion_state.linear_accel[0] = ax - g_sensor_hub.fusion_state.gravity[0] * 9.81f;
    g_sensor_hub.fusion_state.linear_accel[1] = ay - g_sensor_hub.fusion_state.gravity[1] * 9.81f;
    g_sensor_hub.fusion_state.linear_accel[2] = az - g_sensor_hub.fusion_state.gravity[2] * 9.81f;

    g_sensor_hub.fusion_updates++;
}

/**
 * sensor_get_orientation - Get fused orientation
 * @azimuth: Output azimuth (degrees)
 * @pitch: Output pitch (degrees)
 * @roll: Output roll (degrees)
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_get_orientation(float *azimuth, float *pitch, float *roll)
{
    if (!azimuth || !pitch || !roll) {
        return -1;
    }

    if (!g_sensor_hub.initialized || !g_sensor_hub.fusion_enabled) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    float *q = g_sensor_hub.fusion_state.quaternion;

    /* Calculate yaw (azimuth) from quaternion */
    *azimuth = quaternion_to_euler_yaw(q[0], q[1], q[2], q[3]) * (180.0f / M_PI);
    if (*azimuth < 0) *azimuth += 360.0f;

    /* Calculate pitch */
    float sinp = 2.0f * (q[0]*q[2] - q[3]*q[1]);
    if (fabsf(sinp) >= 1) {
        *pitch = copysignf(M_PI / 2, sinp);
    } else {
        *pitch = asinf(sinp) * (180.0f / M_PI);
    }

    /* Calculate roll */
    float siny_cosp = 2.0f * (q[0]*q[1] + q[2]*q[3]);
    float cosy_cosp = 1.0f - 2.0f * (q[1]*q[1] + q[2]*q[2]);
    *roll = atan2f(siny_cosp, cosy_cosp) * (180.0f / M_PI);

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/**
 * quaternion_to_euler_yaw - Convert quaternion to yaw angle
 */
static float quaternion_to_euler_yaw(float w, float x, float y, float z)
{
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    return atan2f(siny_cosp, cosy_cosp);
}

/*===========================================================================*/
/*                         SENSOR LISTENERS                                  */
/*===========================================================================*/

/**
 * sensor_set_listener - Set event listener for a sensor
 * @sensor_id: Sensor ID
 * @callback: Callback function
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_set_listener(u32 sensor_id, void (*callback)(sensor_event_t *))
{
    if (sensor_id == 0 || sensor_id > MAX_SENSORS) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    g_sensor_hub.listeners[sensor_id - 1].sensor_id = sensor_id;
    g_sensor_hub.listeners[sensor_id - 1].callback = callback;
    g_sensor_hub.listeners[sensor_id - 1].active = (callback != NULL);

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/*===========================================================================*/
/*                         WAKE SENSORS                                      */
/*===========================================================================*/

/**
 * sensor_set_wake_enabled - Enable/disable sensor as wake source
 * @sensor_id: Sensor ID
 * @enabled: Enable as wake source
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_set_wake_enabled(u32 sensor_id, bool enabled)
{
    if (sensor_id == 0 || sensor_id > 32) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    if (enabled) {
        g_sensor_hub.wake_sensor_mask |= (1 << (sensor_id - 1));
    } else {
        g_sensor_hub.wake_sensor_mask &= ~(1 << (sensor_id - 1));
    }

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/**
 * sensor_is_wake_enabled - Check if sensor is wake source
 * @sensor_id: Sensor ID
 *
 * Returns: true if wake source, false otherwise
 */
bool sensor_is_wake_enabled(u32 sensor_id)
{
    if (sensor_id == 0 || sensor_id > 32) {
        return false;
    }

    return (g_sensor_hub.wake_sensor_mask & (1 << (sensor_id - 1))) != 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * sensor_get_statistics - Get sensor statistics
 * @sensor_id: Sensor ID
 * @event_count: Output event count
 * @enabled: Output enabled status
 *
 * Returns: 0 on success, error code on failure
 */
int sensor_get_statistics(u32 sensor_id, u64 *event_count, bool *enabled)
{
    if (sensor_id == 0 || sensor_id > g_sensor_hub.num_sensors) {
        return -1;
    }

    spin_lock(&g_sensor_hub.lock);

    if (event_count) {
        *event_count = g_sensor_hub.event_count[sensor_id - 1];
    }

    if (enabled) {
        *enabled = g_sensor_hub.sensors[sensor_id - 1].enabled;
    }

    spin_unlock(&g_sensor_hub.lock);

    return 0;
}

/**
 * sensor_hub_get_total_events - Get total event count
 *
 * Returns: Total number of sensor events processed
 */
u64 sensor_hub_get_total_events(void)
{
    return g_sensor_hub.total_events;
}

/**
 * sensor_hub_get_fusion_updates - Get fusion update count
 *
 * Returns: Number of sensor fusion updates
 */
u64 sensor_hub_get_fusion_updates(void)
{
    return g_sensor_hub.fusion_updates;
}
