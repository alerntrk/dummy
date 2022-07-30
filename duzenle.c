
// SPDX-License-Identifier: GPL-2.0-only
/*
 * OmniVision ov9282 Camera Sensor Driver
 *
 * Copyright (C) 2021 Intel Corporation
 */
#include <asm/unaligned.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

/* Streaming Mode */
#define OV9282_REG_MODE_SELECT	0x0100
#define OV9282_MODE_STANDBY	0x00
#define OV9282_MODE_STREAMING	0x01

/* Lines per frame */
#define OV9282_REG_LPFR		0x380e

/* Chip ID */
#define OV9282_REG_ID		0x300a
#define OV9282_ID		0x9281

/* Exposure control */
#define OV9282_REG_EXPOSURE	0x3500
#define OV9282_EXPOSURE_MIN	1
#define OV9282_EXPOSURE_OFFSET	12
#define OV9282_EXPOSURE_STEP	1
#define OV9282_EXPOSURE_DEFAULT	0x0282

/* Analog gain control */
#define OV9282_REG_AGAIN	0x3509
#define OV9282_AGAIN_MIN	0x10
#define OV9282_AGAIN_MAX	0xff
#define OV9282_AGAIN_STEP	1
#define OV9282_AGAIN_DEFAULT	0x10

/* Group hold register */
#define OV9282_REG_HOLD		0x3308

/* Input clock rate */
#define OV9282_INCLK_RATE	24000000

/* CSI2 HW configuration */
#define OV9282_LINK_FREQ	400000000
#define OV9282_NUM_DATA_LANES	2

#define OV9282_REG_MIN		0x00
#define OV9282_REG_MAX		0xfffff

/**
 * struct ov9282_reg - ov9282 sensor register
 * @address: Register address
 * @val: Register value
 */
struct ov9282_reg {
	u16 address;
	u8 val;
};

/**
 * struct ov9282_reg_list - ov9282 sensor register list
 * @num_of_regs: Number of registers in the list
 * @regs: Pointer to register list
 */
struct ov9282_reg_list {
	u32 num_of_regs;
	const struct ov9282_reg *regs;
};

/**
 * struct ov9282_mode - ov9282 sensor mode structure
 * @width: Frame width
 * @height: Frame height
 * @code: Format code
 * @hblank: Horizontal blanking in lines
 * @vblank: Vertical blanking in lines
 * @vblank_min: Minimum vertical blanking in lines
 * @vblank_max: Maximum vertical blanking in lines
 * @pclk: Sensor pixel clock
 * @link_freq_idx: Link frequency index
 * @reg_list: Register list for sensor mode
 */
struct ov9282_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u64 pclk;
	u32 link_freq_idx;
	struct ov9282_reg_list reg_list;
};

/**
 * struct ov9282 - ov9282 sensor device structure
 * @dev: Pointer to generic device
 * @client: Pointer to i2c client
 * @sd: V4L2 sub-device
 * @pad: Media pad. Only one pad supported
 * @reset_gpio: Sensor reset gpio
 * @inclk: Sensor input clock
 * @ctrl_handler: V4L2 control handler
 * @link_freq_ctrl: Pointer to link frequency control
 * @pclk_ctrl: Pointer to pixel clock control
 * @hblank_ctrl: Pointer to horizontal blanking control
 * @vblank_ctrl: Pointer to vertical blanking control
 * @exp_ctrl: Pointer to exposure control
 * @again_ctrl: Pointer to analog gain control
 * @vblank: Vertical blanking in lines
 * @cur_mode: Pointer to current selected sensor mode
 * @mutex: Mutex for serializing sensor controls
 * @streaming: Flag indicating streaming state
 */
struct ov9282 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *pclk_ctrl;
	struct v4l2_ctrl *hblank_ctrl;
	struct v4l2_ctrl *vblank_ctrl;
	struct {
		struct v4l2_ctrl *exp_ctrl;
		struct v4l2_ctrl *again_ctrl;
	};
	u32 vblank;
	const struct ov9282_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
};

static const s64 link_freq[] = {
	OV9282_LINK_FREQ,
};

