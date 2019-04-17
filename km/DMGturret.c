#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */ 
#include <asm/uaccess.h> /* copy_from/to_user */
#include <asm/hardware.h>
#include <asm/gpio.h>
#include <linux/interrupt.h>
#include <asm/arch/pxa-regs.h>

MODULE_LICENSE("Dual BSD/GPL");

#define WRITE_BUFFER_SIZE (64)
#define READ_BUFFER_SIZE (128)
#define DEV_NAME "DMGturret"
#define PWM_PERIOD 20000 /* 20 ms in us */
#define PAN_SERVO 28
#define TILT_SERVO 31

/* Declare Function Prototypes - Module File Operations */
static int DMGturret_init(void);
static int DMGturret_open(struct inode *inode, struct file *filp);
static ssize_t DMGturret_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t DMGturret_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos); 
static int DMGturret_release(struct inode *inode, struct file *filp);
static void DMGturret_exit(void);

/* Declare Function Prototypes - Auxiliary Operations */
static bool PWM_PULSE_ON(uint8_t servo);
static bool PWM_PULSE_OFF(uint8_t servo);
static irqreturn_t handle_ost(int irq, void *dev_id);
static bool parse_uint(const char *buf, uint32_t* num);
static bool set_pulse_width(int index, char servo);

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

/* Holds Servo State */
static bool pan_servo_state = false;
static bool tilt_servo_state = false;

/* Holds Servo Pulse Width */
static uint32_t pan_servo_pulse;
static uint32_t tilt_servo_pulse;

/* Holds PWM timer remaining time */
static uint32_t pwm_pulse_remain = 0;
static uint32_t pwm_period_remain = PWM_PERIOD;

/* Holds the Pulse Width  */
static uint32_t PULSE_LENGTH[3];

/* Holds PWM State */
typedef enum {
	PWM_STATE_ALL_OFF,
	PWM_STATE_EQUAL_ON,
	PWM_STATE_PAN_TO_TILT,
	PWM_STATE_TILT_ONLY,
	PWM_STATE_TILT_TO_PAN,
	PWM_STATE_PAN_ONLY
} pwm_state;
static pwm_state CURRENT_PWM_STATE = PWM_STATE_ALL_OFF;

/* Holds the debug counter (SIMULATION ONLY)*/
#ifdef SIM_MODE
static uint32_t debug_counter = 0;
#endif

/* Auxiliary Function Definitions */
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

