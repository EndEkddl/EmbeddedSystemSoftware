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
#include <linux/workqueue.h>

#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/gpio.h>

#include <mach/gpio.h>

#define DEVICE_NAME "/dev/BullsAndCows"
#define MAJOR_NUM 242
#define FND_ADDR 0x08000004 // physical addr for FND
#define LED_ADDR 0x08000016 // physical addr for LED
#define DOT_ADDR 0x08000210 // physical addr for DOT
#define LCD_ADDR 0x08000090 // physical addr for LCD
#define SWITCH_ADDR 0x08000050 // physical addr for SWITCH
#define BLINK_DELAY HZ/10 // timer value
#define SUCCESS 0
#define CORRECT 1
#define PLAY 0
#define EXIT -1
#define HOME IMX_GPIO_NR(1,11)
#define VOLP IMX_GPIO_NR(2,15)
#define VOLM IMX_GPIO_NR(5,14)

struct timer_list timer;
struct timer_list termination;

static unsigned char *fnd_addr, *led_addr, *dot_addr, *lcd_addr, *switch_addr;

typedef struct ioctl_arg {
	int state, cnt;
}_arg;
_arg arg;

static const int NUMBER[10] = { 2318, 1593, 7126, 3751, 1234, 5678, 1378, 8629, 9371, 4267 };
static int Device_Open, enable = 0;
static unsigned int fnd[4];
unsigned long curr, prev;
static int firstPush = 1;
static int end, realEnd;
static int number_idx = 0;
int input, ans, cnt, led = 0;
static int fndIdx, strike=0, ball=0;
int fpga_fnd_write(unsigned int arr[4]);
int fpga_led_write(int idx);
int fpga_lcd_write(int idx1, int idx2);
int fpga_lcd_init_write(int idx1, int idx2);
int fpga_lcd_correct_write(int idx1, int idx2);
int fpga_switch_read(void);
void my_timer_func(unsigned long);
void guess_num(void);
static int bullsAndCows_device_open(struct inode*, struct file*);
static int bullsAndCows_device_ioctl(struct file*, unsigned int, unsigned long);
static int bullsAndCows_device_release(struct inode*, struct file*);
static ssize_t bullsAndCows_device_read(struct file*, char*, size_t, loff_t*);

irqreturn_t home_handler(int irq, void* dev_id, struct pt_regs* regs);
irqreturn_t volP_handler(int irq, void* dev_id, struct pt_regs* regs);
irqreturn_t volM_handler(int irq, void* dev_id, struct pt_regs* regs);

wait_queue_head_t wq;
DECLARE_WAIT_QUEUE_HEAD(wq);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = bullsAndCows_device_open,
	.read = bullsAndCows_device_read,
	.release = bullsAndCows_device_release,
	.unlocked_ioctl = bullsAndCows_device_ioctl,
};

irqreturn_t home_handler(int irq, void* dev_id, struct pt_regs* regs) {

	int i;
	printk("INT 1(Home button / Challenge with current number) = %d!\n", gpio_get_value(HOME));

	if(!enable) return IRQ_HANDLED;

	cnt++;
	fndIdx=0;
	strike = ball = 0;
	guess_num();

	for(i=0; i<4; i++) fnd[i]=0;
	return IRQ_HANDLED;
}

irqreturn_t volP_handler(int irq, void* dev_id, struct pt_regs* regs) {

	printk("INT 3(VOL+ button / Generate next number) = %d!\n", gpio_get_value(VOLP));
	number_idx = (number_idx + 1) % 10;
	ans = NUMBER[number_idx];
	printk("answer is %d\n", ans);
	cnt = 0;

	fpga_lcd_write(0, 0);

	return IRQ_HANDLED;
}

irqreturn_t volM_handler(int irq, void* dev_id, struct pt_regs* regs) {

	int i;
	unsigned long diff;
	printk("INT 4(VOL- button / stop) = %d!\n", gpio_get_value(VOLM));

	// VOL- is pressed
	if (firstPush) {
		firstPush = 0;
		prev = get_jiffies_64();
	}
	// VOL- is released
	else {
		firstPush = 1;
		curr = get_jiffies_64();
		diff = curr - prev;// The time you were pressing the VOL-
		printk("curr-prev : %ld, expire : %ld\n", curr - prev, 30 * BLINK_DELAY);
		// Did you press the button longer than 3 sec
		if (diff >= 29 * BLINK_DELAY) {
			end = realEnd = 1;
		}
		prev = curr;
	}

	if (realEnd) {
		//printk("diff:%lu\n",diff);
		for (i = 0; i < 4; i++) fnd[i] = 0;
		fpga_fnd_write(fnd);
		fpga_lcd_init_write(0,0);
		firstPush = 1;
	//	realEnd = 0;
		arg.state = EXIT;
		del_timer(&timer);
		del_timer(&termination);
		__wake_up(&wq, 1, 1, NULL); // Wake up applicaion process
	}
	return IRQ_HANDLED;
}

