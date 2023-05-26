/*
 * $FILE: irq.c
 *
 * PRTOS irq support functions
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
#include <linux/linkage.h>
#include <linux/acpi.h>
#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <asm/bitops.h>
#include <asm/ptrace.h>
#include <asm/desc.h>
#include <asm/hw_irq.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/paravirt.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/setup.h>
#include <asm/reboot.h>

static inline void prtos_pic_mask_irq(struct irq_data *data)
{
	if (prtos_set_irqmask((1 << data->irq), 0) < 0)
		BUG();
}

static inline void prtos_pic_unmask_irq(struct irq_data *data)
{
	if (prtos_clear_irqmask((1 << data->irq), 0) < 0)
	    BUG();
}

static inline int prtos_are_irqs_enabled(void) {
    return prtosPartCtrTab->iFlags&_CPU_FLAG_IF;
}

static struct irq_chip prtos_pic_chip = {
	.name		    = "prtos-pic",
	.irq_mask		= prtos_pic_mask_irq,
	.irq_disable   	= prtos_pic_mask_irq,
	.irq_unmask		= prtos_pic_unmask_irq,
	.irq_mask_ack	= prtos_pic_mask_irq,
};

#undef PV_SAVE_ALL_CALLER_REGS
#define PV_SAVE_ALL_CALLER_REGS	 \
	"pushl %ebp;" \
	"pushl %edi;" \
	"pushl %esi;" \
	"pushl %edx;" \
	"pushl %ecx;" \
	"pushl %ebx;"

#undef PV_RESTORE_ALL_CALLER_REGS
#define PV_RESTORE_ALL_CALLER_REGS \
	"popl %ebx;" \
	"popl %ecx;" \
	"popl %edx;" \
	"popl %esi;" \
	"popl %edi;" \
	"popl %ebp;"

static inline unsigned long prtos_save_fl(void)
{
    return prtosPartCtrTab->iFlags&_CPU_FLAG_IF;
}

static inline void prtos_restore_fl(unsigned long flags)
{
    if (flags&X86_EFLAGS_IF)
        prtos_x86_set_if();
    else
        prtos_x86_clear_if();
}
PV_CALLEE_SAVE_REGS_THUNK(prtos_save_fl);
PV_CALLEE_SAVE_REGS_THUNK(prtos_restore_fl);
PV_CALLEE_SAVE_REGS_THUNK(prtos_x86_clear_if);
PV_CALLEE_SAVE_REGS_THUNK(prtos_x86_set_if);

inline long __save_iflags(void)
{
    return prtos_save_fl();
}

inline void __restore_iflags(long iflags)
{
    prtos_restore_fl(iflags);
}

#define EXT_IRQ_VECTOR 0x90

#ifndef CONFIG_X86_LOCAL_APIC
int first_system_vector = 0xfe;
#endif

/*
 * source: lguest
 * Decode I/O instructions that generate #GP and ignore them
 */
int prtos_skip_io(struct pt_regs *regs)
{
    u8 instr;
    u32 addr, in=0, shift=0, insnlen=0;

    addr = (u32)regs->ip;
    instr = *((u8 *)addr);
    if (instr == 0x66) {
        shift = 16;
        insnlen += 1;
        instr = *((u8 *)addr+insnlen);
    }

    switch (instr & 0xFE) {
    case 0xE4: /* in     <next byte>,%al */
        insnlen += 2;
        in = 1;
        break;
    case 0xEC: /* in     (%dx),%al */
        insnlen += 1;
        in = 1;
        break;
    case 0xE6: /* out    %al,<next byte> */
        insnlen += 2;
        break;
    case 0xEE: /* out    %al,(%dx) */
        insnlen += 1;
        break;
    default:
        return 0;
    }

    if (in) {
        if (instr & 0x1)
            regs->ax = 0xFFFFFFFF;
        else
            regs->ax |= (0xFFFF << shift);
    }
    regs->ip += insnlen;
    return 1;
}

#ifdef CONFIG_CHIPSET_ICH
static inline void __init lpc_activate_apic_pirq(u8 offset) {
    extern u8 read_pci_config_byte(u8 bus, u8 slot, u8 func, u8 offset);
    extern void write_pci_config_byte(u8 bus, u8 slot, u8 func, u8 offset, u8 val);
    u8 config;
    config = read_pci_config_byte(0, 31, 0, offset);
    config |= 0x80;
    write_pci_config_byte(0, 31, 0, offset, config);
}

