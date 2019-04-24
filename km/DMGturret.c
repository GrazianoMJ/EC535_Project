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
#define PWM_PERIOD 20000 /* 20 ms in us */
#define STEP_MOTOR_DRIVE 16
#define STEP_MOTOR_DIRECTION 113
#define STEP_MOTOR_ENABLE 9  
#define STEP_MOTOR_FEEDBACK 28
#define PAN_SERVO 29
#define TILT_SERVO 30
#define SOLENOID_ENABLE 31
#define OIER_E4 (1 << 4) /* pxa-regs.h only gives us OIER_E0 - 3 */

/* Declare Function Prototypes - Module File Operations */
static int DMGturret_init(void);
static int DMGturret_open(struct inode *inode, struct file *filp);
static ssize_t DMGturret_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t DMGturret_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos); 
static int DMGturret_release(struct inode *inode, struct file *filp);
static void DMGturret_exit(void);

/* Declare Function Prototypes - Auxiliary Operations */
static int step_motor_pwm_setup(unsigned gpio);
static void step_motor_release(unsigned gpio);
static bool GPIO_OUTPUT_ON(uint8_t servo);
static bool GPIO_OUTPUT_OFF(uint8_t servo);
static irqreturn_t handle_ost(int irq, void *dev_id);
static bool parse_uint(const char *buf, uint32_t* num);
static bool set_pulse_width(uint32_t width, char servo);
static void hardware_timer_callback(unsigned long data);
static irqreturn_t turret_prime_stop(int irq, void *dev_id);

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

/* Holds External Hardware State */
static bool pan_servo_state = false;
static bool tilt_servo_state = false;
static bool step_motor_state = false;
static bool solenoid_state = false;

/* Holds Servo Pulse Width */
static uint32_t pan_servo_pulse;
static uint32_t tilt_servo_pulse;

/* Holds PWM timer remaining time */
static uint32_t pwm_pulse_remain = 0;
static uint32_t pwm_period_remain = PWM_PERIOD;

/* Holds the timer for the solenoid & stepper motor */
static struct timer_list hardware_timer;

/* Holds the Pulse Width */
#define PULSE_COUNT 10
#define DEFAULT_PULSE_INDEX (PULSE_COUNT / 2)
#define MIN_PAN_PULSE (1000u) /* 1 ms */
#define MAX_PAN_PULSE (2000u) /* 2 ms */
#define MIN_TILT_PULSE (1300u) /* 1.3 ms */
#define MAX_TILT_PULSE (1600u) /* 1.6 ms */
#define PAN_PULSE_GRANULARITY ((MAX_PAN_PULSE - MIN_PAN_PULSE) / PULSE_COUNT)
#define TILT_PULSE_GRANULARITY ((MAX_TILT_PULSE - MIN_TILT_PULSE) / PULSE_COUNT)
#define PAN_PULSE_LENGTH(index) ((index) * PAN_PULSE_GRANULARITY + MIN_PAN_PULSE)
#define TILT_PULSE_LENGTH(index) ((index) * TILT_PULSE_GRANULARITY + MIN_TILT_PULSE)

/* Holds PWM State */
typedef enum {
	PWM_STATE_ALL_OFF,
	PWM_STATE_EQUAL_ON,
	PWM_STATE_PAN_TO_TILT,
	PWM_STATE_TILT_ONLY,
	PWM_STATE_TILT_TO_PAN,
	PWM_STATE_PAN_ONLY
} pwm_state;
static pwm_state current_pwm_state = PWM_STATE_ALL_OFF;

/* Holds Turret Firing State */
typedef enum {
	TURRET_STANDBY,
	TURRET_PRIMING,
	TURRET_READY,
	TURRET_FIRING
} turret_state;
static turret_state current_turret_state = TURRET_STANDBY;

/* Holds the debug counter (SIMULATION ONLY)*/
#ifdef SIM_MODE
static uint32_t debug_counter = 0;
#endif