int fpga_fnd_write(unsigned int arr[4]){
	unsigned int res = 0;
	res = (arr[3]<<12) | (arr[2]<<8) | (arr[1]<<4) | (arr[0]);
	outw(res, (unsigned int)fnd_addr);
	return 0;
}

int fpga_lcd_write(int idx1, int idx2){
	int i;
	unsigned char str[33];
	str[32] = 0;
	for(i=0; i<32; i++) str[i] = ' ';
	str[0] = 'C'; str[1] = 'N'; str[2] = 'T'; str[3] = ':'; 
	str[16] = 'R'; str[17] = 'E'; str[18] = 'S'; str[19] = 'U';
	str[20] = 'L'; str[21] = 'T'; str[22] = ':';

	if(cnt>=10) {
		str[4] = ((cnt / 10) % 10) + '0';
		str[5] = (cnt % 10) + '0';
	}
	else str[4] = cnt + '0';
	str[23] = strike + '0'; str[24] = 'S'; str[25] = ball + '0'; str[26] = 'B';
	
	for(i=0; i<32; i++){
		outw((unsigned short int)((str[i]&0xFF)<<8) | (str[i+1]&0xFF), (unsigned int)lcd_addr + i);
		i++;
	}

	return 0;
}

int fpga_led_write(int idx){

	unsigned short res = 0;
	res = (1<<(8-idx));
//	if(idx>=1) res = idx;
	outw(res, (unsigned int)led_addr);
	return 0;
}

int fpga_switch_read(void){
	int i;
	unsigned short int res;

	for(i=0; i<9; i++){
		res = inw((unsigned int)switch_addr + 2*i);
		if((res&0xFF)) return i;
	}

	return -1;
}

int fpga_lcd_init_write(int idx1, int idx2){
	int i;
	unsigned char str[33];
	str[32] = 0;
	for(i=0; i<32; i++) str[i] = ' ';
	
	for(i=0; i<32; i++){
		outw((unsigned short int)((str[i]&0xFF)<<8) | (str[i+1]&0xFF), (unsigned int)lcd_addr + i);
		i++;
	}

	return 0;

}