static irqreturn_t
handle_ost(int irq, void *dev_id)
{
	/* All OS timers 4-11 are handled here. Check which one ticked. */
	if (!(OSSR & 1<<4))
	{
		return IRQ_NONE;
	}
	
	/* Handle PWM Signals */
	switch (CURRENT_PWM_STATE) {
		case PWM_STATE_ALL_OFF: 
#ifdef SIM_MODE
			debug_counter++;
#endif
			pwm_pulse_remain = (pan_servo_pulse < tilt_servo_pulse) ? tilt_servo_pulse - pan_servo_pulse : pan_servo_pulse - tilt_servo_pulse;
			pwm_period_remain = (pan_servo_pulse < tilt_servo_pulse) ? PWM_PERIOD - tilt_servo_pulse : PWM_PERIOD - pan_servo_pulse;
			CURRENT_PWM_STATE = (pan_servo_pulse < tilt_servo_pulse) ? PWM_STATE_PAN_TO_TILT :
				    (pan_servo_pulse > tilt_servo_pulse) ? PWM_STATE_TILT_TO_PAN : PWM_STATE_EQUAL_ON;
			pan_servo_state = PWM_PULSE_ON(PAN_SERVO);
			tilt_servo_state = PWM_PULSE_ON(TILT_SERVO);
			OSMR4 = (pan_servo_pulse < tilt_servo_pulse) ? pan_servo_pulse : tilt_servo_pulse;
			break;
		case PWM_STATE_EQUAL_ON: 
			pan_servo_state = PWM_PULSE_OFF(PAN_SERVO);
			tilt_servo_state = PWM_PULSE_OFF(TILT_SERVO);
			CURRENT_PWM_STATE = PWM_STATE_ALL_OFF;
			OSMR4 = pwm_period_remain;
			break;
		case PWM_STATE_PAN_TO_TILT:
			pan_servo_state = PWM_PULSE_OFF(PAN_SERVO);
			CURRENT_PWM_STATE = PWM_STATE_TILT_ONLY;
			OSMR4 = pwm_pulse_remain;
			break;
		case PWM_STATE_TILT_ONLY:
			tilt_servo_state = PWM_PULSE_OFF(TILT_SERVO);
			CURRENT_PWM_STATE = PWM_STATE_ALL_OFF;
			OSMR4 = pwm_period_remain;
			break;
		case PWM_STATE_TILT_TO_PAN:
			tilt_servo_state = PWM_PULSE_OFF(TILT_SERVO);
			CURRENT_PWM_STATE = PWM_STATE_PAN_ONLY;
			OSMR4 = pwm_pulse_remain;
			break;
		case PWM_STATE_PAN_ONLY:
			pan_servo_state = PWM_PULSE_OFF(PAN_SERVO);
			CURRENT_PWM_STATE = PWM_STATE_ALL_OFF;
			OSMR4 = pwm_period_remain;
			break;
	}
#ifdef SIM_MODE
	if (debug_counter == 100) {
		printk(KERN_INFO "Two seconds of cycles; Pan Pulse Width = %u | Tilt Pulse Width = %u\n", pan_servo_pulse, tilt_servo_pulse);
		debug_counter = 1;
	}
#endif
	/* Mark the tick as handled by writing a 1 in this timer's status. */
	OSSR = 1<<4;
	OSCR4 = 0; /* Reset the counter. XXX: This shoudln't be necessary, but it seems to be... */
	return IRQ_HANDLED;
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
set_pulse_width(int index, char servo)
{
	if (index < 0 || index > 2 || (servo != 'p' && servo != 't'))
	{
		return false;
	}
	pan_servo_pulse = (servo == 'p') ? PULSE_LENGTH[index] : pan_servo_pulse;
	tilt_servo_pulse = (servo == 't') ? PULSE_LENGTH[index] : tilt_servo_pulse;
	return true;
}

/* Module File Operation Definitions */ static int DMGturret_init(void)
{
	int result;

	printk(KERN_INFO "Installing module...\n");

	/* Register Device */
	result = register_chrdev(DMGturret_major, DEV_NAME, &DMGturret_fops);
	if (result < 0)
	{
		return result;
	}

	/* Initialize GPIO Settings */
	result = gpio_request(PAN_SERVO, "PAN_SERVO")
		|| gpio_request(TILT_SERVO, "TILT_SERVO")
                || gpio_direction_output(PAN_SERVO, 0)
		|| gpio_direction_output(TILT_SERVO, 0);
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

	/* Initialize PWM Variable Values: */
//	PULSE_LENGTH[0] = 1000; /* 1 ms */
//	PULSE_LENGTH[1] = 7500; /* 7.5 ms */
//	PULSE_LENGTH[2] = 15000; /* 15 ms */
	PULSE_LENGTH[0] = 1000; /* 1 ms */
	PULSE_LENGTH[1] = 1500; /* 1.5 ms */
	PULSE_LENGTH[2] = 2000; /* 2.0 ms */

	pan_servo_pulse = PULSE_LENGTH[1];
	tilt_servo_pulse = PULSE_LENGTH[1];

	/* Initialize OS Timer for Pulse Width Modulation */
	if (request_irq(IRQ_OST_4_11, &handle_ost, 0, DEV_NAME, NULL) != 0) {
		printk("OST irq not acquired \n");
		goto fail;
	}
       	else
	{
                printk("OST irq %d acquired successfully \n", IRQ_OST_4_11);

		OMCR4 = 0xcc;
		/* 1100 1100 .- 001: 1/32768th of a second. This one continues while sleeping.
		 * ^^\/ ^\ / +- 010: 1 ms
		 * |||  | `--+- 011: 1 s
		 * |||  |    +- 100: 1 us
		 * |||  |    `- 101: external control. Others reserved.
		 * |||  `------ Reset counter on match
		 * |||       .- 00: no external synch control
		 * ||`-------+- 01-10: select which external control.
		 * ||        `- 11: reserved
		 * |`---------- Continue counting on a match
		 * `----------- A write to OSCR4 starts the counter
		 */
		OSMR4 = PWM_PERIOD; /* Number of ticks before the IRQ is triggered
		                 * For 1us clock, 500k => 0.5 seconds.
		                 */
		OIER |= 1 << 4; /* XXX: fix the hack if we can... should use a macro if available... Only saw for counters OIER_E0 - 3 */
		OSCR4 = 0; /* Initialize the counter value (and start the counter) */
	}

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
	if (write_buffer[0] == 'p' || write_buffer[0] == 't')
	{
		if (!parse_uint(write_buffer + 1, &value) || !set_pulse_width(value, write_buffer[0])) {
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
	
	/* Turn Off & Release GPIO */
	PWM_PULSE_OFF(PAN_SERVO);
	PWM_PULSE_OFF(TILT_SERVO);
	gpio_free(PAN_SERVO);
	gpio_free(TILT_SERVO);

	/* Release OS Timer */
	OIER &= ~(1<<4);
	free_irq(IRQ_OST_4_11, NULL);

	printk(KERN_INFO "...module removed!\n");
}
