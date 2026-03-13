/*
 * NEXUS OS - Serial Driver
 * drivers/serial/serial.c
 *
 * UART/Serial port driver (16550 compatible)
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SERIAL CONFIGURATION                              */
/*===========================================================================*/

#define SERIAL_MAX_PORTS          8
#define SERIAL_FIFO_SIZE          16
#define SERIAL_BUFFER_SIZE        4096

/* Standard COM Ports */
#define COM1_BASE                 0x3F8
#define COM2_BASE                 0x2F8
#define COM3_BASE                 0x3E8
#define COM4_BASE                 0x2E8

/* UART Register Offsets */
#define UART_RX                   0x00  /* Receive Buffer */
#define UART_TX                   0x00  /* Transmit Buffer */
#define UART_IER                  0x01  /* Interrupt Enable */
#define UART_IIR                  0x02  /* Interrupt ID */
#define UART_FCR                  0x02  /* FIFO Control */
#define UART_LCR                  0x03  /* Line Control */
#define UART_MCR                  0x04  /* Modem Control */
#define UART_LSR                  0x05  /* Line Status */
#define UART_MSR                  0x06  /* Modem Status */
#define UART_SCR                  0x07  /* Scratch */
#define UART_DLL                  0x00  /* Divisor Latch Low */
#define UART_DLM                  0x01  /* Divisor Latch High */

/* Line Control Register */
#define UART_LCR_DLAB             0x80  /* Divisor Latch Access */
#define UART_LCR_8BIT             0x03  /* 8 data bits */
#define UART_LCR_1STOP            0x00  /* 1 stop bit */
#define UART_LCR_NOPARITY         0x00  /* No parity */

/* Line Status Register */
#define UART_LSR_DATA             0x01  /* Data Ready */
#define UART_LSR_THRE             0x20  /* Transmit Hold Empty */
#define UART_LSR_TEMT             0x40  /* Transmit Empty */

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 port_id;
    char name[16];
    u16 base_addr;
    u32 irq;
    u32 baud;
    u8 data_bits;
    u8 stop_bits;
    u8 parity;
    bool is_enabled;
    u8 rx_buffer[SERIAL_BUFFER_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u8 tx_buffer[SERIAL_BUFFER_SIZE];
    u32 tx_head;
    u32 tx_tail;
    u32 bytes_rx;
    u32 bytes_tx;
    u32 errors;
} serial_port_t;

typedef struct {
    bool initialized;
    serial_port_t ports[SERIAL_MAX_PORTS];
    u32 port_count;
    spinlock_t lock;
} serial_driver_t;

static serial_driver_t g_serial;

/*===========================================================================*/
/*                         SERIAL LOW-LEVEL OPERATIONS                       */
/*===========================================================================*/

static inline void serial_write_reg(u16 base, u8 offset, u8 value)
{
    vga_outb(base + offset, value);
}

static inline u8 serial_read_reg(u16 base, u8 offset)
{
    return vga_inb(base + offset);
}

static bool serial_is_transmit_empty(u16 base)
{
    return (serial_read_reg(base, UART_LSR) & UART_LSR_THRE) != 0;
}

static bool serial_is_data_ready(u16 base)
{
    return (serial_read_reg(base, UART_LSR) & UART_LSR_DATA) != 0;
}

/*===========================================================================*/
/*                         SERIAL CORE FUNCTIONS                             */
/*===========================================================================*/

int serial_init_port(u16 base, u32 irq, const char *name)
{
    if (g_serial.port_count >= SERIAL_MAX_PORTS) return -ENOMEM;
    
    serial_port_t *port = &g_serial.ports[g_serial.port_count++];
    memset(port, 0, sizeof(serial_port_t));
    
    port->port_id = g_serial.port_count;
    port->base_addr = base;
    port->irq = irq;
    port->baud = 115200;
    port->data_bits = 8;
    port->stop_bits = 1;
    port->parity = 0;
    strncpy(port->name, name, sizeof(port->name) - 1);
    
    /* Disable interrupts */
    serial_write_reg(base, UART_IER, 0x00);
    
    /* Enable DLAB */
    serial_write_reg(base, UART_LCR, UART_LCR_DLAB | UART_LCR_8BIT);
    
    /* Set divisor for 115200 baud (115200 = 115200 / (divisor * 16)) */
    u32 divisor = 115200 / (port->baud * 16);
    serial_write_reg(base, UART_DLL, divisor & 0xFF);
    serial_write_reg(base, UART_DLM, (divisor >> 8) & 0xFF);
    
    /* Disable DLAB, set 8N1 */
    serial_write_reg(base, UART_LCR, UART_LCR_8BIT);
    
    /* Enable FIFO */
    serial_write_reg(base, UART_FCR, 0x07);
    
    /* Enable RTS/DSR */
    serial_write_reg(base, UART_MCR, 0x03);
    
    /* Enable receive interrupts */
    serial_write_reg(base, UART_IER, 0x01);
    
    port->is_enabled = true;
    
    printk("[SERIAL] Initialized %s at 0x%03X (IRQ %d, %d baud)\n",
           name, base, irq, port->baud);
    
    return port->port_id;
}

