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

/* Declare Function Prototypes - Missing Library Operations */
ktime_t ktime_get(void);
ktime_t ktime_add_ns(const ktime_t kt, u64 nsec);
unsigned long ktime_divns(const ktime_t kt, s64 div);
unsigned long hrtimer_forward(struct hrtimer *timer, ktime_t now, ktime_t interval);

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

/* Missing Library Operation Definitions */

/**
 * ktime_get - get the monotonic time in ktime_t format
 *
 * returns the time in ktime_t format
 */
ktime_t
ktime_get (void) {
	struct timespec now;
	ktime_get_ts(&now);
	return timespec_to_ktime(now);
}

/**
 * ktime_add_ns - Add a scalar nanoseconds value to a ktime_t variable
 * @kt:         addend
 * @nsec:       the scalar nsec value to add
 *
 * Returns the sum of kt and nsec in ktime_t format
 */
ktime_t
ktime_add_ns(const ktime_t kt, u64 nsec)
{
	ktime_t tmp;

	if (likely(nsec < NSEC_PER_SEC)) {
		tmp.tv64 = nsec;
	} else {
		unsigned long rem = do_div(nsec, NSEC_PER_SEC);

		tmp = ktime_set((long)nsec, rem);
	}

	return ktime_add(kt, tmp);
}

/*
 * Divide a ktime value by a nanosecond value
 */
unsigned long
ktime_divns(const ktime_t kt, s64 div)
{
	u64 dclc, inc, dns;
	int sft = 0;

	dclc = dns = ktime_to_ns(kt);
	inc = div;
	/* Make sure the divisor is less than 2^32: */
	while (div >> 32) {
		sft++;
		div >>= 1;
	}
	dclc >>= sft;
	do_div(dclc, (unsigned long) div);

	return (unsigned long) dclc;
}

/**
 * hrtimer_forward - forward the timer expiry
 * @timer:	hrtimer to forward
 * @now:	forward past this time
 * @interval:	the interval to forward
 *
 * Forward the timer expiry so it will expire in the future.
 * Returns the number of overruns.
 */
unsigned long
hrtimer_forward(struct hrtimer *timer, ktime_t now, ktime_t interval)
{
	unsigned long orun = 1;
	ktime_t delta;

	delta = ktime_sub(now, timer->expires);

	if (delta.tv64 < 0)
		return 0;

	if (interval.tv64 < timer->base->resolution.tv64)
		interval.tv64 = timer->base->resolution.tv64;

	if (unlikely(delta.tv64 >= interval.tv64)) {
		s64 incr = ktime_to_ns(interval);

		orun = ktime_divns(delta, incr);
		timer->expires = ktime_add_ns(timer->expires, incr * orun);
		if (timer->expires.tv64 > now.tv64)
			return orun;
		/*
		 * This (and the ktime_add() below) is the
		 * correction for exact:
		 */
		orun++;
	}
	timer->expires = ktime_add(timer->expires, interval);
	/*
	 * Make sure, that the result did not wrap with a very large
	 * interval.
	 */
	if (timer->expires.tv64 < 0)
		timer->expires = ktime_set(KTIME_SEC_MAX, 0);

	return orun;
}

/* Auxiliary Function Definition Section */

//enum hrtimer_restart 
//example_callback (struct hrtimer *timer)
//{
//	ktime_t new_value = ktime_set (5, 0), time_delta;
//	printk(KERN_INFO "Callback activated. The expiration value saved as %d seconds & %d nanoseconds \n", timer->expires.tv.sec, timer->expires.tv.nsec);
//	time_delta = ktime_add(timer->expires, new_value);
//	printk(KERN_INFO "The updated value will be %d seconds & %d nanoseconds \n", time_delta.tv.sec, time_delta.tv.nsec);
//	timer->expires = ktime_add(timer->expires, new_value);
//	return HRTIMER_RESTART;
//}

enum hrtimer_restart
example_callback (struct hrtimer *timer)
{
	ktime_t currtime, interval;
	currtime = ktime_get();
	interval = ktime_set(5, 0);
	printk(KERN_INFO "Callback activated. The expiration value saved as %d seconds & %d nanoseconds \n", timer->expires.tv.sec, timer->expires.tv.nsec);
	printk(KERN_INFO "The current time when entering this function is save as %d seconds & %d nanoseconds \n", currtime.tv.sec, currtime.tv.nsec);
	hrtimer_forward(timer, currtime, interval);
	printk(KERN_INFO "The updated value will be %d seconds & %d nanoseconds \n", timer->expires.tv.sec, timer->expires.tv.nsec);
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
