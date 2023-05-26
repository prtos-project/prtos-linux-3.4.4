/*
 * $FILE: prtossrv.c
 *
 * PRTOS services for user space
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
#include <linux/module.h>

static long prtos_srv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	unsigned long c, a[8];

	ret = copy_from_user(a, (char*)arg, sizeof(a));
	if (ret != 0)
		return -EFAULT;
	c = a[0];
	switch (c) {
	    case raise_ipvi_nr:
	        prtos_raise_ipvi(a[1]);
	        break;
	    case get_vcpuid_nr:
	        ret = prtos_get_vcpuid();
	        break;
	    default:
	        return -EPERM;
	}

	return ret;
}

static struct file_operations prtos_srv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = prtos_srv_ioctl,
};

static struct miscdevice prtos_srv_dev = {
	MISC_DYNAMIC_MINOR,
	"prtos_ctl",
	&prtos_srv_fops,
};

static int __init prtos_srv_register(void)
{
	int ret;

	ret = misc_register(&prtos_srv_dev);
	if (ret<0){
		printk("cannot register device %s: error %d\n", prtos_srv_dev.name, ret);
	}
	return ret;
}

module_init(prtos_srv_register);
