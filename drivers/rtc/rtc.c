/*
 * NEXUS OS - RTC Driver
 * drivers/rtc/rtc.c
 *
 * Real-Time Clock driver with alarm support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         RTC CONFIGURATION                                 */
/*===========================================================================*/

#define RTC_MAX_DEVICES           4
#define RTC_PORT_CMOS             0x70
#define RTC_PORT_DATA             0x71

/* RTC Registers */
#define RTC_SECOND                0x00
#define RTC_MINUTE                0x02
#define RTC_HOUR                  0x04
#define RTC_DAY_OF_WEEK           0x06
#define RTC_DAY_OF_MONTH          0x07
#define RTC_MONTH                 0x08
#define RTC_YEAR                  0x09
#define RTC_CENTURY               0x32
#define RTC_STATUS_A              0x0A
#define RTC_STATUS_B              0x0B
#define RTC_STATUS_C              0x0C
#define RTC_STATUS_D              0x0D
#define RTC_ALARM_SECOND          0x01
#define RTC_ALARM_MINUTE          0x03
#define RTC_ALARM_HOUR            0x05

/* Status Register B Bits */
#define RTC_STATUS_B_UIP          0x80
#define RTC_STATUS_B_DM           0x04
#define RTC_STATUS_B_24H          0x02
#define RTC_STATUS_B_DSE          0x01

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 device_id;
    char name[32];
    bool is_enabled;
    bool is_24hour;
    bool is_binary;
    u8 century;
    u32 alarm_enabled;
    void (*on_alarm)(void *);
    void *alarm_user_data;
} rtc_device_t;

typedef struct {
    bool initialized;
    rtc_device_t devices[RTC_MAX_DEVICES];
    u32 device_count;
    rtc_device_t *current_device;
    spinlock_t lock;
} rtc_driver_t;

static rtc_driver_t g_rtc;

/*===========================================================================*/
/*                         RTC LOW-LEVEL OPERATIONS                          */
/*===========================================================================*/

static inline u8 rtc_read_reg(u8 reg)
{
    vga_outb(RTC_PORT_CMOS, reg);
    return vga_inb(RTC_PORT_DATA);
}

static inline void rtc_write_reg(u8 reg, u8 value)
{
    vga_outb(RTC_PORT_CMOS, reg);
    vga_outb(RTC_PORT_DATA, value);
}

static u8 rtc_read_bcd(u8 reg)
{
    u8 value = rtc_read_reg(reg);
    rtc_device_t *dev = g_rtc.current_device;
    
    if (dev && !dev->is_binary) {
        return ((value >> 4) * 10) + (value & 0x0F);
    }
    return value;
}

static void rtc_write_bcd(u8 reg, u8 value)
{
    rtc_device_t *dev = g_rtc.current_device;
    
    if (dev && !dev->is_binary) {
        value = ((value / 10) << 4) | (value % 10);
    }
    rtc_write_reg(reg, value);
}

/*===========================================================================*/
/*                         RTC TIME FUNCTIONS                                */
/*===========================================================================*/

int rtc_get_time(u32 *hour, u32 *minute, u32 *second)
{
    rtc_device_t *dev = g_rtc.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    spinlock_lock(&g_rtc.lock);
    
    /* Wait for update in progress to finish */
    while (rtc_read_reg(RTC_STATUS_A) & RTC_STATUS_B_UIP);
    
    u32 h = rtc_read_bcd(RTC_HOUR);
    u32 m = rtc_read_bcd(RTC_MINUTE);
    u32 s = rtc_read_bcd(RTC_SECOND);
    
    /* Convert to 24-hour if needed */
    if (!dev->is_24hour && (h & 0x80)) {
        /* PM */
        h = (h & 0x7F) + 12;
    }
    
    spinlock_unlock(&g_rtc.lock);
    
    if (hour) *hour = h;
    if (minute) *minute = m;
    if (second) *second = s;
    
    return 0;
}

int rtc_set_time(u32 hour, u32 minute, u32 second)
{
    rtc_device_t *dev = g_rtc.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    spinlock_lock(&g_rtc.lock);
    
    /* Disable updates */
    u8 status_b = rtc_read_reg(RTC_STATUS_B);
    rtc_write_reg(RTC_STATUS_B, status_b | RTC_STATUS_B_UIP);
    
    /* Set time */
    rtc_write_bcd(RTC_HOUR, hour);
    rtc_write_bcd(RTC_MINUTE, minute);
    rtc_write_bcd(RTC_SECOND, second);
    
    /* Re-enable updates */
    rtc_write_reg(RTC_STATUS_B, status_b);
    
    spinlock_unlock(&g_rtc.lock);
    
    printk("[RTC] Time set: %02d:%02d:%02d\n", hour, minute, second);
    return 0;
}

int rtc_get_date(u32 *year, u32 *month, u32 *day, u32 *weekday)
{
    rtc_device_t *dev = g_rtc.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    spinlock_lock(&g_rtc.lock);
    
    /* Wait for update in progress to finish */
    while (rtc_read_reg(RTC_STATUS_A) & RTC_STATUS_B_UIP);
    
    u32 y = rtc_read_bcd(RTC_YEAR);
    u32 m = rtc_read_bcd(RTC_MONTH);
    u32 d = rtc_read_bcd(RTC_DAY_OF_MONTH);
    u32 w = rtc_read_bcd(RTC_DAY_OF_WEEK);
    
    /* Add century */
    if (dev->century > 0) {
        y += dev->century * 100;
    } else {
        /* Assume 2000s for years < 80 */
        y += (y < 80) ? 2000 : 1900;
    }
    
    spinlock_unlock(&g_rtc.lock);
    
    if (year) *year = y;
    if (month) *month = m;
    if (day) *day = d;
    if (weekday) *weekday = w;
    
    return 0;
}

