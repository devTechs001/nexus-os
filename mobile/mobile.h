/*
 * NEXUS OS - Mobile Framework
 * Copyright (c) 2026 NEXUS Development Team
 *
 * mobile.h - Mobile Framework Header
 *
 * This header defines the core mobile framework interfaces for NEXUS OS.
 * It provides support for battery management, telephony, sensors, and
 * other mobile-specific functionality.
 */

#ifndef _NEXUS_MOBILE_H
#define _NEXUS_MOBILE_H

#include "../kernel/include/types.h"
#include "../kernel/include/kernel.h"

/*===========================================================================*/
/*                         MOBILE FRAMEWORK VERSION                          */
/*===========================================================================*/

#define MOBILE_VERSION_MAJOR    1
#define MOBILE_VERSION_MINOR    0
#define MOBILE_VERSION_PATCH    0
#define MOBILE_VERSION_STRING   "1.0.0"
#define MOBILE_CODENAME         "MobileCore"

/*===========================================================================*/
/*                         FRAMEWORK CONFIGURATION                           */
/*===========================================================================*/

/* Battery configuration */
#define BATTERY_LEVEL_UNKNOWN   -1
#define BATTERY_LEVEL_CRITICAL  5
#define BATTERY_LEVEL_LOW       15
#define BATTERY_LEVEL_MEDIUM    50
#define BATTERY_LEVEL_HIGH      80
#define BATTERY_LEVEL_FULL      100

#define BATTERY_TEMP_CRITICAL   600  /* 60.0°C in tenths */
#define BATTERY_TEMP_HIGH       450  /* 45.0°C in tenths */
#define BATTERY_TEMP_NORMAL     350  /* 35.0°C in tenths */

/* Telephony configuration */
#define MAX_PHONE_NUMBER_LEN    32
#define MAX_IMSI_LEN          16
#define MAX_IMEI_LEN          16
#define MAX_ICCID_LEN         20
#define MAX_OPERATOR_NAME_LEN 64

#define CALL_STATE_IDLE         0
#define CALL_STATE_RINGING      1
#define CALL_STATE_OFFHOOK      2
#define CALL_STATE_DIALING      3
#define CALL_STATE_CONNECTED    4
#define CALL_STATE_DISCONNECTED 5

#define DATA_STATE_DISCONNECTED 0
#define DATA_STATE_CONNECTING   1
#define DATA_STATE_CONNECTED    2
#define DATA_STATE_SUSPENDED    3

#define NETWORK_TYPE_UNKNOWN    0
#define NETWORK_TYPE_GPRS       1
#define NETWORK_TYPE_EDGE       2
#define NETWORK_TYPE_UMTS       3
#define NETWORK_TYPE_CDMA       4
#define NETWORK_TYPE_EVDO       5
#define NETWORK_TYPE_HSDPA      6
#define NETWORK_TYPE_HSUPA      7
#define NETWORK_TYPE_HSPA       8
#define NETWORK_TYPE_LTE        9
#define NETWORK_TYPE_5G         10

/* Sensor configuration */
#define MAX_SENSORS             32
#define MAX_SENSOR_NAME_LEN     64
#define SENSOR_FIFO_SIZE        128

/* Sensor types */
#define SENSOR_TYPE_ACCELEROMETER       1
#define SENSOR_TYPE_GYROSCOPE           2
#define SENSOR_TYPE_MAGNETOMETER        3
#define SENSOR_TYPE_ORIENTATION         4
#define SENSOR_TYPE_PROXIMITY           5
#define SENSOR_TYPE_LIGHT               6
#define SENSOR_TYPE_PRESSURE            7
#define SENSOR_TYPE_TEMPERATURE         8
#define SENSOR_TYPE_HUMIDITY            9
#define SENSOR_TYPE_HEART_RATE          10
#define SENSOR_TYPE_STEP_COUNTER        11
#define SENSOR_TYPE_SIGNIFICANT_MOTION  12
#define SENSOR_TYPE_GRAVITY             13
#define SENSOR_TYPE_LINEAR_ACCEL        14
#define SENSOR_TYPE_ROTATION_VECTOR     15
#define SENSOR_TYPE_AMBIENT_TEMPERATURE 16
#define SENSOR_TYPE_HINGE_ANGLE         17

