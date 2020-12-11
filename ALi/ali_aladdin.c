/*
 * 86Box	A hypervisor and IBM PC system emulator that specializes in
 *		running old operating systems and software designed for IBM
 *		PC systems and compatibles from 1981 through fairly recent
 *		system designs based on the PCI bus.
 *
 *		This file is part of the 86Box distribution.
 *
 *		Implementation of the ALi ALADDiN chipset.
 *
 *      Note: Most components were taken from datasheets close to the chip
 *            and miscellaneous reverse engineering of the BIOS from random
 *            machines.
 * 
 *
 *      Authors: Tiseno100
 *
 *		Copyright 2020 Tiseno100
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include "cpu.h"
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/smram.h>
#include <86box/port_92.h>
#include <86box/keyboard.h>

typedef struct ali_aladdin_t
{
    uint8_t pci_conf[2][256];
    uint32_t base;

    port_92_t *port_92;
    smram_t *smram;
} ali_aladdin_t;

static void
ali_aladdin_write(int func, int addr, uint8_t val, void *priv)
{

    ali_aladdin_t *dev = (ali_aladdin_t *)priv;
    dev->pci_conf[0][addr] = val;

    switch (addr)
    {
    case 0x48: /* SMRAM Registers */
        switch (val & 0x07)
        {
        case 0x00:
            smram_enable(dev->smram, 0xd0000, 0xb0000, 0x10000, 0, 1);
            break;
        case 0x01:
            smram_enable(dev->smram, 0xa0000, 0xa0000, 0x20000, 1, 1);
            break;
        case 0x02:
            smram_enable(dev->smram, 0xa0000, 0xa0000, 0x20000, 0, 1);
            break;
        case 0x03:
            smram_enable(dev->smram, 0xa0000, 0xa0000, 0x20000, 1, 1);
            break;
        case 0x04:
            smram_enable(dev->smram, 0x30000, 0xb0000, 0x10000, 0, 1);
            break;
        case 0x05:
            smram_enable(dev->smram, 0x30000, 0xb0000, 0x10000, 1, 1);
            break;
        }
        break;

    case 0x4c: /* Shadow Registers */
    case 0x4d:
    case 0x4e:
    case 0x4f:
        for (uint32_t i = 0; i < 8; i++)
        {
            dev->base = 0xc0000 + (i << 14);
            mem_set_mem_state_both(dev->base, 0x4000, ((dev->pci_conf[0][0x4c] & (1 << i)) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) | (!(dev->pci_conf[0][0x4e] & (1 << i)) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
            dev->base = 0xe0000 + (i << 14);
            mem_set_mem_state_both(dev->base, 0x4000, ((dev->pci_conf[0][0x4d] & (1 << i)) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) | (!(dev->pci_conf[0][0x4f] & (1 << i)) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
        }
        break;
    }
    pclog("Northbridge: dev->pci_conf[%02x] = %02x\n", addr, val);
}

static uint8_t
ali_aladdin_read(int func, int addr, void *priv)
{
    ali_aladdin_t *dev = (ali_aladdin_t *)priv;
    //pclog("Northbridge: dev->pci_conf[%02x] (%02x)\n", addr, dev->pci_conf[0][addr]);
    return dev->pci_conf[0][addr];
}

static void
ali_aladdin_sb_write(int func, int addr, uint8_t val, void *priv)
{
    ali_aladdin_t *dev = (ali_aladdin_t *)priv;
    dev->pci_conf[1][addr] = val;

    switch (addr)
    {
    case 0x43:
        if (val & 0x80)
            port_92_add(dev->port_92);
        else
            port_92_remove(dev->port_92);
        break;

    case 0x48:
        if ((dev->pci_conf[1][0x48] & 0x08) && ((val & 0x0f) != 0))
            pci_set_irq_routing(PCI_INTA, val & 0x0f);
        else
            pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);

        if ((dev->pci_conf[1][0x48] & 0x80) && (((val & 0x0f) << 4) != 0))
            pci_set_irq_routing(PCI_INTB, ((val & 0x0f) << 4));
        else
            pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
        break;

    case 0x49:
        if ((dev->pci_conf[1][0x49] & 0x08) && ((val & 0x0f) != 0))
            pci_set_irq_routing(PCI_INTC, val & 0x0f);
        else
            pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);

        if ((dev->pci_conf[1][0x49] & 0x80) && (((val & 0x0f) << 4) != 0))
            pci_set_irq_routing(PCI_INTD, ((val & 0x0f) << 4));
        else
            pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);
        break;
    }
    pclog("Southbridge: dev->pci_conf[%02x] = %02x\n", addr, val);
}

