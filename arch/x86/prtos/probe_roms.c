/*
 * $FILE: irq.c
 *
 * ROM support functions
 *
 * $LICENSE:
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "prtos-linux.h"
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/mmzone.h>
#include <linux/ioport.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/edd.h>
#include <linux/dmi.h>
#include <linux/pfn.h>
#include <linux/pci.h>
#include <asm/pci-direct.h>

#include <asm/e820.h>
#include <asm/mmzone.h>
#include <asm/setup.h>
#include <asm/sections.h>
#include <asm/io.h>
#include <asm/setup_arch.h>

static struct resource system_rom_resource = {
    .name   = "System ROM",
    .start  = 0xf0000,
    .end    = 0xfffff,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
};

static struct resource extension_rom_resource = {
    .name   = "Extension ROM",
    .start  = 0xe0000,
    .end    = 0xeffff,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
};

static struct resource adapter_rom_resources[] = { {
    .name   = "Adapter ROM",
    .start  = 0xc8000,
    .end    = 0,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
}, {
    .name   = "Adapter ROM",
    .start  = 0,
    .end    = 0,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
}, {
    .name   = "Adapter ROM",
    .start  = 0,
    .end    = 0,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
}, {
    .name   = "Adapter ROM",
    .start  = 0,
    .end    = 0,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
}, {
    .name   = "Adapter ROM",
    .start  = 0,
    .end    = 0,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
}, {
    .name   = "Adapter ROM",
    .start  = 0,
    .end    = 0,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
} };

static struct resource video_rom_resource = {
    .name   = "Video ROM",
    .start  = 0xc0000,
    .end    = 0xc7fff,
    .flags  = IORESOURCE_BUSY | IORESOURCE_READONLY | IORESOURCE_MEM
};

#define ROMSIGNATURE 0xaa55

static int __init romsignature(const unsigned char *rom)
{
    const unsigned short * const ptr = (const unsigned short *)rom;
    unsigned short sig;

    return probe_kernel_address(ptr, sig) == 0 && sig == ROMSIGNATURE;
}

static int __init romchecksum(const unsigned char *rom, unsigned long length)
{
    unsigned char sum, c;

    for (sum = 0; length && probe_kernel_address(rom++, c) == 0; length--)
        sum += c;
    return !length && !sum;
}

__init static int rom_available(unsigned long start, unsigned long end) {
    struct prtosPhysicalMemMap *memMap;
    int e;
    unsigned long size;

    size = end - start + 1;
    memMap = prtosGetMemMap();
    for (e = 0; e<prtosPartCtrTab->noPhysicalMemAreas; e++) {
        if ((memMap[e].startAddr <= start) && ((start+size) <= (memMap[e].startAddr+memMap[e].size))) {
            return 1;
        }
    }

    return 0;
}

__init static int probe_video_rom(void) {
    const unsigned char *rom;
    unsigned long start, length;
    unsigned char c;

    for (start = video_rom_resource.start; start < adapter_rom_resources[0].start; start += 2048) {
        rom = isa_bus_to_virt(start);
        if (!romsignature(rom))
            continue;

        video_rom_resource.start = start;

        if (probe_kernel_address(rom + 2, c) != 0)
            continue;

        /* 0 < length <= 0x7f * 512, historically */
        length = c * 512;

        /* if checksum okay, trust length byte */
        if (length && romchecksum(rom, length))
            video_rom_resource.end = start + length - 1;

        request_resource(&iomem_resource, &video_rom_resource);

        return 1;
    }

    return 0;
}

__init void probe_adapter_roms(unsigned long upper) {
    const unsigned char *rom;
    unsigned long start, length;
    unsigned char c;
    int i;

    /* check for adapter roms on 2k boundaries */
    start = adapter_rom_resources[0].start;
    for (i = 0; i < ARRAY_SIZE(adapter_rom_resources) && start < upper; start += 2048) {
        rom = isa_bus_to_virt(start);
        if (!romsignature(rom))
            continue;

        if (probe_kernel_address(rom + 2, c) != 0)
            continue;

        /* 0 < length <= 0x7f * 512, historically */
        length = c * 512;

        /* but accept any length that fits if checksum okay */
        if (!length || start + length > upper || !romchecksum(rom, length))
            continue;

        adapter_rom_resources[i].start = start;
        adapter_rom_resources[i].end = start + length - 1;
        request_resource(&iomem_resource, &adapter_rom_resources[i]);

        start = adapter_rom_resources[i++].end & ~2047UL;
    }
}

__init static int probe_extension_rom(void) {
    const unsigned char *rom;
    unsigned long length;

    rom = isa_bus_to_virt(extension_rom_resource.start);
    if (romsignature(rom)) {
        length = extension_rom_resource.end - extension_rom_resource.start + 1;
        if (romchecksum(rom, length)) {
            request_resource(&iomem_resource, &extension_rom_resource);
            return 1;
        }
    }

    return 0;
}

__init static void prtos_probe_roms(void) {
    unsigned long upper=0;

    if (rom_available(video_rom_resource.start, video_rom_resource.end)) {
        probe_video_rom();
    }
    if (rom_available(system_rom_resource.start, system_rom_resource.end)) {
        request_resource(&iomem_resource, &system_rom_resource);
        upper = system_rom_resource.start;
    }
    if (rom_available(extension_rom_resource.start, extension_rom_resource.end)) {
        if (probe_extension_rom()) {
            upper = extension_rom_resource.start;
        }
    }
    if (upper) {
        if (rom_available(adapter_rom_resources[0].start, upper-1)) {
            probe_adapter_roms(upper);
        }
    }
}

__init void init_roms_paravirt(void) {
    x86_init.resources.probe_roms = prtos_probe_roms;
}