/* Sensor accuracy */
#define SENSOR_ACCURACY_UNRELIABLE  0
#define SENSOR_ACCURACY_LOW         1
#define SENSOR_ACCURACY_MEDIUM      2
#define SENSOR_ACCURACY_HIGH        3

/* Power modes */
#define POWER_MODE_FULL           0
#define POWER_MODE_NORMAL         1
#define POWER_MODE_POWER_SAVE     2
#define POWER_MODE_ULTRA_SAVE     3
#define POWER_MODE_FLIGHT         4

/* Screen states */
#define SCREEN_STATE_OFF          0
#define SCREEN_STATE_ON           1
#define SCREEN_STATE_DOZE         2
#define SCREEN_STATE_AOD          3  /* Always On Display */

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * Battery Information
 */
typedef struct battery_info {
    s32 level;                    /* Battery level percentage (0-100) */
    s32 voltage;                  /* Battery voltage in millivolts */
    s32 current;                  /* Battery current in milliamps */
    s32 temperature;              /* Battery temperature in tenths of °C */
    s32 health;                   /* Battery health status */
    s32 status;                   /* Battery charging status */
    s32 technology[8];            /* Battery technology */
    u64 charge_counter;           /* Charge counter in microamp-hours */
    u64 charge_full;              /* Full charge capacity */
    u64 charge_design;            /* Design capacity */
    u64 time_to_full;             /* Estimated time to full (seconds) */
    u64 time_to_empty;            /* Estimated time to empty (seconds) */
    u64 last_updated;             /* Last update timestamp */
} battery_info_t;

/**
 * Battery Health Status
 */
#define BATTERY_HEALTH_UNKNOWN    0
#define BATTERY_HEALTH_GOOD       1
#define BATTERY_HEALTH_OVERHEAT   2
#define BATTERY_HEALTH_DEAD       3
#define BATTERY_HEALTH_OVER_VOLTAGE 4
#define BATTERY_HEALTH_UNSPEC_FAILURE 5
#define BATTERY_HEALTH_COLD       6

/**
 * Battery Status
 */
#define BATTERY_STATUS_UNKNOWN    0
#define BATTERY_STATUS_CHARGING   1
#define BATTERY_STATUS_DISCHARGING 2
#define BATTERY_STATUS_NOT_CHARGING 3
#define BATTERY_STATUS_FULL       4

/**
 * Battery Charging Type
 */
#define BATTERY_CHARGE_TYPE_NONE  0
#define BATTERY_CHARGE_TYPE_AC    1
#define BATTERY_CHARGE_TYPE_USB   2
#define BATTERY_CHARGE_TYPE_WIRELESS 3
#define BATTERY_CHARGE_TYPE_DOCK  4

/**
 * SIM Card Information
 */
typedef struct sim_info {
    u32 slot_id;                  /* SIM slot identifier */
    char iccid[MAX_ICCID_LEN];    /* ICCID (SIM serial number) */
    char imsi[MAX_IMSI_LEN];      /* IMSI (Subscriber identity) */
    char operator_name[MAX_OPERATOR_NAME_LEN];
    char operator_numeric[8];     /* MCC+MNC */
    bool is_ready;                /* SIM is ready */
    bool has_pin;                 /* SIM has PIN lock */
    bool is_pin_unlocked;         /* SIM PIN is unlocked */
    bool is_network_locked;       /* Network locked */
    char carrier_config[64];      /* Carrier configuration */
} sim_info_t;

/**
 * Call Information
 */
typedef struct call_info {
    u32 call_id;                  /* Call identifier */
    char number[MAX_PHONE_NUMBER_LEN];
    u32 state;                    /* Call state */
    u32 type;                     /* Call type (voice, video, etc.) */
    u64 start_time;               /* Call start time */
    u64 duration;                 /* Call duration in seconds */
    u32 direction;                /* Call direction (in/out) */
    char contact_name[64];        /* Contact name if available */
} call_info_t;

/**
 * Network Information
 */
