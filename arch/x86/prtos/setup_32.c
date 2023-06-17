/*
 * $FILE: setup_32.c
 *
 * PRTOS partition code
 *
 * $LICENSE:
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the void iox_interrupt(void *data)hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "prtos-linux.h"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/virtio_console.h>
#include <linux/console.h>
#include <linux/proc_fs.h>
#include <linux/screen_info.h>

#include <asm/bitops.h>
#include <asm/bootparam.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/setup.h>

part_ctl_table_t *prtosPartCtrTab;
#if 0
EXPORT_SYMBOL(prtosPartCtrTab);
EXPORT_SYMBOL(libXmParams);
EXPORT_SYMBOL(prtos_get_physmem_map);
#endif

#ifdef CONFIG_IOX_RING
#define CONFIG_IOX_IRQ PRTOS_VT_EXT_IPVI0

extern int ioxbus_add_bus(void *base, u32 size);
extern void ioxbus_interrupt(int vector, void *data);
extern void iox_interrupt(int vector, void *data);
extern void ioxring_set_bus(void *base, u32 size);
#endif

struct attr2flags {
    char *attr;
    prtos_u32_t flag;
};

static struct attr2flags mem_area_flags_tab[] = {
    {"unmapped", PRTOS_MEM_AREA_UNMAPPED},
    {"shared", PRTOS_MEM_AREA_SHARED},
    {"read-only", PRTOS_MEM_AREA_READONLY},
    {"uncacheable", PRTOS_MEM_AREA_UNCACHEABLE},
    {"rom", PRTOS_MEM_AREA_ROM},
    {"flag0", PRTOS_MEM_AREA_FLAG0},
    {"flag1", PRTOS_MEM_AREA_FLAG1},
    {"flag2", PRTOS_MEM_AREA_FLAG2},
    {"flag3", PRTOS_MEM_AREA_FLAG3},
    {"none", 0},
};

static void flags_to_str(char *buf, prtos_u32_t flags) {
    int i, len;

    len = 0;
    *buf = '\0';
    for (i=0; mem_area_flags_tab[i].flag; ++i) {
        if (mem_area_flags_tab[i].flag & flags) {
            if (len == 0) {
                len = sprintf(buf, mem_area_flags_tab[i].attr);
            } else {
                len += sprintf(buf+len, "|%s", mem_area_flags_tab[i].attr);
            }
        }
    }
    if (*buf == '\0') {
        sprintf(buf, "none");
    }
}

static int prtos_procinfo(char *buf, char **start, off_t offset, int count, int *eof, void *data) {
    struct prtos_physical_mem_map *memMap;
    int e, len = 0;
    int limit = count - 80;
    unsigned int irq_mask;
    char flags[256];

    len += sprintf(buf+len, "PRTOS:%s\n", CONFIG_KERNELVERSION);
    len += sprintf(buf+len, "Id:%d\n", PCT_GET_PARTITION_ID(prtosPartCtrTab));
    len += sprintf(buf+len, "Name:%s\n", prtosPartCtrTab->name);
    memMap = prtosGetMemMap();
    for (e = 0; e<prtosPartCtrTab->noPhysicalMemAreas; e++) {
        flags_to_str(flags, memMap[e].flags);
        len += sprintf(buf+len, "Area:0x%08x:%d:%s\n", memMap[e].startAddr, memMap[e].size, flags);
        if (len >= limit) {
            goto out;
        }
    }

    sprintf(buf+len, "Interrupts:");
    irq_mask = prtosPartCtrTab->hwIrqs;
    while (irq_mask) {
        e = __ffs(irq_mask);
        len += sprintf(buf+len, " %d", e);
        irq_mask &= ~(1<<e);
    }
    len += sprintf(buf+len, "\n");

out:
    *eof = 1;
    return len;
}

static int __init create_prtos_procinfo(void)
{
    create_proc_read_entry("prtosinfo", 0, NULL, prtos_procinfo, NULL);
    return 0;
}
late_initcall(create_prtos_procinfo);

int is_io_server(void) {
    return (PCT_GET_PARTITION_ID(prtosPartCtrTab) == 0);
}
EXPORT_SYMBOL(is_io_server);

static __init char *virt_memory_setup(void) {
	struct prtos_physical_mem_map *memMap;
	int e, flags;

	flags = PRTOS_MEM_AREA_FLAG0|PRTOS_MEM_AREA_FLAG1|PRTOS_MEM_AREA_FLAG2|PRTOS_MEM_AREA_FLAG3|PRTOS_MEM_AREA_ROM;
	memMap = prtos_get_partition_mmap();
	for (e = 0; e<prtosPartCtrTab->noPhysicalMemAreas; e++) {
		if (!(memMap[e].flags & flags))
			e820_add_region(memMap[e].startAddr, memMap[e].size, E820_RAM);
#ifdef CONFIG_IOX
		if (memMap[e].flags & PRTOS_MEM_AREA_FLAG3) {
		    if (is_io_server()) {
		        ioxbus_add_bus((void *)memMap[e].startAddr, memMap[e].size);
		    }
#ifdef CONFIG_IOX_RING
		    if (!is_io_server()) {
		        ioxring_set_bus((void *)memMap[e].startAddr, memMap[e].size);
		    }
#endif
		}
#endif
	}
	return "PRTOS";
}

static unsigned virt_patch(u8 type, u16 clobber, void *ibuf, unsigned long addr, unsigned len) {
	return len;
}

#ifdef CONFIG_PRTOS_BOOTCONSOLE
static char prtos_con_buffer[512];
static void prtosboot_write_console(struct console *console, const char *string, unsigned len)
{
    unsigned int i, k;
    static int newline=1;

    k = 0;
    if (newline) {
        k = sprintf(prtos_con_buffer, "[P%d] ", PRTOS_PARTITION_SELF);
        newline = 0;
    }
    for (i=0; i<len; ++i, ++k) {
        prtos_con_buffer[k] = string[i];
        if (string[i] == '\n') {
            if (i == (len-1)) {
                newline = 1;
            } else {
                k += sprintf(prtos_con_buffer+k, "[P%d] ", PRTOS_PARTITION_SELF);
            }
        }
    }
    prtos_write_console(prtos_con_buffer, k);
}

static struct console prtosboot_console = {
	.name		= "prtos",
	.write		= prtosboot_write_console,
#ifdef CONFIG_PRTOS_DEBUGCONSOLE
	.flags		= CON_PRINTBUFFER | CON_ANYTIME,
#else
	.flags		= CON_PRINTBUFFER | CON_ANYTIME | CON_BOOT,
#endif
};
#endif

/* only used for debugging */
char xprintk_buf[512];
int xprintk(const char *fmt, ...)
{
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf(xprintk_buf, sizeof(xprintk_buf), fmt, ap);
	va_end(ap);
#ifdef CONFIG_PRTOS_BOOTCONSOLE
	prtosboot_write_console(0, xprintk_buf, n);
#else
	prtos_write_console(xprintk_buf, n);
#endif
	return n;
}
EXPORT_SYMBOL(xprintk);

