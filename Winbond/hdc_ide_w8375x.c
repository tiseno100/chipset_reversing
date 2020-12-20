/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Implementation of the Winbond W8375x VL-IDE Controller.
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
#include <86box/hdc.h>
#include <86box/hdc_ide.h>

#define ENABLE_W8375X_LOG 1
#ifdef ENABLE_W8375X_LOG
int w8375x_do_log = ENABLE_W8375X_LOG;
static void
w8375x_log(const char *fmt, ...)
{
    va_list ap;

    if (w8375x_do_log)
    {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#define w8375x_log(fmt, ...)
#endif

typedef struct
{
    uint8_t index, cfg_lock, set_chip_id, chip_id,
        regs[256];
} w8375x_t;

static uint8_t w8375x_read(uint16_t addr, void *priv);

static void
w8375x_write(uint16_t addr, uint8_t val, void *priv)
{
    w8375x_t *dev = (w8375x_t *)priv;
    switch (addr)
    {
    //Chip ID determination for multi-chip mode
    case 0x1b0:
    case 0x130:
        dev->chip_id = val;
        break;

    //Index Port
    case 0x1b4:
    case 0x134:
        dev->index = val;
        break;

    case 0x1b8:
    case 0x138:
        dev->regs[dev->index] = val;

        if (!(dev->regs[0x83] & 0x02))
            dev->cfg_lock = !(dev->set_chip_id == dev->chip_id);
        else
            dev->cfg_lock = 0;

        if (!dev->cfg_lock)
        {
            w8375x_log("dev->regs[%02x] = %02x\n", dev->index, val);
            switch (dev->index)
            {
            case 0x81:
                ide_pri_disable();
                ide_sec_disable();

                if (val & 0x80)
                {
                    if (val & 0x01)
                        ide_pri_enable();
                    else if (val & 0x03)
                    {
                        ide_pri_enable();
                        ide_sec_enable();
                    }
                }
                break;

            case 0x83:
                //Index Port
                io_sethandler((dev->regs[0x83] & 0x01) ? 0x1b4 : 0x134, 0x0001, w8375x_read, NULL, NULL, w8375x_write, NULL, NULL, dev);

                //Data Port
                io_sethandler((dev->regs[0x83] & 0x01) ? 0x1b8 : 0x138, 0x0001, w8375x_read, NULL, NULL, w8375x_write, NULL, NULL, dev);

                //IDIN Port
                io_sethandler((dev->regs[0x83] & 0x01) ? 0x1b0 : 0x130, 0x0001, w8375x_read, NULL, NULL, w8375x_write, NULL, NULL, dev);

                //IDOUT Port
                io_sethandler((dev->regs[0x83] & 0x01) ? 0x1bc : 0x13c, 0x0001, w8375x_read, NULL, NULL, w8375x_write, NULL, NULL, dev);

                dev->set_chip_id = 0x60 + ((val >> 2) & 0x03);
                break;

            case 0x85:
                if (val & 0x01)
                {
                    ide_set_base(0, 0x1f0);
                    ide_set_side(0, 0x3f6);
                    ide_set_base(1, 0x170);
                    ide_set_side(1, 0x376);
                }
                else
                {
                    ide_set_base(1, 0x1f0);
                    ide_set_side(1, 0x3f6);
                    ide_set_base(0, 0x170);
                    ide_set_side(0, 0x376);
                }
                break;
            }
            break;
        }
    }
}

static uint8_t
w8375x_read(uint16_t addr, void *priv)
{
    w8375x_t *dev = (w8375x_t *)priv;

    if ((addr == 0x1bc) || (addr == 0x13c))
        return dev->chip_id;
    else
        return dev->regs[dev->index];
}

static void
w8375x_close(void *priv)
{
    w8375x_t *dev = (w8375x_t *)priv;

    free(dev);
}

static void *
w8375x_init(const device_t *info)
{
    w8375x_t *dev = (w8375x_t *)malloc(sizeof(w8375x_t));
    memset(dev, 0, sizeof(w8375x_t));
    device_add(&ide_vlb_2ch_device);

    //Default Registers
    dev->regs[0x80] = 0x8f;
    dev->regs[0x81] = 0x8f;
    dev->regs[0x82] = 0xff;
    dev->regs[0x83] = 0xff;
    dev->regs[0x84] = 0xff;
    dev->regs[0x85] = 0xff;
    dev->regs[0x86] = 0x80;
    dev->regs[0x87] = 0x8a;

    //Basic Programming(Mostly done for clearity purposes)
    w8375x_write(0x1b4, 0x81, dev);
    w8375x_write(0x1b8, 0x8f, dev);

    w8375x_write(0x1b4, 0x83, dev);
    w8375x_write(0x1b8, 0xff, dev);

    w8375x_write(0x1b4, 0x85, dev);
    w8375x_write(0x1b8, 0xff, dev);

    w8375x_write(0x1b4, 0, dev);

    return dev;
}

const device_t ide_w8375x_vlb_device = {
    "Winbond W8375X VL-IDE Controller",
    0,
    0,
    w8375x_init,
    w8375x_close,
    NULL,
    {NULL},
    NULL,
    NULL,
    NULL};