typedef struct network_info {
    u32 network_type;             /* Network type */
    s32 signal_strength;          /* Signal strength (dBm) */
    s32 signal_level;             /* Signal level (0-4) */
    char operator_name[MAX_OPERATOR_NAME_LEN];
    char operator_numeric[8];
    char network_name[64];        /* Network name */
    u32 lac;                      /* Location Area Code */
    u32 cid;                      /* Cell ID */
    u32 psc;                      /* Primary Scrambling Code */
    s32 timing_advance;           /* Timing advance */
    bool is_roaming;              /* Roaming status */
    bool is_data_connected;       /* Data connection status */
    u32 data_network_type;        /* Data network type */
} network_info_t;

/**
 * Sensor Event
 */
typedef struct sensor_event {
    u32 sensor_id;                /* Sensor identifier */
    u32 sensor_type;              /* Sensor type */
    u64 timestamp;                /* Event timestamp (nanoseconds) */
    u32 accuracy;                 /* Accuracy status */
    
    /* Sensor data (union based on type) */
    union {
        struct {
            float x, y, z;        /* Acceleration (m/s²) */
        } accelerometer;
        struct {
            float x, y, z;        /* Angular velocity (rad/s) */
        } gyroscope;
        struct {
            float x, y, z;        /* Magnetic field (μT) */
        } magnetometer;
        struct {
            float azimuth;        /* Azimuth (degrees) */
            float pitch;          /* Pitch (degrees) */
            float roll;           /* Roll (degrees) */
        } orientation;
        struct {
            float distance;       /* Distance (cm) */
        } proximity;
        struct {
            float lux;            /* Illuminance (lux) */
        } light;
        struct {
            float pressure;       /* Pressure (hPa) */
        } pressure;
        struct {
            float temperature;    /* Temperature (°C) */
        } temperature;
        struct {
            float humidity;       /* Humidity (%) */
        } humidity;
        struct {
            s32 heart_rate;       /* Heart rate (bpm) */
            s32 status;           /* Status */
        } heart_rate;
        struct {
            u64 steps;            /* Step count */
        } step_counter;
        struct {
            float x, y, z, w;     /* Quaternion */
        } rotation_vector;
        float values[4];          /* Generic values */
    } data;
} sensor_event_t;

/**
 * Sensor Configuration
 */
typedef struct sensor_config {
    u32 sensor_id;                /* Sensor identifier */
    char name[MAX_SENSOR_NAME_LEN];
    char vendor[64];
    u32 type;                     /* Sensor type */
    u32 version;                  /* Sensor version */
    
    /* Capabilities */
    float max_range;              /* Maximum range */
    float resolution;             /* Resolution */
    float power_ma;               /* Power consumption (mA) */
    s32 min_delay_us;             /* Minimum delay (microseconds) */
    u32 fifo_reserved;            /* FIFO reserved size */
    u32 fifo_max;                 /* FIFO maximum size */
    
    /* Current settings */
    s32 sampling_rate_hz;         /* Sampling rate (Hz) */
    u32 flags;                    /* Sensor flags */
    bool enabled;                 /* Sensor enabled */
} sensor_config_t;

/**
 * Power State
 */
typedef struct power_state {
    u32 power_mode;               /* Current power mode */
    u32 screen_state;             /* Screen state */
    bool screen_on;               /* Screen is on */
    bool interactive;             /* Device is interactive */
    bool charging;                /* Device is charging */
    u32 wake_locks;               /* Active wake locks */
    u64 last_sleep_time;          /* Last sleep timestamp */
    u64 last_wake_time;           /* Last wake timestamp */
    u64 total_sleep_time;         /* Total sleep time */
    u64 total_wake_time;          /* Total wake time */
} power_state_t;

/**
 * Radio Interface Layer State
 */
typedef struct ril_state {
    bool initialized;             /* RIL initialized */
    bool radio_on;                /* Radio is on */
    u32 phone_type;               /* Phone type (GSM, CDMA, etc.) */
    
    /* SIM state */
    sim_info_t sim_info[2];       /* SIM info for dual SIM */
    u32 num_sims;                 /* Number of SIMs */
    u32 active_sim;               /* Active SIM slot */
    
    /* Call state */
    call_info_t active_call;
    call_info_t background_call;
    u32 call_state;
    
    /* Network state */
    network_info_t network_info;
    bool registered;
    u32 registration_state;
    
    /* Data state */
    u32 data_state;
    char data_apn[64];            /* Current APN */
    
    /* Callbacks */
    void (*on_signal_strength_changed)(s32, s32);
    void (*on_network_changed)(network_info_t *);
    void (*on_call_state_changed)(call_info_t *, u32);
    void (*on_sim_state_changed)(u32, u32);
    void (*on_data_connection_changed)(u32);
    
    /* Synchronization */
    spinlock_t lock;
} ril_state_t;

