/*
 * $FILE: prtos-linux.h
 *
 * PRTOS main paravirt header
 *
 * $LICENSE:
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _PRTOS_LINUX_H_
#define _PRTOS_LINUX_H_

#include <prtos.h>
#ifdef CONFIG_SMP
#define CONFIG_PRTOS_SMP
#endif /*CONFIG_SMP*/
#undef GDT_ENTRY    /* XXX: These symbols have to be undefined as they collide with the ones defined by Linux*/
#undef PAGE_ALIGN
#undef PAGE_SIZE
#undef CONFIG_SMP

#define PRTOS_OFFSET   CONFIG_PRTOS_OFFSET
#define NO_HWIRQS   CONFIG_NO_HWIRQS

#ifndef __ASSEMBLY__

#define __prtosinit    __attribute__((section(".prtosinit.text")))

extern part_ctl_table_t *prtosPartCtrTab;
extern struct prtos_image_hdr prtos_image_hdr;

extern void prtos_init_boot_params(void);
extern void init_irq_paravirt(void);
extern void init_time_paravirt(void);
extern void init_cpu_paravirt(void);
extern void init_vmm_paravirt(unsigned long *pgd, int noPg);
extern void init_roms_paravirt(void);

extern int prtos_setup_irq(int irq, void (*action)(int, void *), void * data);
extern int prtos_raise_ipvi_wrapper(int ipvi);

extern int xprintk(const char *fmt, ...);
extern int is_io_server(void);

static inline struct prtosPhysicalMemMap *prtosGetMemMap(void) {
    return (struct prtosPhysicalMemMap *)((prtosAddress_t)prtosPartCtrTab+sizeof(part_ctl_table_t));
}

#endif /*__ASSEMBLY__*/

#endif /*_PRTOS_LINUX_H_*/