int serial_putc(u32 port_id, char c)
{
    if (port_id > g_serial.port_count) return -EINVAL;
    
    serial_port_t *port = &g_serial.ports[port_id - 1];
    u16 base = port->base_addr;
    
    /* Wait for transmit buffer to be empty */
    while (!serial_is_transmit_empty(base));
    
    serial_write_reg(base, UART_TX, c);
    port->bytes_tx++;
    
    return 0;
}

int serial_puts(u32 port_id, const char *str)
{
    while (*str) {
        serial_putc(port_id, *str++);
    }
    return 0;
}

int serial_getc(u32 port_id, char *c, u32 timeout_ms)
{
    if (port_id > g_serial.port_count) return -EINVAL;
    
    serial_port_t *port = &g_serial.ports[port_id - 1];
    u16 base = port->base_addr;
    
    u64 start = ktime_get_ms();
    
    while (!serial_is_data_ready(base)) {
        if (timeout_ms > 0 && (ktime_get_ms() - start) > timeout_ms) {
            return -ETIMEDOUT;
        }
        cpu_relax();
    }
    
    *c = serial_read_reg(base, UART_RX);
    port->bytes_rx++;
    
    return 0;
}

int serial_write(u32 port_id, const void *data, u32 size)
{
    const u8 *buf = (const u8 *)data;
    
    for (u32 i = 0; i < size; i++) {
        serial_putc(port_id, buf[i]);
    }
    
    return size;
}

int serial_read(u32 port_id, void *data, u32 size, u32 timeout_ms)
{
    u8 *buf = (u8 *)data;
    u32 count = 0;
    
    for (u32 i = 0; i < size; i++) {
        if (serial_getc(port_id, &buf[i], timeout_ms) == 0) {
            count++;
        } else {
            break;
        }
    }
    
    return count;
}

int serial_set_baud(u32 port_id, u32 baud)
{
    if (port_id > g_serial.port_count) return -EINVAL;
    
    serial_port_t *port = &g_serial.ports[port_id - 1];
    u16 base = port->base_addr;
    
    port->baud = baud;
    
    /* Enable DLAB */
    serial_write_reg(base, UART_LCR, UART_LCR_DLAB | UART_LCR_8BIT);
    
    /* Set divisor */
    u32 divisor = 115200 / (baud * 16);
    serial_write_reg(base, UART_DLL, divisor & 0xFF);
    serial_write_reg(base, UART_DLM, (divisor >> 8) & 0xFF);
    
    /* Disable DLAB */
    serial_write_reg(base, UART_LCR, UART_LCR_8BIT);
    
    printk("[SERIAL] %s baud set to %d\n", port->name, baud);
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int serial_init(void)
{
    printk("[SERIAL] ========================================\n");
    printk("[SERIAL] NEXUS OS Serial Driver\n");
    printk("[SERIAL] ========================================\n");
    
    memset(&g_serial, 0, sizeof(serial_driver_t));
    spinlock_init(&g_serial.lock);
    
    /* Initialize standard COM ports */
    serial_init_port(COM1_BASE, 4, "ttyS0");
    serial_init_port(COM2_BASE, 3, "ttyS1");
    
    printk("[SERIAL] Serial driver initialized\n");
    return 0;
}

int serial_shutdown(void)
{
    printk("[SERIAL] Shutting down serial driver...\n");
    
    for (u32 i = 0; i < g_serial.port_count; i++) {
        serial_port_t *port = &g_serial.ports[i];
        
        /* Disable interrupts */
        serial_write_reg(port->base_addr, UART_IER, 0x00);
        port->is_enabled = false;
    }
    
    g_serial.port_count = 0;
    g_serial.initialized = false;
    
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

serial_driver_t *serial_get_driver(void)
{
    return &g_serial;
}

int serial_get_stats(u32 port_id, u32 *rx_bytes, u32 *tx_bytes, u32 *errors)
{
    if (port_id > g_serial.port_count) return -EINVAL;
    
    serial_port_t *port = &g_serial.ports[port_id - 1];
    
    if (rx_bytes) *rx_bytes = port->bytes_rx;
    if (tx_bytes) *tx_bytes = port->bytes_tx;
    if (errors) *errors = port->errors;
    
    return 0;
}
