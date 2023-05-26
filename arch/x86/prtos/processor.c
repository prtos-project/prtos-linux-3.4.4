/*
 * $FILE: processor.c
 *
 * processor paravirtualisation
 *
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

#include "prtos-linux.h"
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <asm/bootparam.h>
#include <asm/processor.h>
#include <asm/desc.h>
#include <asm/hw_irq.h>
#include <asm/e820.h>
#include <asm/paravirt.h>
#include <asm/pgtable_32.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/stackprotector.h>

static inline void virt_cpuid(unsigned int *ax, unsigned int *bx, unsigned int *cx, unsigned int *dx) {
	int function = *ax;

	native_cpuid(ax, bx, cx, dx);
	switch (function) {
	case 1: 
		*cx &= 0x00002000;
		*dx &= ~(BIT(X86_FEATURE_SEP) |
                 BIT(X86_FEATURE_PSE36) |
                 BIT(X86_FEATURE_PSE) |
                 BIT(X86_FEATURE_ACPI) |
                 BIT(X86_FEATURE_APIC) |
                 BIT(X86_FEATURE_PGE) |
                 BIT(X86_FEATURE_DS));
		*ax &= 0xFFFFF0FF;
		*ax |= 0x00000500;
		break;

	case 0x80000000:
		if (*ax > 0x80000008)
			*ax = 0x80000008;
		break;
	}
}

static inline void virt_load_tr_desc(void) {
	struct tss_struct *t = &per_cpu(init_tss, get_cpu());
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
	    if (prtos_x86_load_tss((struct x86Tss *)t) < 0)
			BUG();
	} else {
	    prtos_lazy_x86_load_tss((struct x86Tss *)t);
	}
}

static inline unsigned long virt_read_cr0(void) {
	return prtosPartCtrTab->arch.cr0;
}

static inline unsigned long virt_read_cr4(void) {
	return prtosPartCtrTab->arch.cr4;	
}

static inline void virt_write_cr0(unsigned long val) {
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
	    if (prtos_x86_load_cr0(val) < 0)
			BUG();
	} else {
	    prtos_lazy_x86_load_cr0(val);
	}
}

static inline void virt_write_cr4(unsigned long val) {
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
	    if (prtos_x86_load_cr4(val) < 0)
			BUG();
	} else {
	    prtos_lazy_x86_load_cr4(val);
	}
}

static inline unsigned long virt_get_debugreg(int regno) {
	return 0;
}

static inline void virt_set_debugreg(int regno, unsigned long value) {
}

static inline void virt_wbinvd(void) {
}

static inline void virt_load_idt(const struct desc_ptr *desc) {
    if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
        prtos_x86_load_idtr((struct x86DescReg *)desc);
    } else {
        prtos_lazy_x86_load_idtr((struct x86DescReg *)desc);
    }
}

static inline void virt_clts(void) {
	virt_write_cr0((virt_read_cr0()&~X86_CR0_TS));
}

static inline void virt_load_gdt(const struct desc_ptr *desc) {
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
	    if (prtos_x86_load_gdt((struct x86DescReg *)desc) < 0)
			BUG();
	} else {
	    prtos_lazy_x86_load_gdt((struct x86DescReg *)desc);
	}
}

static inline void virt_load_tls(struct thread_struct *t, unsigned int cpu) {
	struct desc_struct *gdt = get_cpu_gdt_table(cpu);
	unsigned short new_gs = 0;
	unsigned int i;

	loadsegment(gs, new_gs);
	for (i = 0; i < GDT_ENTRY_TLS_ENTRIES; i++) {
		gdt[GDT_ENTRY_TLS_MIN + i] = t->tls_array[i];
		if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
			if (prtos_x86_update_gdt(GDT_ENTRY_TLS_MIN + i, (struct x86Desc *)&gdt[GDT_ENTRY_TLS_MIN + i]) < 0)
				BUG();
		} else {
		    prtos_lazy_x86_update_gdt(GDT_ENTRY_TLS_MIN + i, (struct x86Desc *)&gdt[GDT_ENTRY_TLS_MIN + i]);
		}
	}
}

static inline void virt_write_gdt_entry(struct desc_struct *gdt, int entry, const void *desc, int type)
{
    if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
        if (prtos_x86_update_gdt(entry, (struct x86Desc *)gdt) < 0) {
            BUG();
        }
    } else {
        prtos_lazy_x86_update_gdt(entry, (struct x86Desc *)gdt);
    }
    native_write_gdt_entry(gdt, entry, desc, type);
}

static inline void virt_write_idt_entry(gate_desc *idt, int entry, const gate_desc *gate)
{
    if (entry >= 0x30 && entry < IDT_ENTRIES) {
        if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
            if (prtos_x86_update_idt(entry, (struct x86Gate *)gate) < 0)
                BUG();
        } else {
            prtos_lazy_x86_update_idt(entry, (struct x86Gate *)gate);
        }
    }
    native_write_idt_entry(idt, entry, gate);
}

/* TODO: Are these needed? */
static inline u64 virt_read_msr(unsigned int msr, int *err) {
#if 0
	u32 low=0, high=0;
	if (is_io_client())
		return 0;
	if(prtos_write_register64(RDMSR_REG64, msr, (u32)&high, (u32)&low) != PRTOS_OK)
		BUG();
	return ((u64)(high)<<32)|(u64)low;
#endif
	//BUG();
	return 0;
}

