/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Implementation of the Macronics MXIC 307 chipset.
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
#include <86box/mem.h>
#include <86box/device.h>
#include <86box/port_92.h>
#include <86box/chipset.h>

#ifdef ENABLE_MXIC307_LOG
int mxic307_do_log = ENABLE_MXIC307_LOG;
static void
mxic307_log(const char *fmt, ...)
{
    va_list ap;

    if (mxic307_do_log)
    {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#define mxic307_log(fmt, ...)
#endif

typedef struct
{
    /* Base */
    uint8_t index, regs[256];

    /* Shadowing */
    uint16_t can_read, can_write;
    uint32_t base;

} mxic307_t;

static void
mxic307_write(uint16_t addr, uint8_t val, void *priv)
{
    mxic307_t *dev = (mxic307_t *)priv;

    switch (addr)
    {
    case 0x22:
        dev->index = val;
        break;

    case 0x23:
        dev->regs[dev->index] = val;
        mxic307_log("dev->regs[%02x] = %02x \n", dev->index, val);

        switch (dev->index)
        {
        case 0x3a: /* Shadow Control */
            /*
            Bit 7: Read Enable
            Bit 6: Write Enable
            Bit 5: E8000-EFFFF
            Bit 4: E0000-E7FFF
            Bit 3: D8000-DFFFF
            Bit 2: D0000-D7FFF
            Bit 1: C8000-CFFFF
            Bit 0: C0000-C7FFF
            */
            dev->can_read = (val & 0x80) ? MEM_READ_INTERNAL : MEM_READ_EXTANY;
            dev->can_write = (val & 0x40) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY;

            for (uint32_t i = 0; i < 6; i++)
            {
                dev->base = 0xc0000 + (i << 15);
                if (val & (1 << i))
                    mem_set_mem_state_both(dev->base, 0x8000, dev->can_read | dev->can_write);
                else
                    mem_set_mem_state_both(dev->base, 0x8000, MEM_READ_EXTANY | MEM_WRITE_EXTANY);
            }

            mem_set_mem_state_both(
                0xf0000,
                0x10000,
                ((val & 0x80) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                ((val & 0x40) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
            flushmmucache();
            break;

        case 0x3d: /* DRAM Control */
            cpu_update_waitstates();
            break;

        case 0x3e: /* Cache Control */
            /*
            Bit 4: System Cache Enable
            */
            cpu_cache_int_enabled = (val & 0x10);
            break;
        }
        break;
    }
}

static uint8_t
mxic307_read(uint16_t addr, void *priv)
{
    mxic307_t *dev = (mxic307_t *)priv;
    return (addr == 0x23) ? dev->regs[dev->index] : dev->index;
}

static void
mxic307_close(void *priv)
{
    mxic307_t *dev = (mxic307_t *)priv;

    free(dev);
}

static void *
mxic307_init(const device_t *info)
{
    mxic307_t *dev = (mxic307_t *)malloc(sizeof(mxic307_t));
    memset(dev, 0, sizeof(mxic307_t));

    /*
    MXIC 307 Ports:
    22h Index Port
    23h Data Port
    */
    io_sethandler(0x022, 0x0002, mxic307_read, NULL, NULL, mxic307_write, NULL, NULL, dev);

    device_add(&port_92_device);

    /*
    MXIC 307 Registers:
    34h: ??
    35h: ??
    36h: ??
    37h: ??
    38h: ??
    39h: ??
    3ah: Shadow Control
    3bh: ??
    3ch: ??
    3dh: DRAM Control
    3eh: Cache Control
    */
    dev->regs[0x3b] = 0x03;
    dev->regs[0x3d] = 0x4c;
    dev->regs[0x3e] = 0x8e;

    return dev;
}

const device_t mxic307_device = {
    "Macronix MXIC307",
    0,
    0,
    mxic307_init,
    mxic307_close,
    NULL,
    {NULL},
    NULL,
    NULL,
    NULL};
