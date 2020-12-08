/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Implementation of the Micronics MIC 471(MCM 2) chipset.
 *
 *      Note: This chipset has no datasheet, everything were done via
 *      reverse engineering the BIOS of various machines using it.
 *
 *      Authors: Tiseno100
 *
 *		Copyright 2020 Tiseno100
 *
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/timer.h>
#include <86box/io.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/port_92.h>
#include <86box/chipset.h>

#define ENABLE_MIC_471_LOG 1
#ifdef ENABLE_MIC_471_LOG
int mic471_do_log = ENABLE_MIC_471_LOG;
static void
mic471_log(const char *fmt, ...)
{
    va_list ap;

    if (mic471_do_log)
    {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#define mic471_log(fmt, ...)
#endif

typedef struct
{
    /* Base Registers */
    uint8_t index, regs[256];

    /* Memory Control Registers*/
    uint16_t can_read, can_write;

    /* Shadow Registers */
    uint32_t base;

} mic471_t;

static void
mic471_write(uint16_t addr, uint8_t val, void *priv)
{
    mic471_t *dev = (mic471_t *)priv;

    switch (addr)
    {
    case 0x22:
        dev->index = val;
        break;

    case 0x23:

        mic471_log("MIC 471: dev->regs[%02x] = %02x\n", dev->index, val);

        dev->regs[dev->index] = val;

        switch (dev->index)
        {
        case 0x52: /* Shadow RAM Controller */
        case 0x57: /* Memory & Cache Controller */
            dev->can_read = (dev->regs[0x57] & 0x40) ? MEM_READ_INTERNAL : MEM_READ_DISABLED;
            dev->can_write = (dev->regs[0x57] & 0x80) ? MEM_WRITE_INTERNAL : MEM_WRITE_DISABLED;

            for (uint32_t i = 0; i < 8; i++)
            {
                dev->base = 0xc0000 + (i << 15);

                if (dev->regs[0x52] & (1 << i))
                    mem_set_mem_state_both(dev->base, 0x8000, dev->can_read | dev->can_write);
                else
                    mem_set_mem_state_both(dev->base, 0x8000, MEM_READ_EXTANY | MEM_WRITE_EXTANY);
            }
            break;
        }
        break;
    }
}

static uint8_t
mic471_read(uint16_t addr, void *priv)
{
    mic471_t *dev = (mic471_t *)priv;
    mic471_log("MIC 471: dev->regs[%02x] (%02x)\n", dev->index, dev->regs[dev->index]);
    return (addr == 0x23) ? dev->regs[dev->index] : dev->index;
}

static void
mic471_close(void *priv)
{
    mic471_t *dev = (mic471_t *)priv;

    free(dev);
}

static void *
mic471_init(const device_t *info)
{
    mic471_t *dev = (mic471_t *)malloc(sizeof(mic471_t));
    memset(dev, 0, sizeof(mic471_t));

    /*
    MIC 471 Ports:
    22h Index Port
    23h Data Port
    */
    io_sethandler(0x0022, 0x0002, mic471_read, NULL, NULL, mic471_write, NULL, NULL, dev);

    /*
    MIC 471 Defaults:
    50h: System Controller
    51h: ??
    52h: Shadow RAM Controller
    53h: ??
    54h: ??
    55h: ??
    56h: ??
    57h: Memory & Cache Controller
    58h: SMM & SMI Controller(?)
    59h: ??
    60h: ??
    61h: ??
    */
    dev->regs[0x50] = 0x90;
    dev->regs[0x51] = 0x01;
    dev->regs[0x57] = 0x38;
    dev->regs[0x58] = 0x30;
    dev->regs[0x59] = 0xc8;
    dev->regs[0x60] = 0xc0;
    dev->regs[0x61] = 0xe7;

    device_add(&port_92_device);

    return dev;
}

const device_t mic471_device = {
    "MIC 471",
    0,
    0,
    mic471_init,
    mic471_close,
    NULL,
    {NULL},
    NULL,
    NULL,
    NULL};