/* Auxiliary Function Definitions */
static int
step_motor_pwm_setup(unsigned gpio)
{
	// Check gpio to verify if it is PWM compatible
	// Set up PWM if valid
	if (gpio == GPIO16_PWM0) {
		CKEN |= CKEN0_PWM0;
		pxa_gpio_mode(GPIO16_PWM0_MD);
		PWM_CTRL0 |= 0x3f;
		PWM_PERVAL0 |= 0x3ff;
		PWM_PWDUTY0 |= 0x1ff;
	} else if (gpio == GPIO17_PWM1) {
		CKEN |= CKEN1_PWM1;
		pxa_gpio_mode(GPIO17_PWM1_MD);
		PWM_CTRL1 |= 0x3f;
	        PWM_PERVAL1 |= 0x3ff;
		PWM_PWDUTY1 |= 0x1ff;
	} else {
		return -EINVAL;
	}

	return 0;
}

static void
step_motor_release(unsigned gpio)
{
	// Check gpio to verify it is PWM compatible
	// Set PWM_PWDUTY to zero so output is disabled
	if (gpio == GPIO16_PWM0)
		PWM_PWDUTY0 = 0;
	else if (gpio == GPIO17_PWM1)
		PWM_PWDUTY1 = 0;
}

static bool
GPIO_OUTPUT_ON(uint8_t servo)
{ 
#ifndef SIM_MODE
	pxa_gpio_set_value(servo, 1);
#endif
	return true;
}

static bool
GPIO_OUTPUT_OFF(uint8_t servo)
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
	if (!(OSSR & OIER_E4))
	{
		return IRQ_NONE;
	}
	
	/*
	 * Handle PWM Signals
	 * Align them to start at the same time. Trigger each followup OST tick
	 * to happen when the next-soonest PWM falling edge is until none remain.
	 * Then schedule the next rising edge.
	 */
	switch (current_pwm_state) {
		case PWM_STATE_ALL_OFF: 
#ifdef SIM_MODE
			debug_counter++;
#endif
			pwm_pulse_remain = (pan_servo_pulse < tilt_servo_pulse) ? tilt_servo_pulse - pan_servo_pulse : pan_servo_pulse - tilt_servo_pulse;
			pwm_period_remain = (pan_servo_pulse < tilt_servo_pulse) ? PWM_PERIOD - tilt_servo_pulse : PWM_PERIOD - pan_servo_pulse;
			current_pwm_state = (pan_servo_pulse < tilt_servo_pulse) ? PWM_STATE_PAN_TO_TILT :
				    (pan_servo_pulse > tilt_servo_pulse) ? PWM_STATE_TILT_TO_PAN : PWM_STATE_EQUAL_ON;
			pan_servo_state = GPIO_OUTPUT_ON(PAN_SERVO);
			tilt_servo_state = GPIO_OUTPUT_ON(TILT_SERVO);
			OSMR4 = (pan_servo_pulse < tilt_servo_pulse) ? pan_servo_pulse : tilt_servo_pulse;
			break;
		case PWM_STATE_EQUAL_ON: 
			pan_servo_state = GPIO_OUTPUT_OFF(PAN_SERVO);
			tilt_servo_state = GPIO_OUTPUT_OFF(TILT_SERVO);
			current_pwm_state = PWM_STATE_ALL_OFF;
			OSMR4 = pwm_period_remain;
			break;
		case PWM_STATE_PAN_TO_TILT:
			pan_servo_state = GPIO_OUTPUT_OFF(PAN_SERVO);
			current_pwm_state = PWM_STATE_TILT_ONLY;
			OSMR4 = pwm_pulse_remain;
			break;
		case PWM_STATE_TILT_ONLY:
			tilt_servo_state = GPIO_OUTPUT_OFF(TILT_SERVO);
			current_pwm_state = PWM_STATE_ALL_OFF;
			OSMR4 = pwm_period_remain;
			break;
		case PWM_STATE_TILT_TO_PAN:
			tilt_servo_state = GPIO_OUTPUT_OFF(TILT_SERVO);
			current_pwm_state = PWM_STATE_PAN_ONLY;
			OSMR4 = pwm_pulse_remain;
			break;
		case PWM_STATE_PAN_ONLY:
			pan_servo_state = GPIO_OUTPUT_OFF(PAN_SERVO);
			current_pwm_state = PWM_STATE_ALL_OFF;
			OSMR4 = pwm_period_remain;
			break;
	}
