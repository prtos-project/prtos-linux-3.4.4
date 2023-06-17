/*
 * $FILE: vmm.c
 *
 * memory paravirtualisation
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
#include <linux/memblock.h>
#include <asm/bootparam.h>
#include <asm/processor.h>
#include <asm/desc.h>
#include <asm/hw_irq.h>
#include <asm/e820.h>
#include <asm/paravirt.h>
#include <asm/pgtable_32.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/pgtable_types.h>
#include <asm/stacktrace.h>

#define PGT_MASK 0x003FF000
#define BUG_TRACE() do { \
    dump_stack(); \
    BUG(); \
} while (0)

#define virt_lazy_update_page32(pte, val)    virt_lazy_hypercall(update_page32_nr, 2, pte, val)
#define virt_lazy_set_page_type(addr, type)  virt_lazy_hypercall(set_page_type_nr, 2, addr, type)
#define virt_lazy_invld_tlb(val)             virt_lazy_hypercall(invld_tlb_nr, 1, val)
#define virt_lazy_x86_load_cr3(cr3)          virt_lazy_hypercall(x86_load_cr3_nr, 1, cr3)

#define MMU_BATCH_LEN 512

static prtos_u32_t mmu_mc_batch[MMU_BATCH_LEN];
static volatile prtos_u32_t batch_len=0;

static void virt_lazy_hypercall(prtos_u32_t hcall_nr, prtos_s32_t no_args, ...) {
    va_list args;
    int i;

    if ((batch_len+no_args+2) >= MMU_BATCH_LEN) {
        prtos_multicall(&mmu_mc_batch[0], &mmu_mc_batch[batch_len]);
        batch_len = 0;
    }
    mmu_mc_batch[batch_len++] = hcall_nr;
    mmu_mc_batch[batch_len++] = no_args;
    va_start(args, no_args);
    for (i=0; i<no_args; i++)
        mmu_mc_batch[batch_len++] = va_arg(args, prtos_u32_t);
    va_end(args);
}

static void virt_flush_batch(void) {
    if (batch_len > 0) {
        prtos_multicall(&mmu_mc_batch[0], &mmu_mc_batch[batch_len]);
        batch_len = 0;
    }
}

static inline void __set_page_type(unsigned long addr, int type) {
    u32 pgte, pgde;
	u32 *pgd, *pgt, oval;
	s32 ret;

	pgte = ((u32)__va(addr)&PGT_MASK)>>PAGE_SHIFT;
	pgde = (u32)__va(addr)>>PGDIR_SHIFT;
	pgd = (u32*)((prtosPartCtrTab->arch.cr3&PAGE_MASK)+__PAGE_OFFSET);
    pgt = (u32*)(pgd[pgde]&PAGE_MASK);
    oval = ((u32 *)__va(pgt))[pgte];

	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_MMU) {
		virt_lazy_update_page32((u32)&(pgt[pgte]), oval&~_PAGE_PRESENT);
		virt_lazy_set_page_type(addr, type);
		if (type == PPAG_STD)
			oval |= _PAGE_RW;
		virt_lazy_update_page32((u32)&(pgt[pgte]), oval);
	} else {
	    if (prtos_update_page32((u32)&(pgt[pgte]), oval&~_PAGE_PRESENT)<0)
            BUG_TRACE();
        ret = prtos_set_page_type(addr, type);
        if ((ret != PRTOS_OK) && (ret != PRTOS_NO_ACTION)) {
            BUG_TRACE();
        }
        if (type == PPAG_STD)
            oval |= _PAGE_RW;
        if (prtos_update_page32((u32)&(pgt[pgte]), oval))
            BUG_TRACE();
    }
}

static inline void virt_flush_tlb(void) {
    if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_MMU) {
        virt_lazy_invld_tlb(-1);
    } else {
        prtos_invld_tlb(-1);
    }
}

static inline void virt_flush_tlb_single(unsigned long x) {
    if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_MMU) {
        virt_lazy_invld_tlb(x);
    } else {
        if (prtos_invld_tlb(x) < 0) {
            BUG_TRACE();
        }
    }
}

static inline unsigned long virt_read_cr2(void) {
	return prtosPartCtrTab->arch.cr2;	
}

static inline unsigned long virt_read_cr3(void) {
	return prtosPartCtrTab->arch.cr3;
}

static inline void virt_write_cr3(unsigned long cr3) {
    unsigned long old_cr3;

    old_cr3 = virt_read_cr3();
    if (old_cr3 == cr3) {
        virt_flush_tlb();
    } else {
        __set_page_type(cr3, PPAG_PTDL1);
        if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_MMU) {
            virt_lazy_x86_load_cr3(cr3);
        } else {
            if (prtos_x86_load_cr3(cr3)<0) {
                BUG_TRACE();
            }
        }
        __set_page_type(old_cr3, PPAG_STD);
    }
}

static inline void virt_set_pmd(pmd_t *pmdp, pmd_t pmdval) {
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_MMU) {
		virt_lazy_update_page32(__pa(pmdp), *((u32*)&pmdval));
	} else {
		if (prtos_update_page32(__pa(pmdp), *((u32*)&pmdval)) != PRTOS_OK) {
			BUG_TRACE();
		}
	}
}

static inline void virt_set_pte(pte_t *ptep, pte_t pte) {
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_MMU) {
		virt_lazy_update_page32(__pa(ptep), pte_val(pte));
	} else {
		if (prtos_update_page32(__pa(ptep), pte_val(pte)) != PRTOS_OK) {
			BUG_TRACE();
		}
	}
}

static inline void virt_set_pte_at(struct mm_struct *mm, unsigned long addr, pte_t *ptep, pte_t pte) {
	virt_set_pte(ptep, pte);
}

static void virt_alloc_pte(struct mm_struct *mm, unsigned long pfn) {
	__set_page_type(pfn<<PAGE_SHIFT, PPAG_PTDL2);
}

static void virt_alloc_pmd(struct mm_struct *mm, unsigned long pfn) {
	__set_page_type(pfn<<PAGE_SHIFT, PPAG_PTDL1);
}

static void virt_release_pte(unsigned long pfn) {
	__set_page_type(pfn<<PAGE_SHIFT, PPAG_STD);
}
	
static int virt_pgd_alloc(struct mm_struct *mm) {
	return 0;
}

static void virt_pgd_free(struct mm_struct *mm, pgd_t *pgd) {
	__set_page_type(__pa(mm->pgd), PPAG_STD);
}

static void virt_release_pmd(unsigned long pfn) {
	__set_page_type(pfn<<PAGE_SHIFT, PPAG_STD);
}

static void prtos_leave_lazy_mode(void) {
    preempt_disable();
    virt_flush_batch();
    paravirt_leave_lazy_mmu();
    preempt_enable();
}


#define PTE_IDENT __PV_IS_CALLEE_SAVE(_paravirt_ident_32)

static const struct pv_mmu_ops prtos_mmu_ops __initdata = {

	.read_cr2 = virt_read_cr2,
	.write_cr2 = native_write_cr2,
	.read_cr3 = virt_read_cr3,
	.write_cr3 = virt_write_cr3,

	.flush_tlb_user = virt_flush_tlb,
	.flush_tlb_kernel = virt_flush_tlb,
	.flush_tlb_single = virt_flush_tlb_single,
	.flush_tlb_others = native_flush_tlb_others,

	.pgd_alloc = virt_pgd_alloc,
	.pgd_free = virt_pgd_free,

	.alloc_pte = virt_alloc_pte,
	.alloc_pmd = virt_alloc_pmd,
	.release_pte = virt_release_pte,
	.release_pmd = virt_release_pmd,

	.alloc_pud = paravirt_nop,
	.release_pud = paravirt_nop,

	.set_pte = virt_set_pte,
	.set_pte_at = virt_set_pte_at,
	.set_pmd = virt_set_pmd,

	.pte_update = paravirt_nop,
	.pte_update_defer = paravirt_nop,

	.ptep_modify_prot_start = __ptep_modify_prot_start,
	.ptep_modify_prot_commit = __ptep_modify_prot_commit,

#if PAGETABLE_LEVELS >= 3
#ifdef CONFIG_X86_PAE
	.set_pte_atomic = native_set_pte_atomic,
	.pte_clear = native_pte_clear,
	.pmd_clear = native_pmd_clear,
#endif
	.set_pud = native_set_pud,

	.pmd_val = PTE_IDENT,
	.make_pmd = PTE_IDENT,

#if PAGETABLE_LEVELS == 4
	.pud_val = PTE_IDENT,
	.make_pud = PTE_IDENT,

	.set_pgd = native_set_pgd,
#endif
#endif /* PAGETABLE_LEVELS >= 3 */

	.pte_val = PTE_IDENT,
	.pgd_val = PTE_IDENT,

	.make_pte = PTE_IDENT,
	.make_pgd = PTE_IDENT,

	.dup_mmap = paravirt_nop,
	.exit_mmap = paravirt_nop,
	.activate_mm = paravirt_nop,

	.lazy_mode = {
		.enter = paravirt_enter_lazy_mmu,
		.leave = prtos_leave_lazy_mode,
	},

	.set_fixmap = native_set_fixmap,
};

