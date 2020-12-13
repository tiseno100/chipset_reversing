/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Implementation of the ALi ALADDiN III chipset.
 *
 *
 *
 * Authors:	Tiseno100,
 *
 *		Copyright 2020 Tiseno100.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/io.h>
#include <86box/timer.h>

#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/port_92.h>
#include <86box/smram.h>
#include <86box/spd.h>
#include <86box/chipset.h>

typedef struct aladdin_iii_t
{
    uint8_t pci_conf[3][256];
    uint8_t smiajt;

    smram_t *smram;
    port_92_t *port_92;
} aladdin_iii_t;

static void
aladdin_iii_shadow_recalc(aladdin_iii_t *dev)
{
    for (uint32_t i = 0; i < 8; i++)
    {
        mem_set_mem_state_both((0xc0000 + (i << 14)), 0x4000, ((dev->pci_conf[0][0x4c] & (1 << i)) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) | ((dev->pci_conf[0][0x4e] & (1 << i)) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
        mem_set_mem_state_both((0xe0000 + (i << 14)), 0x4000, ((dev->pci_conf[0][0x4d] & (1 << i)) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) | ((dev->pci_conf[0][0x4f] & (1 << i)) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
    }
}

static void
aladdin_iii_smm_recalc(aladdin_iii_t *dev)
{
    if (dev->pci_conf[0][0x48] & 0x01)
    {
        switch ((dev->pci_conf[0][0x48] >> 1) & 0x07)
        {
        case 0x00:
            smram_enable(dev->smram, 0xd0000, 0xb0000, 0x10000, 0, 1);
            break;
        case 0x01:
            smram_enable(dev->smram, 0xd0000, 0xb0000, 0x10000, 1, 1);
            break;
        case 0x02:
            smram_enable(dev->smram, 0xa0000, 0xa0000, 0x20000, 0, 1);
            break;
        case 0x03:
            if (!(dev->pci_conf[0][0x47] & 0x04))
                smram_enable(dev->smram, 0xa0000, 0xa0000, 0x20000, 1, 1);
            break;
        case 0x04:
            smram_enable(dev->smram, 0x30000, 0xb0000, 0x20000, 0, 1);
            break;
        case 0x05:
            smram_enable(dev->smram, 0x30000, 0xb0000, 0x20000, 1, 1);
            break;
        }
    }
    else
        smram_disable_all();
}

static void
aladdin_iii_ide_handler(aladdin_iii_t *dev)
{
    ide_pri_disable();
    ide_sec_disable();
    if (dev->pci_conf[2][0x50] & 0x01)
    {
        ide_pri_enable();
        ide_sec_enable();

        ide_set_base(0, 0x1f0);
        ide_set_side(0, 0x3f6);

        ide_set_base(1, 0x170);
        ide_set_side(1, 0x376);
    }
}

static void
aladdin_iii_write(int func, int addr, uint8_t val, void *priv)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)priv;

    dev->pci_conf[0][addr] = val;

    switch (addr)
    {
    case 0x42:
        cpu_cache_ext_enabled = (val & 0x01);
        break;

    case 0x47:
        mem_set_mem_state_both(0x80000, 0x20000, (val & 0x02) ? (MEM_READ_EXTANY | MEM_WRITE_EXTANY) : (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL));
        mem_set_mem_state_both(0xa0000, 0x20000, !(val & 0x04) ? (MEM_READ_EXTANY | MEM_WRITE_EXTANY) : (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL));
        mem_set_mem_state_both(0xf00000, 0x100000, !(val & 0x08) ? (MEM_READ_EXTANY | MEM_WRITE_EXTANY) : (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL));
        break;

    case 0x48:
        aladdin_iii_smm_recalc(dev);
        break;

    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f:
        aladdin_iii_shadow_recalc(dev);
        break;

    case 0x60:
    case 0x62:
    case 0x64:
    case 0x66:
    case 0x68:
    case 0x6a:
    case 0x6c:
    case 0x6e:
        spd_write_drbs(dev->pci_conf[0], 0x60, 0x6f, 2);
        break;
    }
    pclog("M1521-NB: dev->regs[%02x] = %02x\n", addr, val);
}

static uint8_t
aladdin_iii_read(int func, int addr, void *priv)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)priv;

    return dev->pci_conf[0][addr];
}

static void
aladdin_iii_sb_write(int func, int addr, uint8_t val, void *priv)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)priv;
    dev->pci_conf[1][addr] = val;

    if (!func)
    {
        switch (addr)
        {
        case 0x43:
            if (val & 0x80)
                port_92_add(dev->port_92);
            else
                port_92_remove(dev->port_92);
            break;

        case 0x48:
            pci_set_irq_routing(PCI_INTA, (val & 0x0f));
            pci_set_irq_routing(PCI_INTB, (val & 0xf0));
            break;

        case 0x49:
            pci_set_irq_routing(PCI_INTC, (val & 0x0f));
            pci_set_irq_routing(PCI_INTD, (val & 0xf0));
            break;

        case 0x50:
            pci_set_irq_routing(5, (val & 0x0f));
            pci_set_irq_routing(6, (val & 0xf0));
            break;

        case 0x51:
            pci_set_irq_routing(7, (val & 0x0f));
            pci_set_irq_routing(8, (val & 0xf0));
            break;

        case 0x55:
            dev->smiajt = (val & 0x01);
            break;
        }
        pclog("M1523-SB: dev->regs[%02x] = %02x\n", addr, val);
    }
    else
    {
        if (dev->pci_conf[1][0x46] & 0x10)
        {
            dev->pci_conf[2][addr] = val;
            switch (addr)
            {
            case 0x50:
                aladdin_iii_ide_handler(dev);
                break;
            }
            pclog("M1523-IDE: dev->regs[%02x] = %02x\n", addr, val);
        }
    }
}