static inline void __init lpc_activate_apic(void) {
    extern void write_pci_config_byte(u8 bus, u8 slot, u8 func, u8 offset, u8 val);

    write_pci_config_byte(0, 31, 0, 0x4c, 0x10);    /* GPIO Control register */
    write_pci_config_byte(0, 31, 0, 0x64, 0xd0);    /* SERIRQ Control Register */

    lpc_activate_apic_pirq(0x60);
    lpc_activate_apic_pirq(0x61);
    lpc_activate_apic_pirq(0x62);
    lpc_activate_apic_pirq(0x63);

    lpc_activate_apic_pirq(0x68);
    lpc_activate_apic_pirq(0x69);
    lpc_activate_apic_pirq(0x6a);
    lpc_activate_apic_pirq(0x6b);
}

static inline void demux_gpio_pirq_pins(void) {
    u32 gpiobase, use_sel;
    extern u32 read_pci_config(u8 bus, u8 slot, u8 func, u8 offset);

    gpiobase = read_pci_config(0, 31, 0, 0x48) & ~0x3;
    use_sel = inl(gpiobase);
    use_sel &= ~0x3e;
    outl(use_sel, gpiobase);
    use_sel = inl(gpiobase);
}

#define ICH7_RCBA_D31IR 0x3140
#define ICH7_RCBA_BASE  0xFED1C000  /* Depends on the chipset */
static inline int __init fetch_rcba_irq_route(u32 rcba_base, int slot, int pin) {
    int irq;

    irq = *(volatile u16 *)(rcba_base + ICH7_RCBA_D31IR + ((31-slot)*2));
    irq = (irq>>((pin-1)*4)) & 0x7;

    return irq+16;
}

static inline int traverse_pci_bridges(void *rcba_base, struct pci_dev *dev, int pin) {
    struct pci_dev *bridge;
    int irq;

    bridge = dev;
    while (bridge->bus->parent) {
        pin = pci_swizzle_interrupt_pin(bridge, pin);
        bridge = bridge->bus->self;
    }

    irq = fetch_rcba_irq_route((u32)rcba_base, PCI_SLOT(bridge->devfn), pin);

    return irq;
}

#ifdef CONFIG_VX4BOX
static void __init prtos_fixup_irqs(void) {
    struct pci_dev *dev = NULL;
    void *rcba_base;
    u8 pin;

    lpc_activate_apic();
    demux_gpio_pirq_pins();
    rcba_base = ioremap(ICH7_RCBA_BASE, 16*1024);

    for_each_pci_dev(dev) {
        pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
        if (pin) {
            switch (dev->bus->number) {
                case 6:
                    dev->irq = 20;
                    break;
                case 1:
                    dev->irq = 16;
                    break;
                default:
                    if (PCI_SLOT(dev->devfn) == 2) {
                        dev->irq = 16;
                    } else {
                        dev->irq = fetch_rcba_irq_route((u32)rcba_base, PCI_SLOT(dev->devfn), pin);
                    }
                    break;
            }
            pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
        }
    }
    iounmap(rcba_base);
}
#endif /*CONFIG_VX4BOX*/

#ifdef CONFIG_MBD_X7SPA_H
static void __init prtos_fixup_irqs(void)
{
    struct pci_dev *dev = NULL;
    u8 pin;
    void *rcba_base;

    lpc_activate_apic();
    demux_gpio_pirq_pins();
    rcba_base = ioremap(ICH7_RCBA_BASE, 16*1024);

    for_each_pci_dev(dev) {
        pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
        if (pin) {
            switch (dev->bus->number) {
                case 2:
                    dev->irq = 16;
                    break;
                case 3:
                    dev->irq = 17;
                    break;
                default:
                    if (PCI_SLOT(dev->devfn) == 2) {
                        dev->irq = 16;
                    } else {
                        dev->irq = traverse_pci_bridges(rcba_base, dev, pin);
                    }
                    break;
            }
            pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
        }
    }
    iounmap(rcba_base);
}
#endif /*CONFIG_MBD_X7SPA_H*/

