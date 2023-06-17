/*
 * $FILE: prtosmap.c
 *
 * PRTOS memory mapper device
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
#include "prtosmap.h"

#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/netdevice.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/module.h>

#include <asm/uaccess.h>

#define PRTOSMAP_NODE                  "prtosmap"
#define PRTOSMAP_COVERAGE              (PRTOS_MEM_AREA_FLAG0|PRTOS_MEM_AREA_FLAG1|PRTOS_MEM_AREA_FLAG2|PRTOS_MEM_AREA_FLAG3)

#define PRTOSMAP_AREA_AVAILABLE (1<<0)
#define PRTOSMAP_AREA_MAPPED (1<<1)

static unsigned char *prtosmap_status;
static struct prtos_physical_mem_map *prtosMemMap;

static struct prtos_physical_mem_map *reserve_area(unsigned long flag)
{
    int i;

    for (i=0; i<prtosPartCtrTab->noPhysicalMemAreas; ++i) {
        if (prtosMemMap[i].flags & flag) {
            if (prtosmap_status[i] == PRTOSMAP_AREA_AVAILABLE) {
                prtosmap_status[i] = 0;
                return &prtosMemMap[i];
            }
        }
    }

    return NULL;
}

static struct prtosPhysicalMemMap *find_area(struct prtosmap_area *area)
{
    int i;

    for (i=0; i<prtosPartCtrTab->noPhysicalMemAreas; ++i) {
        if (((void *)prtosMemMap[i].startAddr == area->start) &&(prtosMemMap[i].size == area->size)) {
            if (prtosmap_status[i] == PRTOSMAP_AREA_AVAILABLE) {
                prtosmap_status[i] = 0;
                return &prtosMemMap[i];
            } else {
                return NULL;
            }
        }
    }

    return NULL;
}

/* Need to return the area found */
long prtosmap_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct prtosPhysicalMemMap *memArea;
    struct prtosmap_area prtosmap_area;

    switch(cmd) {
#if 0
        case PRTOSMAP_ALLOC_AREA:
            copy_from_user(&prtosmap_area, (void *)arg, sizeof(struct prtosmap_area));
            if (!(prtosmap_area.flag & PRTOSMAP_COVERAGE)) {
                return -EPERM;
            }
            memArea = reserve_area(prtosmap_area.flag);
            if (memArea == NULL) {
                return -EBUSY;
            }
            prtosmap_area.start = (void *)memArea->startAddr;
            prtosmap_area.size = memArea->size;
            copy_to_user((void *)arg, &prtosmap_area, sizeof(struct prtosmap_area));
            file->private_data = memArea;
            break;
#endif

        case PRTOSMAP_SET_AREA:
            copy_from_user(&prtosmap_area, (void *)arg, sizeof(struct prtosmap_area));
            memArea = find_area(&prtosmap_area);
            if (memArea == NULL) {
                return -ENXIO;
            }
            file->private_data = memArea;
            break;

        default:
            return -1;
    }

    return 0;
}

int prtosmap_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    struct prtos_physical_mem_map *memArea;

    memArea = file->private_data;
    if (memArea == NULL) {
        return -ENODEV;
    }
    vma->vm_flags |= VM_IO | VM_RESERVED;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    if (remap_pfn_range(vma, start, (memArea->startAddr >> PAGE_SHIFT), vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
        return -EAGAIN;
    }

    return 0;
}

static struct file_operations prtosmap_fops = {
    .owner = THIS_MODULE,
    .mmap = prtosmap_mmap,
    .unlocked_ioctl = prtosmap_ioctl,
};

static struct miscdevice prtosmap_dev =  {
    MISC_DYNAMIC_MINOR,
    "prtosmap",
    &prtosmap_fops,
};

static int __init prtosmap_register(void)
{
    int ret, i;

    prtosmap_status = kzalloc(prtosPartCtrTab->noPhysicalMemAreas*sizeof(int), GFP_KERNEL);
    prtosMemMap = prtosGetMemMap();
    for (i=0; i<prtosPartCtrTab->noPhysicalMemAreas; ++i) {
        if (prtosMemMap[i].flags & PRTOSMAP_COVERAGE) {
            prtosmap_status[i] = PRTOSMAP_AREA_AVAILABLE;
        }
    }

    ret = misc_register(&prtosmap_dev);
    if (ret < 0) {
        printk("cannot register device %s: error %d\n", prtosmap_dev.name, ret);
    }

    return ret;
}
module_init(prtosmap_register);
