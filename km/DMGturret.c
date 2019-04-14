#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */ 
#include <linux/time.h> /* timespec struct */
#include <linux/hrtimer.h> /* high res timer */
#include <linux/ktime.h> /* ktime structure */
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
enum hrtimer_restart example_callback (struct hrtimer *timer);

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

/* High Resolution Timers for PWM */
static struct hrtimer hr_timer;

enum hrtimer_restart 
example_callback (struct hrtimer *timer)
{
	ktime_t new_value = ktime_set (5, 0), time_delta;
	printk(KERN_INFO "Callback activated. The expiration value saved as %d seconds & %d nanoseconds \n", timer->expires.tv.sec, timer->expires.tv.nsec);
	time_delta = ktime_add(timer->expires, new_value);
	printk(KERN_INFO "The updated value will be %d seconds & %d nanoseconds \n", time_delta.tv.sec, time_delta.tv.nsec);
	timer->expires = ktime_add(timer->expires, new_value);
	return HRTIMER_RESTART;
}

static int
DMGturret_init(void)
{
	ktime_t expires;
	int result;

	printk(KERN_INFO "Installing module...\n");

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

	/* Initialize High Resolution Timer */
	expires = ktime_set (5, 0);
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &example_callback;
	hrtimer_start(&hr_timer, expires, HRTIMER_MODE_REL);

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
	int ret;

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
	
	/* Cancel High Resolution Timer */
	ret = hrtimer_cancel( &hr_timer);
	if (ret) printk(KERN_INFO "The timer was running when shut down.\n");
	printk(KERN_INFO "...module removed!\n");
}
