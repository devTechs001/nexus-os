/*
 * NEXUS OS - Programmable Interval Timer (PIT)
 * kernel/arch/x86_64/cpu/timer.c
 */

#include "../../../include/kernel.h"

/* PIT Ports */
#define PIT_CHANNEL_0   0x40
#define PIT_CHANNEL_1   0x41
#define PIT_CHANNEL_2   0x42
#define PIT_COMMAND     0x43

/* PIT Command */
#define PIT_CMD_CHANNEL_0   0x00
#define PIT_CMD_LOBYTE      0x10
#define PIT_CMD_HIBYTE      0x20
#define PIT_CMD_SQUARE_WAVE 0x06
#define PIT_CMD_BINARY      0x00

/* Frequency */
#define PIT_BASE_FREQ   1193182

extern void outb(unsigned short port, unsigned char val);

static u64 ticks = 0;

/**
 * arch_init_timer - Initialize PIT to specified frequency
 */
void arch_init_timer(u64 freq)
{
    u32 divisor = PIT_BASE_FREQ / (u32)freq;

    /* Send command byte */
    outb(PIT_COMMAND, PIT_CMD_CHANNEL_0 | PIT_CMD_LOBYTE | PIT_CMD_HIBYTE | PIT_CMD_SQUARE_WAVE | PIT_CMD_BINARY);

    /* Send divisor bytes */
    outb(PIT_CHANNEL_0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL_0, (u8)((divisor >> 8) & 0xFF));

    pr_info("x86_64 PIT initialized to %llu Hz\n", freq);
}

/**
 * timer_tick - Called from IRQ 0
 */
void timer_tick(void)
{
    ticks++;
}

/**
 * arch_get_pit_ticks - Get current PIT system ticks
 */
u64 arch_get_pit_ticks(void)
{
    return ticks;
}

u64 ktime_get_ms(void)
{
    return ticks; /* Assuming HZ=1000 */
}

u64 ktime_get_us(void)
{
    return ticks * 1000;
}

u64 ktime_get_ns(void)
{
    return ticks * 1000000;
}

u64 get_time_ns(void)
{
    return ktime_get_ns();
}

u64 ktime_get_sec(void)
{
    return ticks / 1000;
}