#ifdef SIM_MODE
	if (debug_counter == 1000) {
		printk(KERN_INFO "Twenty seconds of cycles; Pan Pulse Width = %u | Tilt Pulse Width = %u\n", pan_servo_pulse, tilt_servo_pulse);
		debug_counter = 1;
	}
#endif
	/* Mark the tick as handled by writing a 1 in this timer's status. */
	OSSR = OIER_E4;
	OSCR4 = 0; /* Reset the counter. */
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
set_pulse_width(uint32_t width, char servo)
{
	if (servo == 'p' && width >= MIN_PAN_PULSE && width <= MAX_PAN_PULSE)
	{
		width = min(MAX_PAN_PULSE, max(MIN_PAN_PULSE, width));
		pan_servo_pulse = width;
	}
	else if (servo == 't' && width >= MIN_TILT_PULSE && width <= MAX_TILT_PULSE)
	{
		width = min(MAX_TILT_PULSE, max(MIN_TILT_PULSE, width));
		tilt_servo_pulse = width;
	}
	else
	{
		return false;
	}

#ifdef SIM_MODE
	printk(KERN_INFO "Set %c: %d\n", servo, width);
#endif

	return true;
}

static void
hardware_timer_callback(unsigned long data)
{
	if (current_turret_state == TURRET_FIRING) {
#ifdef SIM_MODE
		printk(KERN_INFO "...solenoid now off after 2 seconds\n");
#endif
		solenoid_state = GPIO_OUTPUT_OFF(SOLENOID_ENABLE);
		current_turret_state = TURRET_STANDBY;
	}
	else if (current_turret_state == TURRET_PRIMING) {
#ifdef SIM_MODE
		printk(KERN_INFO "...stepper motor now off after 10 seconds\n");
#endif
		step_motor_state = !(GPIO_OUTPUT_ON(STEP_MOTOR_ENABLE));
		current_turret_state = TURRET_READY;
	}
}

static irqreturn_t
turret_prime_stop(int irq, void *dev_id)
{
	if (current_turret_state == TURRET_PRIMING) {
		step_motor_state = !(GPIO_OUTPUT_ON(STEP_MOTOR_ENABLE));
		current_turret_state = TURRET_READY;
	}
	return IRQ_HANDLED;
}
		