static inline int virt_write_msr(unsigned int msr, unsigned low, unsigned high) {
#if 0
	if (is_io_client())
		return 0;
	if(prtos_write_register64(WRMSR_REG64, msr, high, low) != PRTOS_OK)
		BUG();
	return 0;
#endif
	//BUG();
	return 0;
}

static inline void virt_set_ldt(const void *addr, unsigned entries) {
}

static inline void virt_load_sp0(struct tss_struct *tss, struct thread_struct *thread) {
	tss->x86_tss.sp1 = thread->sp0;
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE) {
	    if (prtos_x86_update_ss_sp(tss->x86_tss.ss1, tss->x86_tss.sp1, 1) < 0)
			BUG();
	} else {
	    prtos_lazy_x86_update_ss_sp(tss->x86_tss.ss1, tss->x86_tss.sp1, 1);
	}
}

static inline void virt_end_context_switch(struct task_struct *next)
{
    prtos_flush_hyp_batch();
	paravirt_end_context_switch(next);
}

static void virt_io_delay(void)
{
}

static inline __init void init_processor(void) {
	struct desc_ptr gdt_descr;
	struct desc_struct *gdt = get_cpu_gdt_table(get_cpu());

	cpu_detect(&new_cpu_data);
	new_cpu_data.x86_capability[0] = cpuid_edx(1);

	new_cpu_data.hard_math = 1;

	(gdt[GDT_ENTRY_KERNEL_CS]).a = 0x0000bfff;
	(gdt[GDT_ENTRY_KERNEL_CS]).b = 0x00cfba00;

	(gdt[GDT_ENTRY_KERNEL_DS]).a = 0x0000bfff;
	(gdt[GDT_ENTRY_KERNEL_DS]).b = 0x00cfb200;
	
	(gdt[GDT_ENTRY_DEFAULT_USER_CS]).a = 0x0000bfff;
	(gdt[GDT_ENTRY_DEFAULT_USER_CS]).b = 0x00cffa00;
	
	(gdt[GDT_ENTRY_DEFAULT_USER_DS]).a = 0x0000bfff;
	(gdt[GDT_ENTRY_DEFAULT_USER_DS]).b = 0x00cff200;

	(gdt[GDT_ENTRY_PNPBIOS_CS32]).a = 0x0000ffff;
	(gdt[GDT_ENTRY_PNPBIOS_CS32]).b = 0x0040ba00;
	
	(gdt[GDT_ENTRY_PNPBIOS_CS16]).a = 0x0000ffff;
	(gdt[GDT_ENTRY_PNPBIOS_CS16]).b = 0x0000ba00;
	
	(gdt[GDT_ENTRY_PNPBIOS_DS]).a = 0x0000ffff;
	(gdt[GDT_ENTRY_PNPBIOS_DS]).b= 0x0000b200;
	
	(gdt[GDT_ENTRY_PNPBIOS_TS1]).a = 0x0;
	(gdt[GDT_ENTRY_PNPBIOS_TS1]).b= 0x0000b200;
	
	(gdt[GDT_ENTRY_PNPBIOS_TS2]).a = 0x0;
	(gdt[GDT_ENTRY_PNPBIOS_TS2]).b = 0x0000b200;

	(gdt[GDT_ENTRY_APMBIOS_BASE]).a = 0x0000ffff;
	(gdt[GDT_ENTRY_APMBIOS_BASE]).b = 0x0040ba00;

	(gdt[GDT_ENTRY_APMBIOS_BASE+1]).a = 0x0000ffff;
	(gdt[GDT_ENTRY_APMBIOS_BASE+1]).b = 0x0000ba00;

	(gdt[GDT_ENTRY_APMBIOS_BASE+2]).a = 0x0000ffff;
	(gdt[GDT_ENTRY_APMBIOS_BASE+2]).b = 0x0040b200;
	
	(gdt[GDT_ENTRY_APMBIOS_BASE+2]).a = 0x0;
	(gdt[GDT_ENTRY_APMBIOS_BASE+2]).b = 0x00c0b200;
	
	per_cpu(init_tss, get_cpu()).x86_tss = (struct x86_hw_tss){
		.sp1 = sizeof(init_stack)+(long)&init_stack,
		.ss1 = __KERNEL_DS|1,
		.ss2 = __KERNEL_CS|1,
		.io_bitmap_base = IO_BITMAP_OFFSET,
	};
	
	memset((char *)per_cpu(init_tss, get_cpu()).io_bitmap, 0, IO_BITMAP_LONGS*sizeof(unsigned long));
	__set_tss_desc(0, GDT_ENTRY_TSS, &per_cpu(init_tss, get_cpu()));

	gdt_descr.address = (long)get_cpu_gdt_table(0);
	gdt_descr.size = GDT_SIZE - 1;

	load_gdt(&gdt_descr);
	__asm__ __volatile__ ("movl $"TO_STR(__KERNEL_DS|1)", %%eax\n\t" \
				"movl %%eax, %%ds\n\t" \
				"movl %%eax, %%ss\n\t" \
				"movl %%eax, %%es\n\t"			   \
				"ljmp $"TO_STR(__KERNEL_CS|1)", $1f\n\t" \
				"1:\n\t" ::);
}