#endif /*CONFIG_CHIPSET_ICH*/

static void __init prtos_init_IRQ(void)
{
	extern void (*ext_interrupt[0])(void);
	unsigned int i;
	unsigned int irq_mask=0;

	for (i = FIRST_EXTERNAL_VECTOR; i < NR_VECTORS; i++) {
        __this_cpu_write(vector_irq[i], i - FIRST_EXTERNAL_VECTOR);
        if (i != SYSCALL_VECTOR)
            set_intr_gate(i, interrupt[i - FIRST_EXTERNAL_VECTOR]);
    }
	prtos_x86_update_idt(SYSCALL_VECTOR, &((struct x86Gate *)prtos_get_PCT()->arch.idtr.linearBase)[SYSCALL_VECTOR]);

	irq_mask = prtosPartCtrTab->hwIrqs;
	while (irq_mask) {
	    i = __ffs(irq_mask);
        irq_set_chip_and_handler_name(i, &prtos_pic_chip, handle_level_irq, "level");
        clear_bit(i, (volatile long unsigned int *)&irq_mask);
	}

	for (i = 0; i < PRTOS_VT_EXT_MAX; i++) {
	    prtos_route_irq(PRTOS_EXTIRQ_TYPE, i, EXT_IRQ_VECTOR+i);
		alloc_intr_gate(EXT_IRQ_VECTOR+i, ext_interrupt[i]);
	}

	irq_ctx_init(smp_processor_id());
}

static inline void prtos_safe_halt(void)
{
	prtos_x86_set_if();
}

static inline void prtos_halt(void)
{
}

static inline void prtos_restart(char *reason)
{
	xprintk("prtos_restart reason %s\n", reason);
	prtos_x86_clear_if();
	prtos_reset_partition(PRTOS_PARTITION_SELF, PRTOS_COLD_RESET, 1);
}

struct {
	void (*action)(int, void *);
	void * data;
} extIRQhndltab[PRTOS_VT_EXT_MAX] = { [0 ... (PRTOS_VT_EXT_MAX-1)] = {0, 0} };

int prtos_setup_irq(int irq, void (*action)(int, void *), void * data)
{
	if (!action)
		return -EINVAL;
	if (irq < PRTOS_VT_EXT_FIRST || irq > PRTOS_VT_EXT_LAST)
		return -EINVAL;

	extIRQhndltab[irq].data = data;
	extIRQhndltab[irq].action = action;
	prtos_clear_irqmask(0, 1<<irq);

	return 0;
}
EXPORT_SYMBOL(prtos_setup_irq);

int prtos_raise_ipvi_wrapper(int ipvi)
{
    return prtos_raise_ipvi(PRTOS_VT_EXT_IPVI0+ipvi);
}
EXPORT_SYMBOL(prtos_raise_ipvi_wrapper);

unsigned int __irq_entry do_extIRQ(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	int vector=~regs->orig_ax;

	irq_enter();
	prtos_set_irqmask(0, 1<<vector);
	if (extIRQhndltab[vector].action)
		extIRQhndltab[vector].action(PRTOS_VT_EXT_FIRST+vector, extIRQhndltab[vector].data);
	prtos_clear_irqmask(0, 1<<vector);
	irq_exit();
	set_irq_regs(old_regs);

	return 1;
}

static const struct pv_irq_ops prtos_irq_ops __initdata = {
	.save_fl = PV_CALLEE_SAVE(prtos_save_fl),
	.restore_fl= PV_CALLEE_SAVE(prtos_restore_fl),
	.irq_disable = PV_CALLEE_SAVE(prtos_x86_clear_if),
	.irq_enable = PV_CALLEE_SAVE(prtos_x86_set_if),
	.safe_halt = prtos_safe_halt,
	.halt = prtos_halt,
#ifdef CONFIG_X86_64
	.adjust_exception_frame = paravirt_nop,
#endif
};

__init void init_irq_paravirt(void)
{
	pv_irq_ops = prtos_irq_ops;
	if(!is_io_server())
		machine_ops.restart = prtos_restart;
	x86_init.irqs.intr_init = prtos_init_IRQ;

#ifdef CONFIG_CHIPSET_ICH
	x86_init.pci.fixup_irqs = prtos_fixup_irqs;
#endif
}
