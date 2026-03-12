/*
 * NEXUS OS - Panic Handling
 * kernel/core/panic.c
 */

#include "../include/kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>

#define PANIC_STACK_TRACE_SIZE  64

static volatile int panic_in_progress = 0;

void dump_stack_trace(void)
{
    void *buffer[PANIC_STACK_TRACE_SIZE];
    int nptrs = backtrace(buffer, PANIC_STACK_TRACE_SIZE);
    
    printk("Stack trace:\n");
    backtrace_symbols_fd(buffer, nptrs, fileno(stdout));
}

void dump_registers(void)
{
    /* In real OS: dump CPU registers */
    printk("Register dump not available in userspace mode\n");
}

void dump_memory_info(void)
{
    /* In real OS: dump memory state */
    printk("Memory info not available in userspace mode\n");
}

void kernel_oops(const char *msg)
{
    printk("\n");
    printk("!!! OOPS: %s !!!\n", msg);
    printk("\n");

    dump_stack_trace();
}

void kernel_assert_failed(const char *expr, const char *file, int line)
{
    kernel_panic("ASSERTION FAILED: %s\nFile: %s, Line: %d", expr, file, line);
}

void install_panic_handler(void)
{
    /* In real OS: install exception handlers */
}