__init void init_vmm_paravirt(unsigned long *pgd, int noPg) {
    struct prtos_physical_mem_map  *memMap;
	extern unsigned long PrtosPcRom[];
	extern struct prtos_image_hdr __prtos_image_hdr;
	unsigned long *pgt, page, addr;
	int e, i, noRsv;

	memMap = (struct prtosPhysicalMemMap *)((prtosAddress_t)prtosPartCtrTab+sizeof(part_ctl_table_t));

	for (e=0; e<(__PAGE_OFFSET>>PGDIR_SHIFT); e++)
		if (pgd[e]&_PAGE_PRESENT) {
		    virt_lazy_update_page32(__pa(&pgd[e]), 0);
		}
	virt_flush_batch();

	for (e=0, max_pfn_mapped=0; e<prtosPartCtrTab->noPhysicalMemAreas; e++)
		if (max_pfn_mapped < (memMap[e].startAddr+memMap[e].size))
			max_pfn_mapped = memMap[e].startAddr+memMap[e].size;

	memcpy(initial_page_table, pgd, PAGE_SIZE);
	memcpy(swapper_pg_dir, pgd, PAGE_SIZE);
	pgt = (unsigned long *)&pgd[1024];
	for (e=0, noRsv=0; e<noPg; e++) {
		for (i = (__PAGE_OFFSET>>PGDIR_SHIFT); i<1024; i++)
			if ((pgd[i]&PAGE_MASK) == __pa(&pgt[e*1024]))
				break;

		if (i >= 1024) {
			__set_page_type(__pa(&pgt[e*1024]), PPAG_STD);	
		} else
			noRsv++;
	}

	virt_write_cr3(__pa(initial_page_table));

	// Mapping all the pcROM
	page = (u32)PrtosPcRom;
	if (!(initial_page_table[0].pgd&_PAGE_PRESENT)) {
		__set_page_type(__pa(page), PPAG_PTDL2);
		prtos_update_page32(__pa(&(initial_page_table[0].pgd)), __pa(page)|_PAGE_PRESENT|_PAGE_RW);
		page += PAGE_SIZE;
	} else
		pgt = __va(initial_page_table[0].pgd&PAGE_MASK);
	
	if (!(pgt[0]&_PAGE_PRESENT)) {
		virt_lazy_update_page32(__pa(&pgt[0]), __pa(page)|_PAGE_PRESENT|_PAGE_RW);
		page += PAGE_SIZE;
	}
	for (addr=640*1024; addr<1024*1024; addr+=PAGE_SIZE)
		if (!(pgt[addr>>PAGE_SHIFT]&_PAGE_PRESENT)) {
			virt_lazy_update_page32(__pa(&pgt[addr>>PAGE_SHIFT]), __pa(page)|_PAGE_PRESENT|_PAGE_RW);
			page += PAGE_SIZE;
		}

	memblock_reserve(__pa(pgd), (noRsv+1)*PAGE_SIZE);
	memblock_reserve((u32)(&__prtos_image_hdr), sizeof(__prtos_image_hdr));
	memblock_reserve((u32)__pa(&libPrtosParams), sizeof(struct libPrtosParams));

	virt_flush_batch();
	max_pfn_mapped >>= PAGE_SHIFT;	

    memblock_dump_all();

	pv_mmu_ops = prtos_mmu_ops;

	reserve_top_address(-PRTOS_PCTRLTAB_ADDR);
}
