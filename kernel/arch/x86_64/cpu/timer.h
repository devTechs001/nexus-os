/*
 * NEXUS OS - x86_64 PIT Timer
 * kernel/arch/x86_64/cpu/timer.h
 */

#ifndef _NEXUS_ARCH_X86_64_TIMER_H
#define _NEXUS_ARCH_X86_64_TIMER_H

#include "../../../include/types.h"

/* Initialize the PIT to the requested frequency (Hz) */
void arch_init_timer(u64 freq);

/* Called from the IRQ0 handler on every PIT tick */
void timer_tick(void);

/* PIT tick counter accessors */
u64 arch_get_pit_ticks(void);
u64 ktime_get_ms(void);
u64 ktime_get_us(void);
u64 ktime_get_ns(void);
u64 get_time_ns(void);
u64 ktime_get_sec(void);

#endif /* _NEXUS_ARCH_X86_64_TIMER_H */