/*===========================================================================*/
/*                         MOBILE FRAMEWORK STATE                            */
/*===========================================================================*/

typedef struct mobile_framework {
    bool initialized;             /* Framework initialized */
    u32 version;                  /* Framework version */
    
    /* Power management */
    power_state_t power_state;
    battery_info_t battery_info;
    u32 current_power_mode;
    
    /* Telephony */
    ril_state_t ril;
    
    /* Sensors */
    sensor_config_t sensors[MAX_SENSORS];
    u32 num_sensors;
    sensor_event_t sensor_events[MAX_SENSORS];
    
    /* Display */
    u32 screen_brightness;        /* Brightness level (0-255) */
    u32 screen_timeout;           /* Screen timeout (ms) */
    bool auto_brightness;         /* Auto brightness enabled */
    
    /* Vibration */
    bool vibration_enabled;
    u32 vibration_strength;
    
    /* Synchronization */
    spinlock_t lock;
} mobile_framework_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Framework initialization */
int mobile_framework_init(void);
void mobile_framework_shutdown(void);
bool mobile_framework_is_initialized(void);

/* Power management */
int power_get_state(power_state_t *state);
int power_set_mode(u32 mode);
u32 power_get_mode(void);
int power_wake_acquire(const char *name);
int power_wake_release(const char *name);
int power_reboot(void);
int power_shutdown(void);

/* Battery management */
int battery_get_info(battery_info_t *info);
s32 battery_get_level(void);
s32 battery_get_voltage(void);
s32 battery_get_temperature(void);
bool battery_is_charging(void);
bool battery_is_full(void);
int battery_set_charging_enabled(bool enabled);

/* Telephony - SIM */
int ril_get_sim_count(void);
int ril_get_sim_info(u32 slot, sim_info_t *info);
int ril_is_sim_ready(u32 slot);
int ril_enter_sim_pin(u32 slot, const char *pin);
int ril_get_operator_name(u32 slot, char *name, size_t len);

/* Telephony - Calls */
int ril_get_call_state(void);
int ril_get_active_call(call_info_t *call);
int ril_make_call(const char *number);
int ril_answer_call(void);
int ril_end_call(void);
int ril_end_call_by_id(u32 call_id);
int ril_reject_call(void);

/* Telephony - Network */
int ril_get_network_info(network_info_t *info);
s32 ril_get_signal_strength(void);
s32 ril_get_signal_level(void);
bool ril_is_roaming(void);
bool ril_is_data_connected(void);
int ril_set_data_enabled(bool enabled);
bool ril_is_data_enabled(void);

/* Sensors */
int sensor_get_count(void);
int sensor_get_config(u32 sensor_id, sensor_config_t *config);
int sensor_get_config_by_type(u32 type, sensor_config_t *config);
int sensor_enable(u32 sensor_id, s32 sampling_rate_hz);
int sensor_disable(u32 sensor_id);
int sensor_read(u32 sensor_id, sensor_event_t *event);
int sensor_set_listener(u32 sensor_id, void (*callback)(sensor_event_t *));

/* Display */
int display_set_brightness(u32 level);
u32 display_get_brightness(void);
int display_set_auto_brightness(bool enabled);
bool display_get_auto_brightness(void);
int display_set_timeout(u32 ms);
u32 display_get_timeout(void);
int display_wake(void);
int display_sleep(void);
bool display_is_on(void);

/* Vibration */
int vibrate(u32 duration_ms);
int vibrate_pattern(const u32 *pattern, u32 pattern_len);
int vibration_cancel(void);
int vibration_set_strength(u32 strength);

/* Utility functions */
const char *mobile_get_version(void);
const char *mobile_get_codename(void);
const char *network_type_to_string(u32 type);
const char *call_state_to_string(u32 state);

#endif /* _NEXUS_MOBILE_H */
