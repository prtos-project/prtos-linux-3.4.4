/*
 * $FILE: virtsrv.c
 *
 * Virtual services for Linux
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
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/netdevice.h>

static DEFINE_SEMAPHORE(virtsrv_sem);

static long virtsrv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	unsigned long c, a[5];

	c = _IOC_NR(cmd);
	if (_IOC_TYPE(cmd) != 'X' || c >= NR_HYPERCALLS+1)
		return -EINVAL;

	ret = copy_from_user(a, (char*)arg, sizeof(a));
	if (ret != 0)
		return -EFAULT;

	pr_debug("virtsrv_ioctl cmd %#lx arg %#lx %#lx %#lx %lx %lx\n",
		c, a[0], a[1], a[2], a[3], a[4]);

	/*
	 * ioctl encoding:
	 * - cmd: encodes the prtos_hypercall number
	 * - arg: encodes the prtos_hypercall arguments (0..5 args)
	 */

	if (down_interruptible(&virtsrv_sem))
		return -ERESTARTSYS;

	switch(c){
	default:
		ret = -EINVAL;
		break;

	/* ordered by hypercall nr */
	case halt_partition_nr:
		ret = prtos_halt_partition(a[0]);
		break;
	case suspend_partition_nr:
		ret = prtos_suspend_partition(a[0]);
		break;
	case resume_partition_nr:
		ret = prtos_resume_partition(a[0]);
		break;
	case reset_partition_nr:
		ret = prtos_reset_partition(a[0], a[1], a[2]);
		break;
	case shutdown_partition_nr:
		ret = prtos_shutdown_partition(a[0]);
		break;

	case halt_system_nr:
		ret = prtos_halt_system();
		break;
	case reset_system_nr:
		ret = prtos_reset_system(a[0]);
		break;
	case idle_self_nr:
		ret = prtos_idle_self();
		break;

	case get_time_nr:
		ret = prtos_get_time(a[0], (prtos_time_t*)a[1]);
		break;
	case set_timer_nr:
		ret = prtos_set_timer(a[0], a[1], a[2]);
		break;
	
	case read_object_nr:
		ret = prtos_read_object(a[0], (char*)a[1], a[2], (prtos_u32_t*)a[3]);
		break;
	case write_object_nr:
		ret = prtos_write_object(a[0], (char*)a[1], a[2], (prtos_u32_t*)a[3]);
		break;
	case seek_object_nr:
		ret = prtos_seek_object(a[0], a[1], a[2]);
		break;
	case ctrl_object_nr:
		ret = prtos_ctrl_object(a[0], a[1], (char*)a[2]);
		break;
	
	/* get libPrtosParam PRTOS_PARTITION_SELF */
	case NR_HYPERCALLS:
		ret = PRTOS_PARTITION_SELF;
		break;
	}

	up(&virtsrv_sem);
	return ret;
}

static struct file_operations virtsrv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = virtsrv_ioctl,
};

static struct miscdevice virtsrv_dev =	{
	MISC_DYNAMIC_MINOR,
	"prtos_ctl",
	&virtsrv_fops,
};

static int __init virtsrv_register(void)
{
	int ret;

	ret = misc_register(&virtsrv_dev);
	if (ret<0){
		printk("cannot register device %s: error %d\n", virtsrv_dev.name, ret);
	}
	return ret;
}

module_init(virtsrv_register);
