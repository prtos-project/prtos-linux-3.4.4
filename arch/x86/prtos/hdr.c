/*
 * $FILE: hdr.c
 *
 * PRTOS header
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "prtos-linux.h"

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/setup.h>
#include <asm/page.h>

#define NO_PGTS         256
#define CMDLINE_SIZE    PAGE_SIZE

prtos_u8_t _pageTable[PAGE_SIZE*(NO_PGTS+1)] __attribute__((aligned(PAGE_SIZE))) __attribute__ ((section(".bss.noinit")));
static prtos_u8_t cmdline[CMDLINE_SIZE] __initdata;

__attribute__((section(".prtosImageHdr"))) struct prtosImageHdr __prtosImageHdr __PRTOSIHDR = {
    .sSignature=PRTOSEF_PARTITION_MAGIC,
    .compilationPrtosAbiVersion=PRTOS_SET_VERSION(PRTOS_ABI_VERSION, PRTOS_ABI_SUBVERSION, PRTOS_ABI_REVISION),
    .compilationPrtosApiVersion=PRTOS_SET_VERSION(PRTOS_API_VERSION, PRTOS_API_SUBVERSION, PRTOS_API_REVISION),
    .pageTable=(prtosAddress_t)__pa(_pageTable),
    .pageTableSize=PAGE_SIZE*(NO_PGTS+1),
    .noCustomFiles=1,
    .customFileTab={
        [0]=(struct xefCustomFile) {
            .sAddr=__pa(cmdline),
            .size=CMDLINE_SIZE,
        },
    },
    .eSignature=PRTOSEF_PARTITION_MAGIC,
};

__attribute__((section(".prtos_boot_params"))) struct boot_params prtos_boot_params = {
    .hdr.version = 0x207,
    .hdr.hardware_subarch = 3,
    .hdr.loadflags = KEEP_SEGMENTS,
    .hdr.type_of_loader = 0xa0,
    .hdr.cmd_line_ptr = __pa(cmdline),
    .hdr.cmdline_size = CMDLINE_SIZE,
    .screen_info={
        .orig_x = 0, /* 0x00 */
        .orig_y = 0, /* 0x01 */
        .orig_video_page = 8, /* 0x04 */
        .orig_video_mode = 3, /* 0x06 */
        .orig_video_cols = 80, /* 0x07 */
        .orig_video_ega_bx = 3, /* 0x0a */
        .orig_video_lines = 25, /* 0x0e */
        .orig_video_isVGA = 1, /* 0x0f */
        .orig_video_points = 16, /* 0x10 */
    },
    .e820_entries = 0,
};

notrace asmlinkage void __prtosinit prtos_setup_vmmap(prtosAddress_t *pgTab) {
    int i, e, k, ret;
    prtos_u32_t __mc_up32_batch[32][4];

    k = 0;
    i = __PAGE_OFFSET >> 22;
    for (e = 0; (e < i) && (e + i < 1024); e++) {
        if (pgTab[e]) {
            __mc_up32_batch[k][0] = update_page32_nr;         /* NoHyp */
            __mc_up32_batch[k][1] = 2;                          /* NoArgs */
            __mc_up32_batch[k][2] = (prtosAddress_t)&pgTab[e+i];   /* A0 */
            __mc_up32_batch[k][3] = pgTab[e];                   /* A1 */
            ++k;
        }
        if (k >= 32) {
            _PRTOS_HCALL2((void *)&__mc_up32_batch[0], (void *)&__mc_up32_batch[k], multicall_nr, ret);
            k = 0;
        }
    }
    if (k > 0) {
        _PRTOS_HCALL2((void *)&__mc_up32_batch[0], (void *)&__mc_up32_batch[k], multicall_nr, ret);
    }
}