int fpga_lcd_correct_write(int idx1, int idx2){
	int i;
	unsigned char str[33];
	str[32] = 0;
	for(i=0; i<32; i++) str[i] = ' ';
	str[0] = 'C'; str[1] = 'O'; str[2] ='R'; str[3] = 'R'; str[4] = 'E';
	str[5] = 'C'; str[6] = 'T'; str[7] = '!';
	
	for(i=0; i<32; i++){
		outw((unsigned short int)((str[i]&0xFF)<<8) | (str[i+1]&0xFF), (unsigned int)lcd_addr + i);
		i++;
	}

	return 0;

}
void guess_num() {

	int i, tmpAns, tmpInput, x, y;
	int cnt[10];

	for(i=0; i<10; i++) cnt[i]=0;
	input = 0;
	for(i=3; i>=0; i--) {
		input *= 10;
		input += fnd[i];
	}
	printk("You entered %d as the guess answer.\n", input);

	tmpAns = ans;
	for(i=0; i<4; i++) {
		x = tmpAns % 10;	
		cnt[x] = 1;
		tmpAns /= 10;
	}

	tmpAns = ans; tmpInput = input;
	for(i=0; i<4; i++){
		x = tmpAns % 10; y = tmpInput % 10;
		if(x==y) strike++;
		else if(cnt[y]) ball++;
		tmpAns /= 10; tmpInput /= 10;
	}
	if (input != ans) {
		printk("Wrong input.. Try again!\n");
		fpga_lcd_write(0, 0);
	}
	else {
		printk("Correct!\n");
		if(led<8) led++;
		fpga_led_write(led);
		fpga_lcd_correct_write(0, 0);
		number_idx = (number_idx + 1) % 10;
		ans = NUMBER[number_idx];
	}
	del_timer(&timer);
	__wake_up(&wq, 1, 1, NULL);
}
void my_timer_func(unsigned long ptr) {
	int i;
	const int pressed = fpga_switch_read();
	enable = 0;

	if(pressed != -1){
		printk("pressed switch is %d\n", pressed);
		if(fndIdx){
			for(i=fndIdx; i>0; i--) fnd[i] = fnd[i-1];		
		}	
		fnd[0] = pressed+1;

		if(fndIdx<3) fndIdx++;
	}
	fpga_fnd_write(fnd);
	
	timer.expires = get_jiffies_64() + BLINK_DELAY;
	timer.function = my_timer_func;
	add_timer(&timer);
	enable = 1;

	return;
}
static int __init bullsAndCows_device_init(void) {

	int res = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
	if (res < 0) {
		printk(KERN_ALERT "Error in register_chrdev: %d\n", res);
		return res;
	}

	printk("BullsAndCows_device_init!\n");
	printk("I was assigned major number %d, device name %s.\n", MAJOR_NUM, DEVICE_NAME);

	realEnd = 0;
	fnd_addr = ioremap(FND_ADDR, 0x4);
	dot_addr = ioremap(DOT_ADDR, 0x10);
	led_addr = ioremap(LED_ADDR, 0x1);
	switch_addr = ioremap(SWITCH_ADDR, 0x18);
	lcd_addr = ioremap(LCD_ADDR, 0x32);
	
	ans = 9999;
	return SUCCESS;
}
static int bullsAndCows_device_open(struct inode* inode, struct file* file) {
	int irq, res;
	init_timer(&timer); // For the kernel timer
	init_timer(&termination); // For the termination condition

	printk(KERN_ALERT "BullsAndCows_device_open\n");

	if (Device_Open) return -EBUSY;

	/*
		INT register and init
	*/
	//Home INT
	gpio_direction_input(HOME);
	irq = gpio_to_irq(HOME);
	res = request_irq(irq, home_handler, IRQF_TRIGGER_FALLING, "HOME", 0);

	//VOL+ INT
	gpio_direction_input(VOLP);
	irq = gpio_to_irq(VOLP);
	res = request_irq(irq, volP_handler, IRQF_TRIGGER_FALLING, "VOL+", 0);

	//VOL- INT
	gpio_direction_input(VOLM);
	irq = gpio_to_irq(VOLM);
	res = request_irq(irq, volM_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "VOL-", 0);

	Device_Open++; // Usage counter inc
	enable = 0;
	led = 0;
	strike = 0;
	ball = 0;
	cnt = 0;	
	
	return SUCCESS;

}
static int bullsAndCows_device_ioctl(struct file* file, unsigned int ioctl_num, unsigned long ioctl_param) {

	switch (ioctl_num)
	{
	case _IO(MAJOR_NUM, 0): {
		printk("ioctl reading...\n");
		timer.expires = get_jiffies_64() + BLINK_DELAY;
		timer.function = my_timer_func;
		add_timer(&timer);
		enable = 1;
		interruptible_sleep_on(&wq);
		timer.data = (unsigned long)&timer;
		break;
	}
	default:
		printk("ioctl error...\n");
		break;
	}
	return SUCCESS;
}
static int bullsAndCows_device_release(struct inode* inode, struct file* file) {
	int i;
	for (i = 0; i < 4; i++) fnd[i] = 0; // Set FND value to zero

	printk(KERN_ALERT "BullsAndCows_device_release\n");
	del_timer_sync(&timer);
	del_timer_sync(&termination);

	// Freeing an INT handler
	free_irq(HOME, NULL);
	free_irq(VOLP, NULL);
	free_irq(VOLM, NULL);

	Device_Open--; // Usage counter dec
	end = realEnd = 0;
	fpga_fnd_write(fnd);
	fpga_lcd_init_write(0,0);
	firstPush = 1;
	prev = 1;

	return SUCCESS;
}
static ssize_t bullsAndCows_device_read(struct file* file, char* data, size_t len, loff_t* off) {

	if (ans == input) {
		printk("Correct! You got the right answer in %d tries!\n", cnt);
		arg.state = CORRECT;
		arg.cnt = 0;
		cnt = 0;
		fpga_lcd_correct_write(0,0);
	}
	else if(realEnd) {
		arg.state = EXIT;
		realEnd = 0;
		led = 0;
		cnt = 0;
		fpga_lcd_init_write(0,0);
		fpga_led_write(led);
	}
	else {
		arg.state = PLAY;
	}
	arg.cnt = cnt;

	if (copy_to_user(data, &arg, len)) return -EFAULT;

	return len;
}

static void __exit bullsAndCows_device_exit(void) {

	iounmap(fnd_addr);
	iounmap(dot_addr);
	iounmap(led_addr);
	iounmap(switch_addr);
	iounmap(lcd_addr);

	del_timer_sync(&timer);
	del_timer_sync(&termination);

	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

	printk("BullsAndCows_device_exit!\n");
	return;
}
module_init(bullsAndCows_device_init);
module_exit(bullsAndCows_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Inhye Kye");
