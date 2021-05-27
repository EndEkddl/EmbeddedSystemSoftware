#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/gpio.h>
#include <mach/gpio.h>


#define STOPWATCH_DEVICE "/dev/stopwatch"
#define MAJOR_NUM 242
#define FND_ADDR 0x08000004
#define SUCCESS 0
#define BLINK_DELAY HZ/10;
#define HOME IMX_GPIO_NR(1,11)
#define BACK IMX_GPIO_NR(1,12)
#define VOLP IMX_GPIO_NR(2,15)
#define VOLM IMX_GPIO_NR(5,14)

struct timer_list timer, termination;
static unsigned char* fnd_addr;
static unsigned int fnd[4];
static int state, end, realEnd;
static int firstPush = 1;
static int Device_Open;
static int oneSecond;
unsigned long curr, prev;

void fpga_fnd_write(unsigned int arr[4]);
static void my_timer_func(unsigned long);
irqreturn_t home_handler(int irq, void* dev_id, struct pt_regs* regs);
irqreturn_t back_handler(int irq, void* dev_id, struct pt_regs* regs);
irqreturn_t volP_handler(int irq, void* dev_id, struct pt_regs* regs);
irqreturn_t volM_handler(int irq, void* dev_id, struct pt_regs* regs);
static int stopwatch_device_open(struct inode* inode, struct file* file);
static int stopwatch_device_release(struct inode* inode, struct file* file);
static int stopwatch_device_write(struct file* file, const char* buf, size_t cnt, loff_t* f_pos);
static int stopwatch_register(void);

wait_queue_head_t wq;
DECLARE_WAIT_QUEUE_HEAD(wq);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = stopwatch_device_open,
	.write = stopwatch_device_write,
	.release = stopwatch_device_release,
};

void fpga_fnd_write(unsigned int arr[4]) {

	unsigned int res = 0;
	res = (arr[3] << 12) | (arr[2] << 8) | (arr[1] << 4) | arr[0];
	outw(res, (unsigned int)fnd_addr);

	return;
}
static void my_timer_func(unsigned long timeout) {

	if (end) return;
	if (oneSecond == 10) {

		oneSecond = 0;
		fnd[0]++;
		if (fnd[0] == 10) {
			fnd[0] = 0; 
			fnd[1]++;
		}
		if (fnd[1] == 6) {
			fnd[1] = 0;
			fnd[2]++;
		}
		if (fnd[2] == 10) {
			fnd[2] = 0;
			fnd[3]++;
		}
		fpga_fnd_write(fnd);
	}

	timer.expires = get_jiffies_64() + BLINK_DELAY;
	timer.data = (unsigned long)&timer;
	timer.function = my_timer_func;

	add_timer(&timer);
	oneSecond++;

	return;
}

irqreturn_t home_handler(int irq, void* dev_id, struct pt_regs* regs) {

	printk("INT 1(Home button / start) = %d!\n", gpio_get_value(HOME));

	if (!state) {
		printk("start timer!\n");
		end = 0;
		state = 1;
		timer.expires = jiffies + BLINK_DELAY;
		timer.function = my_timer_func;
		add_timer(&timer);
	}

	return IRQ_HANDLED;
}

irqreturn_t back_handler(int irq, void* dev_id, struct pt_regs* regs) {

	printk("INT 2(Back button / pause) = %d!\n", gpio_get_value(BACK));
	end = 1;
	state = 0;
	return IRQ_HANDLED;
}

irqreturn_t volP_handler(int irq, void* dev_id, struct pt_regs* regs) {
	
	int i;

	printk("INT 3(VOL+ button / reset) = %d!\n", gpio_get_value(VOLP));

	end = 1;
	state = 0;
	for (i = 0; i < 4; i++) fnd[i] = 0;
	fpga_fnd_write(fnd);

	return IRQ_HANDLED;
}

irqreturn_t volM_handler(int irq, void* dev_id, struct pt_regs* regs) {
	
	int i;
	unsigned long diff;
	printk("INT 4(VOL- button / stop) = %d!\n", gpio_get_value(VOLM));
	if (firstPush) {
		firstPush = 0;
		prev = get_jiffies_64();
	}
	else {
		firstPush = 1;
		curr = get_jiffies_64();
		diff = curr - prev;
		if (diff >= 29 * BLINK_DELAY) {
			end = realEnd = 1;
		}
		prev = curr;
	}

	if (realEnd) {
		for (i = 0; i < 4; i++) fnd[i] = 0;
		fpga_fnd_write(fnd);
		firstPush = 1;
		realEnd = 0;
		__wake_up(&wq, 1, 1, NULL);
	}
	return IRQ_HANDLED;
}

static int stopwatch_device_open(struct inode* inode, struct file* file) {

	int irq, res;
	init_timer(&timer);
	init_timer(&termination);

	printk(KERN_ALERT "stopwatch_device_open\n");

	//Home INT
	gpio_direction_input(HOME);
	irq = gpio_to_irq(HOME);
	res = request_irq(irq, home_handler, IRQF_TRIGGER_FALLING, "HOME", 0);

	//Back INT
	gpio_direction_input(BACK);
	irq = gpio_to_irq(BACK);
	res = request_irq(irq, home_handler, IRQF_TRIGGER_FALLING, "BACK", 0);

	//VOL+ INT
	gpio_direction_input(VOLP);
	irq = gpio_to_irq(VOLP);
	res = request_irq(irq, home_handler, IRQF_TRIGGER_FALLING, "VOL+", 0);

	//VOL- INT
	gpio_direction_input(VOLM);
	irq = gpio_to_irq(VOLM);
	res = request_irq(irq, home_handler, IRQF_TRIGGER_FALLING, "VOL-", 0);

	Device_Open++;

	return SUCCESS;
}

static int stopwatch_device_release(struct inode* inode, struct file* file) {

	int i;
	for (i = 0; i < 4; i++) fnd[i] = 0;

	printk(KERN_ALERT "stopwatch_device_release\n");
	del_timer_sync(&timer);
	del_tiemr_sync(&termination);

	free_irq(HOME, NULL);
	free_irq(BACK, NULL);
	free_irq(VOLP, NULL);
	free_irq(VOLM, NULL);

	Device_Open--;
	end = realEnd = state = oneSecond = 0;
	fpga_fnd_write(fnd);
	firstPush = 1;
	prev = 1;

	return SUCCESS;
}

static int stopwatch_device_write(struct file* file, const char* buf, size_t cnt, loff_t* f_pos) {
	printk("stopwatch_device_write\n");
	interruptible_sleep_on(&wq);

	return SUCCESS;
}

static int stopwatch_register(void) {
	
	return SUCCESS;
}

static int __init stopwatch_device_init(void) {

	int res = stopwatch_register();
	if (res < 0) {
		printk(KERN_ALERT "Error in register_chrdev: %d\n", res);
		return res;
	}
	printk("stopwatch_device_init!\n");
	printk("I was assigned major number %d, device name %s.\n", MAJOR_NUM, STOPWATCH_DEVICE);
	
	fnd_addr = ioremap(FND_ADDR, 0x4);

	return SUCCESS;
}

static void __exit stopwatch_device_exit(void) {

	iounmap(fnd_addr);

	del_timer_sync(&timer);
	del_timer_sync(&termination);

	int res = unregister_chrdev(MAJOR_NUM, STOPWATCH_DEVICE);
	if (res < 0) {
		printk(KERN_ALERT "Error in unregister_chrdev: %d\n", res);
		return;
	}
	printk("stopwatch_device_exit!\n");
	return;
}
module_init(stopwatch_device_init);
module_exit(stopwatch_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Inhye Kye");