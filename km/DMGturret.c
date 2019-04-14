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
#define US_TO_NS(x) (x * 1E3L)

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
enum hrtimer_restart pan_pwm_callback (struct hrtimer *timer);

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
static struct hrtimer pan_pwm;

/* Holds Pan Servo State */
static bool pan_servo_state = false;

/* Holds Pan Servo Pulse Width */
static ktime_t pan_servo_pulse;

/* Holds the Overall Period */
static ktime_t pwm_period;

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

/* Auxiliary Function Definitions */
//enum hrtimer_restart
//pan_pwm_callback (struct hrtimer *timer)
//{
//	ktime_t currtime, interval;
//	currtime = ktime_get();
//	interval = ktime_set(5, 0);
//	printk(KERN_INFO "Callback activated. The expiration value saved as %d seconds & %d nanoseconds \n", timer->expires.tv.sec, timer->expires.tv.nsec);
//	printk(KERN_INFO "The current time when entering this function is save as %d seconds & %d nanoseconds \n", currtime.tv.sec, currtime.tv.nsec);
//	hrtimer_forward(timer, currtime, interval);
//	printk(KERN_INFO "The updated value will be %d seconds & %d nanoseconds \n", timer->expires.tv.sec, timer->expires.tv.nsec);
//	return HRTIMER_RESTART;
//}

enum hrtimer_restart
pan_pwm_callback (struct hrtimer *timer)
{
	ktime_t currtime, interval;
	
	printk(KERN_INFO "Callback activated. The pan servo is currently %s and the expiration value saved as %d seconds & %d nanoseconds \n", ((pan_servo_state) ? "On" : "Off"), timer->expires.tv.sec, timer->expires.tv.nsec);
	currtime = ktime_get();
	if (pan_servo_state) {
		interval = ktime_sub(pwm_period, pan_servo_pulse);
		hrtimer_forward(timer, currtime, interval);
	} else {
		hrtimer_forward(timer, currtime, pan_servo_pulse);
	}
	pan_servo_state = !pan_servo_state;
	printk(KERN_INFO "The pan servo is now set to %s and the next expiration is %d seconds & %d nanoseconds \n", ((pan_servo_state) ? "On" : "Off"), timer->expires.tv.sec, timer->expires.tv.nsec);
	return HRTIMER_RESTART;
}

/* Module File Operation Definitions */
static int
DMGturret_init(void)
{
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
	pan_servo_pulse = ktime_set (2, 0);
	pwm_period = ktime_set (5, 0);
	hrtimer_init(&pan_pwm, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pan_pwm.function = &pan_pwm_callback;
	hrtimer_start(&pan_pwm, pwm_period, HRTIMER_MODE_REL);

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
	ret = hrtimer_cancel(&pan_pwm);
	if (ret) printk(KERN_INFO "The timer was running when shut down.\n");
	printk(KERN_INFO "...module removed!\n");
}