static uint8_t
ali_aladdin_sb_read(int func, int addr, void *priv)
{
    ali_aladdin_t *dev = (ali_aladdin_t *)priv;
    //pclog("Southbridge: dev->pci_conf[%02x] (%02x)\n", addr, dev->pci_conf[1][addr]);
    return dev->pci_conf[1][addr];
}

static void
ali_aladdin_reset(void *priv)
{

    ali_aladdin_t *dev = (ali_aladdin_t *)priv;

    /* Default Northbridge Registers */
    dev->pci_conf[0][0x00] = 0xb9;
    dev->pci_conf[0][0x01] = 0x10;
    dev->pci_conf[0][0x02] = 0x51;
    dev->pci_conf[0][0x03] = 0x14;
    dev->pci_conf[0][0x07] = 0x02;
    dev->pci_conf[0][0x0a] = 0x01;
    dev->pci_conf[0][0x0b] = 0x06;

    /* Default Southbridge Registers */
    dev->pci_conf[1][0x00] = 0xb9;
    dev->pci_conf[1][0x01] = 0x10;
    dev->pci_conf[1][0x02] = 0x49;
    dev->pci_conf[1][0x03] = 0x14;
    dev->pci_conf[1][0x07] = 0x02;
    dev->pci_conf[0][0x0a] = 0x01;
    dev->pci_conf[0][0x0b] = 0x06;

    /* Configure things there so there's no need to mess up init */
    ali_aladdin_write(0, 0x48, 0x00, dev);
    ali_aladdin_write(0, 0x4c, 0x00, dev);
    ali_aladdin_write(0, 0x4d, 0x00, dev);
    ali_aladdin_write(0, 0x4e, 0x00, dev);
    ali_aladdin_write(0, 0x4f, 0x00, dev);

    ali_aladdin_sb_write(0, 0x43, 0x00, dev);
    ali_aladdin_sb_write(0, 0x48, 0x00, dev);
    ali_aladdin_sb_write(0, 0x49, 0x00, dev);
}

static void
ali_aladdin_close(void *priv)
{
    ali_aladdin_t *dev = (ali_aladdin_t *)priv;

    smram_del(dev->smram);
    free(dev);
}

static void *
ali_aladdin_init(const device_t *info)
{
    ali_aladdin_t *dev = (ali_aladdin_t *)malloc(sizeof(ali_aladdin_t));
    memset(dev, 0, sizeof(ali_aladdin_t));

    pci_add_card(PCI_ADD_NORTHBRIDGE, ali_aladdin_read, ali_aladdin_write, dev);
    pci_add_card(PCI_ADD_SOUTHBRIDGE, ali_aladdin_sb_read, ali_aladdin_sb_write, dev);
    dev->smram = smram_add();
    dev->port_92 = device_add(&port_92_pci_device);
    ali_aladdin_reset(dev);

    return dev;
}

const device_t ali_aladdin_device = {
    "ALi ALADDiN",
    DEVICE_PCI,
    0,
    ali_aladdin_init,
    ali_aladdin_close,
    ali_aladdin_reset,
    {NULL},
    NULL,
    NULL,
    NULL};