static uint8_t
aladdin_iii_sb_read(int func, int addr, void *priv)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)priv;

    return (!func) ? dev->pci_conf[2][addr] : dev->pci_conf[1][addr];
}

static void
aladdin_iii_reset(void *priv)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)priv;

    dev->pci_conf[0][0x00] = 0xb9;
    dev->pci_conf[0][0x01] = 0x10;
    dev->pci_conf[0][0x02] = 0x21;
    dev->pci_conf[0][0x03] = 0x15;
    dev->pci_conf[0][0x04] = 0x06;
    dev->pci_conf[0][0x07] = 0x07;
    dev->pci_conf[0][0x08] = 0x01;
    dev->pci_conf[0][0x0b] = 0x06;
    dev->pci_conf[0][0x0d] = 0x20;
    dev->pci_conf[0][0x5a] = 0x20;

    aladdin_iii_write(0, 0x42, 0x00, dev);
    aladdin_iii_write(0, 0x47, 0x00, dev);
    aladdin_iii_write(0, 0x48, 0x00, dev);
    aladdin_iii_write(0, 0x4c, 0x00, dev);
    aladdin_iii_write(0, 0x4d, 0x00, dev);
    aladdin_iii_write(0, 0x4e, 0x00, dev);
    aladdin_iii_write(0, 0x4f, 0x00, dev);

    dev->pci_conf[1][0x00] = 0xb9;
    dev->pci_conf[1][0x01] = 0x10;
    dev->pci_conf[1][0x02] = 0x23;
    dev->pci_conf[1][0x03] = 0x15;
    dev->pci_conf[1][0x07] = 0x02;
    dev->pci_conf[1][0x0a] = 0x01;
    dev->pci_conf[1][0x0b] = 0x06;
    dev->pci_conf[1][0x0e] = 0x80;

    aladdin_iii_sb_write(0, 0x43, 0x00, dev);
    aladdin_iii_sb_write(0, 0x48, 0x00, dev);
    aladdin_iii_sb_write(0, 0x49, 0x00, dev);
    aladdin_iii_sb_write(0, 0x50, 0x00, dev);
    aladdin_iii_sb_write(0, 0x51, 0x00, dev);

    dev->pci_conf[2][0x00] = 0xb9;
    dev->pci_conf[2][0x01] = 0x10;
    dev->pci_conf[2][0x02] = 0x52;
    dev->pci_conf[2][0x06] = 0x02;
    dev->pci_conf[2][0x07] = 0x80;
    dev->pci_conf[2][0x09] = 0xfa;
    dev->pci_conf[2][0x0a] = 0x01;
    dev->pci_conf[2][0x0b] = 0x01;
    dev->pci_conf[2][0x10] = 0xf1;
    dev->pci_conf[2][0x11] = 0x01;
    dev->pci_conf[2][0x14] = 0xf5;
    dev->pci_conf[2][0x15] = 0x03;
    dev->pci_conf[2][0x18] = 0x71;
    dev->pci_conf[2][0x19] = 0x01;
    dev->pci_conf[2][0x20] = 0x01;
    dev->pci_conf[2][0x21] = 0xf0;
    dev->pci_conf[2][0x3d] = 0x01;
    dev->pci_conf[2][0x3e] = 0x02;
    dev->pci_conf[2][0x3f] = 0x04;
    ide_pri_disable();
    ide_sec_disable();
    aladdin_iii_sb_write(1, 0x50, 0x00, dev);
}

static void
aladdin_iii_close(void *priv)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)priv;

    smram_del(dev->smram);

    free(dev);
}

static void *
aladdin_iii_init(const device_t *info)
{
    aladdin_iii_t *dev = (aladdin_iii_t *)malloc(sizeof(aladdin_iii_t));
    memset(dev, 0, sizeof(aladdin_iii_t));

    pci_add_card(PCI_ADD_NORTHBRIDGE, aladdin_iii_read, aladdin_iii_write, dev);
    pci_add_card(PCI_ADD_SOUTHBRIDGE, aladdin_iii_sb_read, aladdin_iii_sb_write, dev);
    dev->smram = smram_add();
    dev->port_92 = device_add(&port_92_pci_device);
    device_add(&ide_pci_2ch_device);

    aladdin_iii_reset(dev);
    return dev;
}

const device_t ali_aladdin_iii_device = {
    "ALi Aladdin III",
    DEVICE_PCI,
    0,
    aladdin_iii_init,
    aladdin_iii_close,
    aladdin_iii_reset,
    {NULL},
    NULL,
    NULL,
    NULL};
