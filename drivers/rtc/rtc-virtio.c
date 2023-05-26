/*
 * RTC driver for virtio
 *  Copyright (C) 2011 Salva Peiró <speiro@ai2.upv.es>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>

#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/module.h>

static struct virtqueue *vq;
static struct rtc_device *rtc;
static u32 rtc_data=0;
static DECLARE_COMPLETION(have_data);

static void virtrtc_recv_done(struct virtqueue *vq)
{
	int len;
    
	if (!virtqueue_get_buf(vq, &len))
		return;
	complete(&have_data);
}

static void register_buffer(void)
{
	struct scatterlist sg;

	sg_init_one(&sg, &rtc_data, sizeof(rtc_data));
	/* There should always be room for one buffer. */
	if (virtqueue_add_buf(vq, &sg, 0, 1, &rtc_data, GFP_KERNEL) < 0)
		BUG();
	virtqueue_kick(vq);
}

static int virtio_read_time(struct device *dev, struct rtc_time *tm)
{
	if (!rtc_data)
		return rtc_data;

	wait_for_completion_interruptible(&have_data);
	rtc_time_to_tm(rtc_data, tm);
	init_completion(&have_data);
	register_buffer();
	return rtc_valid_tm(tm);
}
static int virtio_set_time(struct device *dev, struct rtc_time *tm)
{
    return 0;
}

static const struct rtc_class_ops virtio_rtc_ops = {
	.read_time	= virtio_read_time,
	.set_time = virtio_set_time,
};

static int virtrtc_probe(struct virtio_device *vdev)
{
	/* We expect a single virtqueue. */
	vq = virtio_find_single_vq(vdev, virtrtc_recv_done, "input");

	if (IS_ERR(vq))
		return PTR_ERR(vq);

	rtc = rtc_device_register("virtio", &vdev->dev, &virtio_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)){
		vdev->config->del_vqs(vdev);
		return PTR_ERR(rtc);
	}

	register_buffer();
	return 0;
}

static void virtrtc_remove(struct virtio_device *vdev)
{
	rtc_device_unregister(rtc);
	vdev->config->reset(vdev);
	vdev->config->del_vqs(vdev);
}

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_RTC, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_rtc = {
	.driver.name =	KBUILD_MODNAME,
	.id_table =	id_table,
	.probe =	virtrtc_probe,
	.remove =	__devexit_p(virtrtc_remove),
};

static int __init init(void)
{
	return register_virtio_driver(&virtio_rtc);
}

static void __exit fini(void)
{
	unregister_virtio_driver(&virtio_rtc);
}
module_init(init);
module_exit(fini);

MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio RTC driver");
MODULE_LICENSE("GPL");
