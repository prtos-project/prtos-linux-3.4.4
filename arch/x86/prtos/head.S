/*
 * $FILE: head_32.S
 *
 * Generic boot code, sets up a valid stack and jumps to the C code
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
#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>

.section .prtosinit.text, "ax", @progbits
ENTRY(prtos_boot)

	movl $(init_thread_union+THREAD_SIZE-__PAGE_OFFSET),%esp	/* Stack physical address */
	mov %ebx, prtos_pct-__PAGE_OFFSET

	pushl $(_page_table-__PAGE_OFFSET)
	call prtos_setup_vmmap                                         /* Remap kernel */

	mov $prtos_boot_params, %esi

	jmp startup_32

.section .init.text, "ax", @progbits
ENTRY(prtos_entry)
	movl $(init_thread_union+THREAD_SIZE),%esp                  /* Stack virtual address */
	mov prtos_pct, %ebx
	pushl %ebx
    call init_libprtos

	jmp prtos_start_kernel

__PAGE_ALIGNED_DATA
ENTRY(PrtosPcRom)
	.zero (96+1+1)*PAGE_SIZE // Pages+1 PGT

ENTRY(prtos_pct)
	.long 0
