#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/timer.h> /* timer functions */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <asm/hardware.h>
#include <asm/gpio.h>
#include <linux/interrupt.h>
#include <asm/arch/pxa-regs.h>

MODULE_LICENSE("Dual BSD/GPL");

#define WRITE_BUFFER_SIZE (64)
#define READ_BUFFER_SIZE (128)
#define DEV_NAME "DMGturret"

/* Declare Function Prototypes - Module File Operations */
static int DMGturret_init(void);
static int DMGturret_open(struct inode *inode, struct file *filp);
static ssize_t DMGturret_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t DMGturret_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos); 
static int DMGturret_release(struct inode *inode, struct file *filp);
static void DMGturret_exit(void);

/* Declare Function Prototypes - Auxiliary Operations */
static int pwm_setup(unsigned gpio);
static void modify_pwm(unsigned gpio, uint8_t state, bool active);

/* Set File Access Functions */
struct file_operations DMGturret_fops = {
	read: DMGturret_read,
	write: DMGturret_write,
	open: DMGturret_open,
	release: DMGturret_release
};

/* Set Init & Exit Functions */
module_init(DMGturret_init);
module_exit(DMGturret_exit);

/* Declare Global Variables */
/* Major Number */
static int DMGturret_major = 61;

/* Read/Write Storage Buffers */
static char *write_buffer;
static char *read_buffer;

/* Record Current Message Length */
static int write_len;
static int read_len;

static int
pwm_setup(unsigned gpio)
{
	// Check gpio to verify if it is PWM compatible
	// Set up PWM if valid
	if (gpio == GPIO16_PWM0) {
		CKEN |= CKEN0_PWM0;
		pxa_gpio_mode(GPIO16_PWM0_MD);
		PWM_CTRL0 |= 0x3f;
		PWM_PERVAL0 |= 0x3ff;
		modify_pwm(gpio, brightness_state, counter_value & 0x01);
	} else if (gpio == GPIO17_PWM1) {
		CKEN |= CKEN1_PWM1;
		pxa_gpio_mode(GPIO17_PWM1_MD);
		PWM_CTRL1 |= 0x3f;
	        PWM_PERVAL1 |= 0x3ff;
		modify_pwm(gpio, brightness_state, counter_value & 0x01);
	} else {
		return -EINVAL;
	}

	return 0;
}

static void
modify_pwm(unsigned gpio, uint8_t state, bool active)
{
	// Precondition: GPIO 16 or 17 is confirmed and set up.
	if (gpio == GPIO16_PWM0) 
		PWM_PWDUTY0 = (!active)        ? 0              :
			      (state % 3 == 0) ? MAX_BRIGHTNESS :
			      (state % 3 == 1) ? MID_BRIGHTNESS : MIN_BRIGHTNESS;
	else if (gpio == GPIO17_PWM1)
		PWM_PWDUTY1 = (!active)        ? 0              :
		  	      (state % 3 == 0) ? MAX_BRIGHTNESS :
			      (state % 3 == 1) ? MID_BRIGHTNESS : MIN_BRIGHTNESS;
}

static int
DMGturret_init(void)
{
	int result;

	/* Register Device */
	result = register_chrdev(DMGturret_major, DEV_NAME, &DMGturret_fops);
	if (result < 0)
	{
		return result;
	}

	/* Allocate Write Buffer Memory */
	write_buffer = kmalloc(WRITE_BUFFER_SIZE, GFP_KERNEL);
	if (!write_buffer)
	{
		result = -ENOMEM;
		goto fail;
	}
	memset(write_buffer, 0, WRITE_BUFFER_SIZE);
	write_len = 0;

	/* Allocate Read Buffer Memory */
	read_buffer = kmalloc(READ_BUFFER_SIZE, GFP_KERNEL);
	if (!read_buffer)
	{
		result = -ENOMEM;
		goto fail;
	}
	memset(read_buffer, 0, READ_BUFFER_SIZE);
	read_len = 0;

	return 0;

fail:
	DMGturret_exit();
	return result;
}

static int
DMGturret_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t
DMGturret_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

/*
 * Set counter state through the device interface.
 *
 * Write fH, fM, or fL for high, medium, and low frequency timers.
 *
 * Write v0 through vf to set the counter value to 0 through 15.
 */
static ssize_t
DMGturret_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static int
DMGturret_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static void
DMGturret_exit(void)
{
	/* Free Major Number */
	unregister_chrdev(DMGturret_major, DEV_NAME);

	/* Free Read Buffer Memory */
	if (read_buffer)
	{
		kfree(read_buffer);
	}
	
	/* Free Write Buffer Memory */
	if (write_buffer)
	{
		kfree(write_buffer);
	}
}
