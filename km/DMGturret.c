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
#define PWM_PERIOD 20*1E3L /* 20000 US */
#define PAN_SERVO 29

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
static bool parse_uint(const char *buf, uint32_t* num);
static bool set_pulse_width(int index);
static bool PWM_PULSE_ON(uint8_t servo);
static bool PWM_PULSE_OFF(uint8_t servo);

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

/* Holds Pan Servo Pulse Width & Period */
static ktime_t pan_servo_pulse;
static ktime_t pan_servo_period;

/* Holds Universal PWM Period */
static ktime_t pwm_period;

/* Holds the Pulse Width Values */
static ktime_t PWM_STATES[3];

/* Holds the debug counter (SIMULATION ONLY)*/
#ifdef SIM_MODE
static uint32_t debug_counter = 0;
#endif

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
enum hrtimer_restart
pan_pwm_callback (struct hrtimer *timer)
{
	ktime_t currtime;
	currtime = ktime_get();
	if (pan_servo_state) {
		hrtimer_forward(timer, currtime, pan_servo_period);
	} else {
#ifdef SIM_MODE
		debug_counter++;
#endif
		pan_servo_period = ktime_sub(pwm_period, pan_servo_pulse);
		hrtimer_forward(timer, currtime, pan_servo_pulse);
	}
	pan_servo_state = (pan_servo_state) ? PWM_PULSE_OFF(PAN_SERVO) : PWM_PULSE_ON(PAN_SERVO);
#ifdef SIM_MODE
	if (debug_counter == 50) {
		printk(KERN_INFO "One second of cycles reached. Current expiration: %d s:%d ns\n", timer->expires.tv.sec, timer->expires.tv.nsec);
		debug_counter = 1;
	}
#endif
	return HRTIMER_RESTART;
}

static bool
parse_uint(const char* buf, uint32_t* num)
{
	char* endptr;
	if (num == NULL)
		return false;
	*num = simple_strtol(buf, &endptr, 10);
	if (endptr != buf)
		return true;
	return false;
}

static bool
set_pulse_width(int index)
{
	if (index < 0 || index > 2)
	{
		return false;
	}
	pan_servo_pulse = ktime_set(PWM_STATES[index].tv.sec, PWM_STATES[index].tv.nsec);
	return true;
}

static bool
PWM_PULSE_ON(uint8_t servo)
{ 
#ifndef SIM_MODE
	pxa_gpio_set_value(servo, 1);
#endif
	return true;
}

static bool
PWM_PULSE_OFF(uint8_t servo)
{
#ifndef SIM_MODE
	pxa_gpio_set_value(servo, 0);
#endif
	return false;
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

	result = gpio_request(PAN_SERVO, "PAN_SERVO")
                || gpio_direction_output(PAN_SERVO, 0);
	if (result != 0)
	{
		goto fail;
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

	/* Initialzie the Universal PWM Period */
	pwm_period = ktime_set(0, US_TO_NS(PWM_PERIOD));

	/* Initialize the Pulse Width States */
	PWM_STATES[0] = ktime_set(0, US_TO_NS(1000));
	PWM_STATES[1] = ktime_set(0, US_TO_NS(1500));
	PWM_STATES[2] = ktime_set(0, US_TO_NS(2000));

	/* Initialize pan_servo_variables */
	pan_servo_pulse = ktime_set(PWM_STATES[1].tv.sec, PWM_STATES[1].tv.nsec);
	pan_servo_period = ktime_sub(pwm_period, pan_servo_pulse);

	/* Initialize High Resolution Timer */
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
	uint32_t value;
	if (count > WRITE_BUFFER_SIZE)
		count = WRITE_BUFFER_SIZE;
	if (copy_from_user(write_buffer, buf, count))
		return -EINVAL;
	if (count - 1 != 2)
		return -EINVAL;
	if (write_buffer[0] == 'p')
	{
		if (!parse_uint(write_buffer + 1, &value) || !set_pulse_width(value)) {
			return -EINVAL;
		}
	}
	return count;
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

	/* Turn Off & Release GPIO */
	PWM_PULSE_OFF(PAN_SERVO);
	gpio_free(PAN_SERVO);

	printk(KERN_INFO "...module removed!\n");
}