/* Module File Operation Definitions */
static int DMGturret_init(void)
{
	int result;
	int feedback_irq;

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
		|| gpio_request(STEP_MOTOR_DRIVE, "STEP_MOTOR_DRIVE")
		|| gpio_request(STEP_MOTOR_DIRECTION, "STEP_MOTOR_DIRECTION")
		|| gpio_request(STEP_MOTOR_ENABLE, "STEP_MOTOR_ENABLE")
		|| gpio_request(STEP_MOTOR_FEEDBACK, "STEP_MOTOR_FEEDBACK")
		|| gpio_request(SOLENOID_ENABLE, "SOLENOID_ENABLE")
                || gpio_direction_output(PAN_SERVO, 0)
		|| gpio_direction_output(TILT_SERVO, 0)
		|| gpio_direction_output(STEP_MOTOR_ENABLE, 1) /* Enable is assert low to activate */
		|| gpio_direction_output(STEP_MOTOR_DIRECTION, 0)
		|| gpio_direction_input(STEP_MOTOR_FEEDBACK)
		|| step_motor_pwm_setup(STEP_MOTOR_DRIVE)
		|| gpio_direction_output(SOLENOID_ENABLE, 0);
	if (result != 0)
	{
		goto fail;
	}
	step_motor_state = !(GPIO_OUTPUT_ON(STEP_MOTOR_ENABLE)); /*XXX: Should be taken care of with gpio_direction_output...*/
	feedback_irq = IRQ_GPIO(STEP_MOTOR_FEEDBACK);
	if (request_irq(feedback_irq, &turret_prime_stop, SA_INTERRUPT | SA_TRIGGER_RISING, DEV_NAME, NULL) != 0) {
		printk("Feedback irq not acquired \n");
		goto fail;
	}
	else
	{
		printk("Feedback irq %d acquired successfully \n", feedback_irq);
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

	/* Center the servos */
	pan_servo_pulse = PAN_PULSE_LENGTH(DEFAULT_PULSE_INDEX);
	tilt_servo_pulse = TILT_PULSE_LENGTH(DEFAULT_PULSE_INDEX);

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
		OIER |= OIER_E4;
		OSCR4 = 0; /* Initialize the counter value (and start the counter) */
	}

	/* Setup hardware timer */
	setup_timer(&hardware_timer, hardware_timer_callback, 0); 

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
	uint32_t value; /* First argument for any command */
	bool success = true;
	if (count > WRITE_BUFFER_SIZE)
		count = WRITE_BUFFER_SIZE;
	if (copy_from_user(write_buffer, buf, count))
		return -EINVAL;
	if (count - 1 != 2)
		return -EINVAL;
	if (write_buffer[0] == 'L' || write_buffer[0] == 'R' ||
	    write_buffer[0] == 'U' || write_buffer[0] == 'D' ||
	    write_buffer[0] == 'F' || write_buffer[0] == 'P')
	{
		if (!parse_uint(write_buffer + 1, &value))
		{
			return -EINVAL;
		}

		switch (write_buffer[0])
		{
		case 'F':
			if (!solenoid_state && current_turret_state == TURRET_READY) {
#ifdef SIM_MODE
				printk(KERN_INFO "Solenoid Activated...\n");			
#endif
				solenoid_state = GPIO_OUTPUT_ON(SOLENOID_ENABLE);
				mod_timer(&hardware_timer, jiffies + msecs_to_jiffies(2000));
				current_turret_state = TURRET_FIRING;
				success = true;
			}
			break;
		case 'P':
			if (current_turret_state == TURRET_STANDBY) {
#ifdef SIM_MODE
				printk(KERN_INFO "Stepper Motor Activated...\n");
#endif
				step_motor_state = !(GPIO_OUTPUT_OFF(STEP_MOTOR_ENABLE));
				mod_timer(&hardware_timer, jiffies + msecs_to_jiffies(10000)); /* Added for safety */
				current_turret_state = TURRET_PRIMING;
				success = true;
			}
			break;
		case 'D':
			success = set_pulse_width(tilt_servo_pulse - value * TILT_PULSE_GRANULARITY, 't');
			break;
		case 'U':
			success = set_pulse_width(tilt_servo_pulse + value * TILT_PULSE_GRANULARITY, 't');
			break;
		case 'L':
			success = set_pulse_width(pan_servo_pulse - value * PAN_PULSE_GRANULARITY, 'p');
			break;
		case 'R':
			success = set_pulse_width(pan_servo_pulse + value * PAN_PULSE_GRANULARITY, 'p');
			break;
		}
		if (!success)
		{
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
	GPIO_OUTPUT_OFF(PAN_SERVO);
	GPIO_OUTPUT_OFF(TILT_SERVO);
	step_motor_release(STEP_MOTOR_DRIVE);
	GPIO_OUTPUT_OFF(STEP_MOTOR_DIRECTION);
	GPIO_OUTPUT_OFF(STEP_MOTOR_ENABLE);
	GPIO_OUTPUT_OFF(SOLENOID_ENABLE);
	free_irq(IRQ_GPIO(STEP_MOTOR_FEEDBACK), NULL);
	gpio_free(PAN_SERVO);
	gpio_free(TILT_SERVO);
	gpio_free(STEP_MOTOR_DRIVE);
	gpio_free(STEP_MOTOR_DIRECTION);
	gpio_free(STEP_MOTOR_ENABLE);
	gpio_free(STEP_MOTOR_FEEDBACK);
	gpio_free(SOLENOID_ENABLE);

	/* Release OS Timer */
	OIER &= ~OIER_E4;
	free_irq(IRQ_OST_4_11, NULL);
	
	/* Release hardware timer */
	del_timer(&hardware_timer);

	printk(KERN_INFO "...module removed!\n");
}