int rtc_set_date(u32 year, u32 month, u32 day)
{
    rtc_device_t *dev = g_rtc.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    spinlock_lock(&g_rtc.lock);
    
    /* Disable updates */
    u8 status_b = rtc_read_reg(RTC_STATUS_B);
    rtc_write_reg(RTC_STATUS_B, status_b | RTC_STATUS_B_UIP);
    
    /* Set date */
    rtc_write_bcd(RTC_YEAR, year % 100);
    rtc_write_bcd(RTC_MONTH, month);
    rtc_write_bcd(RTC_DAY_OF_MONTH, day);
    
    /* Re-enable updates */
    rtc_write_reg(RTC_STATUS_B, status_b);
    
    spinlock_unlock(&g_rtc.lock);
    
    printk("[RTC] Date set: %04d-%02d-%02d\n", year, month, day);
    return 0;
}

/*===========================================================================*/
/*                         ALARM FUNCTIONS                                   */
/*===========================================================================*/

int rtc_set_alarm(u32 hour, u32 minute, u32 second)
{
    rtc_device_t *dev = g_rtc.current_device;
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    spinlock_lock(&g_rtc.lock);
    
    /* Set alarm registers (0xFF = disabled) */
    rtc_write_bcd(RTC_ALARM_HOUR, hour);
    rtc_write_bcd(RTC_ALARM_MINUTE, minute);
    rtc_write_bcd(RTC_ALARM_SECOND, second);
    
    /* Enable alarm interrupt */
    u8 status_b = rtc_read_reg(RTC_STATUS_B);
    rtc_write_reg(RTC_STATUS_B, status_b | 0x20);  /* Alarm interrupt enable */
    
    dev->alarm_enabled = 1;
    
    spinlock_unlock(&g_rtc.lock);
    
    printk("[RTC] Alarm set: %02d:%02d:%02d\n", hour, minute, second);
    return 0;
}

int rtc_clear_alarm(void)
{
    rtc_device_t *dev = g_rtc.current_device;
    if (!dev) return -ENODEV;
    
    spinlock_lock(&g_rtc.lock);
    
    /* Disable alarm interrupt */
    u8 status_b = rtc_read_reg(RTC_STATUS_B);
    rtc_write_reg(RTC_STATUS_B, status_b & ~0x20);
    
    /* Clear alarm registers */
    rtc_write_reg(RTC_ALARM_HOUR, 0xFF);
    rtc_write_reg(RTC_ALARM_MINUTE, 0xFF);
    rtc_write_reg(RTC_ALARM_SECOND, 0xFF);
    
    dev->alarm_enabled = 0;
    
    spinlock_unlock(&g_rtc.lock);
    
    printk("[RTC] Alarm cleared\n");
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int rtc_init(void)
{
    printk("[RTC] ========================================\n");
    printk("[RTC] NEXUS OS RTC Driver\n");
    printk("[RTC] ========================================\n");
    
    memset(&g_rtc, 0, sizeof(rtc_driver_t));
    spinlock_init(&g_rtc.lock);
    
    /* Register RTC device */
    rtc_device_t *dev = &g_rtc.devices[g_rtc.device_count++];
    dev->device_id = 1;
    strcpy(dev->name, "CMOS RTC");
    dev->is_enabled = true;
    
    /* Check format */
    u8 status_b = rtc_read_reg(RTC_STATUS_B);
    dev->is_24hour = (status_b & RTC_STATUS_B_24H) != 0;
    dev->is_binary = (status_b & RTC_STATUS_B_DM) != 0;
    
    /* Read century if available */
    if (rtc_read_reg(0x32) != 0xFF) {
        dev->century = rtc_read_bcd(0x32);
    } else {
        dev->century = 20;  /* Assume 2000s */
    }
    
    g_rtc.current_device = dev;
    
    /* Get current time for verification */
    u32 h, m, s;
    rtc_get_time(&h, &m, &s);
    
    printk("[RTC] RTC initialized\n");
    printk("[RTC] Current time: %02d:%02d:%02d\n", h, m, s);
    printk("[RTC] Format: %s, %s\n", 
           dev->is_24hour ? "24-hour" : "12-hour",
           dev->is_binary ? "Binary" : "BCD");
    
    return 0;
}

int rtc_shutdown(void)
{
    printk("[RTC] Shutting down RTC driver...\n");
    
    /* Clear alarm */
    rtc_clear_alarm();
    
    g_rtc.device_count = 0;
    g_rtc.initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

rtc_driver_t *rtc_get_driver(void)
{
    return &g_rtc;
}

u64 rtc_get_timestamp(void)
{
    u32 year, month, day, hour, minute, second;
    
    rtc_get_date(&year, &month, &day, NULL);
    rtc_get_time(&hour, &minute, &second);
    
    /* Simple timestamp calculation */
    u64 timestamp = 0;
    
    /* Years since 1970 */
    for (u32 y = 1970; y < year; y++) {
        timestamp += 365 * 24 * 60 * 60;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            timestamp += 24 * 60 * 60;  /* Leap year */
        }
    }
    
    /* Months */
    u32 days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }
    
    for (u32 m = 0; m < month - 1; m++) {
        timestamp += days_in_month[m] * 24 * 60 * 60;
    }
    
    /* Days */
    timestamp += (day - 1) * 24 * 60 * 60;
    
    /* Hours, minutes, seconds */
    timestamp += hour * 60 * 60 + minute * 60 + second;
    
    return timestamp;
}
