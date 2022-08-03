/* Copyright (c) 2020 Qualcomm Innovation Center, Inc.  All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define LED_FILE "/sys/class/leds/green/breath"

int LED_Ctrol(char *buff)
{
    struct file *fd = NULL;
    mm_segment_t fs;
    loff_t pos;

    fd = filp_open(LED_FILE, O_RDWR, 0);
    if (IS_ERR(fd)) {
	printk(KERN_ERR "open failed!\n");
	filp_close(fd, NULL);
        return -1;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    vfs_write(fd, buff, sizeof(buff), &pos);
    pos = 0;

    filp_close(fd, NULL);
    set_fs(fs);

    return 0;
}

static int  led_init(void)
{
    char buff_on[] = {"128"};
    LED_Ctrol(buff_on);
    printk(KERN_ALERT "LED ON\n");
    return 0;
}




static const struct of_device_id ov9282_of_match[] = {
	{ .compatible = "ovti,ov9282" },
	{ }
};



static int ov9282_probe(struct i2c_client *client){
	
	led_init();
	
	
	return 0;
}

MODULE_DEVICE_TABLE(of, ov9282_of_match);

static struct i2c_driver ov9282_driver = {
	.probe_new = ov9282_probe,
	.driver = {
		.name = "ov9282",
		.pm = &ov9282_pm_ops,
		.of_match_table = ov9282_of_match,
	},
};

module_i2c_driver(ov9282_driver);

MODULE_DESCRIPTION("OmniVision ov9282 sensor driver");
MODULE_LICENSE("GPL");