struct pv_info prtos_pv_info = {
	.name = "prtos-1.0.0",
	.kernel_rpl = 1,
	.paravirt_enabled = 1,
	.shared_kernel_pmd = 1,
};

void prtos_idle(void) {
    prtos_x86_set_if();
    prtos_idle_self();
}
EXPORT_SYMBOL(prtos_idle);

asmlinkage void __init prtos_start_kernel(void) {

	prtosPartCtrTab = prtos_get_PCT();
	BUG_ON(!prtosPartCtrTab);
	BUG_ON(prtosPartCtrTab->magic != KTHREAD_MAGIC);

	pv_info = prtos_pv_info;
	pv_init_ops.patch = virt_patch;
	x86_init.resources.memory_setup = virt_memory_setup;

	if(!is_io_server()) {
		paravirt_disable_iospace();
	}

	init_cpu_paravirt();
	init_irq_paravirt();
	init_vmm_paravirt(__va(prtos_image_hdr.page_table), prtos_image_hdr.page_table_size/PAGE_SIZE);
	init_time_paravirt();
	init_roms_paravirt();

	lockdep_init();
#ifdef CONFIG_PRTOS_BOOTCONSOLE
    register_console(&prtosboot_console);
    add_preferred_console("prtos", 0, NULL);
#endif
    add_preferred_console("tty", 0, NULL);
	if (!is_io_server()) {
	    boot_params.screen_info.orig_video_mode = 0;
	    boot_params.screen_info.orig_video_lines = 0;
	    boot_params.screen_info.orig_video_cols = 0;
	    add_preferred_console("hvc", 0, NULL);
	}

#ifdef CONFIG_IOX_RING
	if (is_io_server()) {                                   /* XXX: Depends on the XML configuration */
	    prtos_setup_irq(CONFIG_IOX_IRQ, ioxbus_interrupt, NULL);
	} else {
	    prtos_setup_irq(CONFIG_IOX_IRQ, iox_interrupt, NULL);
	}
#endif

	i386_start_kernel();
}

#ifdef CONFIG_PRTOS_MULTIPLAN
static int __init prtos_switch_plan(void) {
	int err;

	if (!is_io_server())
		return 0;
	err=prtos_set_plan(1);
	printk("PRTOS: Switching to production plan ... %s\n", (err ? "FAILED": "OK"));
	return 0;
}
late_initcall(prtos_switch_plan);
#endif