/* These are in kernel/entry.S */
extern void native_iret(void);
extern void native_irq_enable_sysexit(void);
extern void native_usergs_sysret32(void);
extern void native_usergs_sysret64(void);

extern void prtos_x86_load_tssiret(void);

static const struct pv_cpu_ops prtos_cpu_ops __initdata = {
	.cpuid = virt_cpuid,
	.set_debugreg = virt_set_debugreg,
	.get_debugreg = virt_get_debugreg,
	.clts = virt_clts,
	.read_cr0 = virt_read_cr0,
	.write_cr0 = virt_write_cr0,
	.read_cr4 = virt_read_cr4,
	.read_cr4_safe = virt_read_cr4,
	.write_cr4 = virt_write_cr4,
#ifdef CONFIG_X86_64
	.read_cr8 = native_read_cr8,
	.write_cr8 = native_write_cr8,
#endif
	.wbinvd = virt_wbinvd,
	.read_msr = virt_read_msr,
	.write_msr = virt_write_msr,
	.read_tsc = native_read_tsc,
	.read_pmc = native_read_pmc,
	.read_tscp = native_read_tscp,
	.load_tr_desc = virt_load_tr_desc,
	.set_ldt = virt_set_ldt,
	.load_gdt = virt_load_gdt,
	.load_idt = virt_load_idt,
	.store_gdt = native_store_gdt,
	.store_idt = native_store_idt,
	.store_tr = native_store_tr,
	.load_tls = virt_load_tls,

	
#ifdef CONFIG_X86_64
	.load_gs_index = native_load_gs_index,
#endif
	.write_ldt_entry = native_write_ldt_entry,
	.write_gdt_entry = virt_write_gdt_entry,//native_write_gdt_entry,//
	.write_idt_entry = native_write_idt_entry,//native_write_idt_entry,//

	.alloc_ldt = paravirt_nop,
	.free_ldt = paravirt_nop,

	.load_sp0 = virt_load_sp0,

#if defined(CONFIG_X86_32) || defined(CONFIG_IA32_EMULATION)
	.irq_enable_sysexit = native_irq_enable_sysexit,
#endif
#ifdef CONFIG_X86_64
#ifdef CONFIG_IA32_EMULATION
	.usergs_sysret32 = native_usergs_sysret32,
#endif
	.usergs_sysret64 = native_usergs_sysret64,
#endif
	.iret = prtos_x86_iret,
	.swapgs = native_swapgs,

	.set_iopl_mask = native_set_iopl_mask,
	.io_delay = virt_io_delay,

    .start_context_switch = paravirt_start_context_switch,
    .end_context_switch = virt_end_context_switch,
};

__init void init_cpu_paravirt(void) {

	pv_cpu_ops = prtos_cpu_ops;

	init_processor();
	setup_stack_canary_segment(0);
	switch_to_new_gdt(0);
}
