/*
 * $FILE: time.c
 *
 * time emulation
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
#include <linux/clockchips.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/smp.h>
#include <asm/delay.h>
#include <asm/time.h>
#include <asm/timer.h>
#include <asm/setup.h>

static void prtos_pit_clockevent_set_mode(enum clock_event_mode mode,
					struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		break;  /* unsupported */

	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_RESUME:
		break;

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		prtos_set_timer(PRTOS_HW_CLOCK, 0, 0);  /* cancel timeout */
		break;
	}
}

static int prtos_pit_clockevent_set_next_event(unsigned long delta,
					struct clock_event_device *evt)
{
	prtosTime_t t;

	/* Please wake us this far in the future. */
	prtos_get_time(PRTOS_HW_CLOCK, &t);
	t += (delta/NSEC_PER_USEC);
	prtos_set_timer(PRTOS_HW_CLOCK, t, 0);
	return 0;
}

#define PRTOS_CLOCK_MIN_DELTA 50000UL
#define PRTOS_CLOCK_MAX_DELTA ULONG_MAX

static struct clock_event_device prtos_pit_clockevent = {
	.name					= "prtos-pit",
	.features				= CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event			        = prtos_pit_clockevent_set_next_event,
	.set_mode				= prtos_pit_clockevent_set_mode,
	.rating					= INT_MAX,
	.mult					= 1,
	.shift					= 0,
	.min_delta_ns			= PRTOS_CLOCK_MIN_DELTA,
	.max_delta_ns			= PRTOS_CLOCK_MAX_DELTA,
};

static cycle_t prtos_pit_clock_read(struct clocksource *cs) {
	prtosTime_t t;
	prtos_get_time(PRTOS_HW_CLOCK, &t);
	return t*1000ULL;
}

u64 prtos_sched_clock(void) {
    prtosTime_t t;
    prtos_get_time(PRTOS_HW_CLOCK, &t);
    return t*1000ULL;
}

#define PRTOS_SHIFT 22
static struct clocksource prtos_pit_clock = {
	.name		= "prtos-clock",
	.rating		= 300,
	.read		= prtos_pit_clock_read,
	.mask		= ~0,
	.mult		= 1<<PRTOS_SHIFT,
	.shift		= PRTOS_SHIFT,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void prtos_pit_time_irq(int vector, void * data) {
	unsigned long flags;
	/* Don't interrupt us while this is running. */
	local_irq_save(flags);
	prtos_pit_clockevent.event_handler(&prtos_pit_clockevent);
	local_irq_restore(flags);
}

static unsigned long virt_get_tsc_khz(void) {
	return prtosPartCtrTab->cpuKhz;
}

static unsigned long virt_get_wallclock(void) {
	return 0;
}

static void __init virt_time_init(void) {
	clocksource_register(&prtos_pit_clock);
	prtos_pit_clockevent.cpumask = cpumask_of(0);
	clockevents_register_device(&prtos_pit_clockevent);
	prtos_setup_irq(PRTOS_VT_EXT_HW_TIMER, prtos_pit_time_irq, NULL);
}

static const struct pv_time_ops prtos_time_ops __initdata = {
	.sched_clock = prtos_sched_clock,
};

__init void init_time_paravirt(void) {
	pv_time_ops = prtos_time_ops;
	x86_init.timers.timer_init = virt_time_init;
	x86_platform.calibrate_tsc = virt_get_tsc_khz;
	x86_platform.get_wallclock = virt_get_wallclock;
}