/* Sensor mode registers */
static const struct ov9282_reg mode_1280x720_regs[] = {
	{0x0302, 0x32},
	{0x030d, 0x50},
	{0x030e, 0x02},
	{0x3001, 0x00},
	{0x3004, 0x00},
	{0x3005, 0x00},
	{0x3006, 0x04},
	{0x3011, 0x0a},
	{0x3013, 0x18},
	{0x301c, 0xf0},
	{0x3022, 0x01},
	{0x3030, 0x10},
	{0x3039, 0x32},
	{0x303a, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x5f},
	{0x3502, 0x1e},
	{0x3503, 0x08},
	{0x3505, 0x8c},
	{0x3507, 0x03},
	{0x3508, 0x00},
	{0x3509, 0x10},
	{0x3610, 0x80},
	{0x3611, 0xa0},
	{0x3620, 0x6e},
	{0x3632, 0x56},
	{0x3633, 0x78},
	{0x3666, 0x00},
	{0x366f, 0x5a},
	{0x3680, 0x84},
	{0x3712, 0x80},
	{0x372d, 0x22},
	{0x3731, 0x80},
	{0x3732, 0x30},
	{0x3778, 0x00},
	{0x377d, 0x22},
	{0x3788, 0x02},
	{0x3789, 0xa4},
	{0x378a, 0x00},
	{0x378b, 0x4a},
	{0x3799, 0x20},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x05},
	{0x3805, 0x0f},
	{0x3806, 0x02},
	{0x3807, 0xdf},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x02},
	{0x380b, 0xd0},
	{0x380c, 0x05},
	{0x380d, 0xfa},
	{0x380e, 0x06},
	{0x380f, 0xce},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x3c},
	{0x3821, 0x84},
	{0x3881, 0x42},
	{0x38a8, 0x02},
	{0x38a9, 0x80},
	{0x38b1, 0x00},
	{0x38c4, 0x00},
	{0x38c5, 0xc0},
	{0x38c6, 0x04},
	{0x38c7, 0x80},
	{0x3920, 0xff},
	{0x4003, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x05},
	{0x400c, 0x00},
	{0x400d, 0x03},
	{0x4010, 0x40},
	{0x4043, 0x40},
	{0x4307, 0x30},
	{0x4317, 0x00},
	{0x4501, 0x00},
	{0x4507, 0x00},
	{0x4509, 0x80},
	{0x450a, 0x08},
	{0x4601, 0x04},
	{0x470f, 0x00},
	{0x4f07, 0x00},
	{0x4800, 0x20},
	{0x5000, 0x9f},
	{0x5001, 0x00},
	{0x5e00, 0x00},
	{0x5d00, 0x07},
	{0x5d01, 0x00},
	{0x0101, 0x01},
	{0x1000, 0x03},
	{0x5a08, 0x84},
};


static int ov9282_probe(struct i2c_client *client)
{
	struct ov9282 *ov9282;
	int ret;

	ov9282 = devm_kzalloc(&client->dev, sizeof(*ov9282), GFP_KERNEL);
	if (!ov9282)
		return -ENOMEM;

	ov9282->dev = &client->dev;

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&ov9282->sd, client, &ov9282_subdev_ops);

	ret = ov9282_parse_hw_config(ov9282);
	if (ret) {
		dev_err(ov9282->dev, "HW configuration is not supported");
		return ret;
	}

	mutex_init(&ov9282->mutex);

	ret = ov9282_power_on(ov9282->dev);
	if (ret) {
		dev_err(ov9282->dev, "failed to power-on the sensor");
		goto error_mutex_destroy;
	}

	/* Check module identity */
	ret = ov9282_detect(ov9282);
	if (ret) {
		dev_err(ov9282->dev, "failed to find sensor: %d", ret);
		goto error_power_off;
	}

	/* Set default mode to max resolution */
	ov9282->cur_mode = &supported_mode;
	ov9282->vblank = ov9282->cur_mode->vblank;

	ret = ov9282_init_controls(ov9282);
	if (ret) {
		dev_err(ov9282->dev, "failed to init controls: %d", ret);
		goto error_power_off;
	}

	/* Initialize subdev */
	ov9282->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov9282->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	ov9282->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&ov9282->sd.entity, 1, &ov9282->pad);
	if (ret) {
		dev_err(ov9282->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&ov9282->sd);
	if (ret < 0) {
		dev_err(ov9282->dev,
			"failed to register async subdev: %d", ret);
		goto error_media_entity;
	}

	pm_runtime_set_active(ov9282->dev);
	pm_runtime_enable(ov9282->dev);
	pm_runtime_idle(ov9282->dev);

	return 0;

error_media_entity:
	media_entity_cleanup(&ov9282->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(ov9282->sd.ctrl_handler);
error_power_off:
	ov9282_power_off(ov9282->dev);
error_mutex_destroy:
	mutex_destroy(&ov9282->mutex);

	return ret;
}

/**
 * ov9282_remove() - I2C client device unbinding
 * @client: pointer to I2C client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int ov9282_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9282 *ov9282 = to_ov9282(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		ov9282_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&ov9282->mutex);

	return 0;
}

static const struct dev_pm_ops ov9282_pm_ops = {
	SET_RUNTIME_PM_OPS(ov9282_power_off, ov9282_power_on, NULL)
};

static const struct of_device_id ov9282_of_match[] = {
	{ .compatible = "ovti,ov9282" },
	{ }
};

MODULE_DEVICE_TABLE(of, ov9282_of_match);

static struct i2c_driver ov9282_driver = {
	.probe_new = ov9282_probe,
	.remove = ov9282_remove,
	.driver = {
		.name = "ov9282",
		.pm = &ov9282_pm_ops,
		.of_match_table = ov9282_of_match,
	},
};

module_i2c_driver(ov9282_driver);

MODULE_DESCRIPTION("OmniVision ov9282 sensor driver");
MODULE_LICENSE("GPL");
