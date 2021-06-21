#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/timer.h>
#include <linux/platform_device.h>

#define SUCCESS 0
#define TIMER_DEVICE "/dev/dev_driver"
#define MAJOR_NUM 242
#define SET_OPTION _IOR(MAJOR_NUM, 0, _arg *) // macros to encode a command(SET_OPTION)
#define COMMAND _IOR(MAJOR_NUM, 1, _arg *) // macros to encode a command(COMMAND)
#define MY_NUMBER "20181593" // my student #
#define MY_NAME "Inhye Kye" // my name
#define FND_ADDR 0x08000004 // physical addr for FND
#define LED_ADDR 0x08000016 // physical addr for LED
#define DOT_ADDR 0x08000210 // physical addr for DOT
#define LCD_ADDR 0x08000090 // physical addr for LCD
#define BLINK_DELAY HZ/10; // timer value

unsigned char fpga_number[9][10] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // empty
    {0x0c,0x1c,0x1c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e}, // 1
    {0x7e,0x7f,0x03,0x03,0x3f,0x7e,0x60,0x60,0x7f,0x7f}, // 2
    {0xfe,0x7f,0x03,0x03,0x7f,0x7f,0x03,0x03,0x7f,0x7e}, // 3
    {0x66,0x66,0x66,0x66,0x66,0x66,0x7f,0x7f,0x06,0x06}, // 4
    {0x7f,0x7f,0x60,0x60,0x7e,0x7f,0x03,0x03,0x7f,0x7e}, // 5
    {0x60,0x60,0x60,0x60,0x7e,0x7f,0x63,0x63,0x7f,0x3e}, // 6
    {0x7f,0x7f,0x63,0x63,0x03,0x03,0x03,0x03,0x03,0x03}, // 7
    {0x3e,0x7f,0x63,0x63,0x7f,0x7f,0x63,0x63,0x7f,0x3e}, // 8
};

static unsigned char* fnd_addr, *led_addr, *dot_addr, *lcd_addr;

static struct _my_data {
    struct timer_list timer;
    int cnt;
};
struct _my_data my_data;

// structure for ioctl argument
typedef struct ioctl_arg {
    int interval, cnt, init;
}_arg;
_arg input;

static int interval, cnt, init, fndIdx;
static int fndUsage=0, ledUsage=0, dotUsage=0, lcdUsage=0; // usage counter
static unsigned int fnd[4];
int rotate = 1, idx1 = 0, idx2 = 0, dir1, dir2;

int device_open(struct inode*, struct file*);
int device_ioctl(struct file*, unsigned int, unsigned long);
int device_release(struct inode*, struct file*);
int fpga_fnd_write(unsigned int arr[4]);
int fpga_led_write(int idx);
int fpga_dot_write(int idx);
int fpga_lcd_write(int idx1, int idx2);
int findIdx(int arr[4], int num); 
void my_timer_func(unsigned long ptr);

struct file_operations fops = {
    owner:  THIS_MODULE,
    open:  device_open,
    release: device_release,
    unlocked_ioctl: device_ioctl,
};

int fpga_fnd_write(unsigned int arr[4]) {

    unsigned int res = 0;
    res = (arr[3] << 12) | (arr[2] << 8) | (arr[1] << 4) | (arr[0]);
    outw(res, (unsigned int)fnd_addr);
    return 0;
}
int fpga_led_write(int idx) {

    unsigned short res = 0;
    //printk("fpga_led_write/ digit : %d, idx : %d\n",fnd[fndIdx], idx);
    //if (idx >= 1) res = (1 << (8 - idx));
    if(idx>=1) res = idx;
    outw(res, (unsigned int)led_addr);

    return 0;
}
int fpga_dot_write(int idx) {

    int i;
    for (i = 0; i < 10; i++) {
        outw((unsigned int)(fpga_number[idx][i] & 0x7F), (unsigned int)dot_addr + 2*i);
    }

    return 0;
}
int fpga_lcd_write(int idx1, int idx2) {
    int i;
    unsigned char str[33];
    str[32] = 0;
    for (i = 0; i < 32; i++) str[i] = ' ';
    //set first line for my student #
    for (i = 7; i >= 0; i--) {
        str[idx1 + i] = MY_NUMBER[i];
    }
    //set second line for my name
    for (i = 8; i >= 0; i--) {
        str[idx2 + 16 + i] = MY_NAME[i];
    }
    printk("fpga_lcd_write(str) : %s\n", str);
    for (i = 0; i < 32; i++) {
        outw((unsigned short int)((str[i] & 0xFF) << 8) | (str[i + 1] & 0xFF), (unsigned int)lcd_addr + i);
        i++;
    }

    return 0;
}

void my_timer_func(unsigned long ptr) {
    struct _my_data* pstatus = (struct _my_data*)ptr;
    int tmp = fnd[fndIdx];
    int i;
    
    pstatus->cnt--;
    printk("my timer func(cnt : %d)\n", pstatus->cnt);
    //TIMER_CNT is over
    if (pstatus->cnt == 0) {
	//printk("cnt over!!\n");
        for (i = 0; i < 4; i++) fnd[i] = 0;
        fpga_fnd_write(fnd);
        fpga_led_write(0);
        fpga_dot_write(0);
        for(i=0; i<32; i+=2){
		outw(0x2020, (unsigned int)lcd_addr+i);
	}
	rotate = 1;
        return;
    }
    
    //shift output position after rotation
    if (rotate == 8) {
        rotate = 0;
        fnd[fndIdx] = 0;
        fndIdx = (fndIdx + 3) % 4;
        fnd[fndIdx] = tmp;
    }
    
    rotate++;
    //increased by 1 from the current number
    if (fnd[fndIdx] == 8) fnd[fndIdx] = 1;
    else fnd[fndIdx] = (fnd[fndIdx] + 1) % 9;

    //move one step at a time according to direction
    idx1 += dir1;
    idx2 += dir2;

    //change of direction 
    if (!(idx1 % 8)) dir1 *= -1;
    if (!(idx2 % 7)) dir2 *= -1;

    fpga_fnd_write(fnd);
    fpga_led_write(1<<(8-fnd[fndIdx]));
    fpga_dot_write(fnd[fndIdx]);
    fpga_lcd_write(idx1, idx2);

    my_data.timer.expires = get_jiffies_64() + interval * BLINK_DELAY;
    my_data.timer.function = my_timer_func;
    my_data.timer.data = (unsigned long)&my_data;
    add_timer(&my_data.timer);

    return;
}

//find a nonzero position
int findIdx(int arr[4], int num) {
    int i, res = -1;

    arr[0] = num % 10; num /= 10;
    arr[1] = num % 10; num /= 10;
    arr[2] = num % 10; num /= 10;
    arr[3] = num % 10;

    for (i = 0; i < 4; i++) {
        if (arr[i]) return i;
    }
    return res;
}
int device_open(struct inode* inode, struct file* file) {

    if (fndUsage || ledUsage || dotUsage || lcdUsage) return -EBUSY;
    fndUsage++; ledUsage++; dotUsage++; lcdUsage++;

    return SUCCESS;
}
int device_ioctl(struct file* file, unsigned int ioctl_num, unsigned long ioctl_param) {

    int res = copy_from_user(&input, (_arg*)ioctl_param, sizeof(_arg));
    
    if (res < 0) {
        printk(KERN_ALERT "Error in device_ioctl: %d\n", res);
        return res;
    }
   
    interval = input.interval;
    cnt = input.cnt;
    init = input.init;
    printk("interval : %d, cnt : %d, init : %d\n", interval, cnt, init);

    switch (ioctl_num) 
    {
    case SET_OPTION: {
	//initialize
        fndIdx = findIdx(fnd, init);
        dir1 = dir2 = 1;
        idx1 = idx2 = 0;
        fpga_fnd_write(fnd);
        fpga_led_write(1 << (8 - fnd[fndIdx]));
        fpga_dot_write(fnd[fndIdx]);
        fpga_lcd_write(idx1, idx2);
        break;
    }
    case COMMAND: {
        my_data.timer.expires = jiffies + interval * BLINK_DELAY;
        my_data.timer.function = my_timer_func;
        my_data.timer.data = (unsigned long)&my_data;
        add_timer(&my_data.timer);
        my_data.cnt = input.cnt;
        break;
    }
    default:
        break;
    }
    return SUCCESS;
}
int device_release(struct inode* inode, struct file* file) {
    fndUsage = ledUsage = dotUsage = lcdUsage = 0;
    //printk("device release here!\n");
    return SUCCESS;
}
static int __init timer_device_init(void) {
    int res; 
    fnd_addr = ioremap(FND_ADDR, 0x4);
    led_addr = ioremap(LED_ADDR, 0x1);
    dot_addr = ioremap(DOT_ADDR, 0x10);
    lcd_addr = ioremap(LCD_ADDR, 0x32);

    res = register_chrdev(MAJOR_NUM, TIMER_DEVICE, &fops);
    if (res < 0) {
        printk(KERN_ALERT "Error in register_chrdev: %d\n", res);
        return res;
    }
    printk("timer_device_init!\n");
    printk("I was assigned major number %d, device name %s.\n", MAJOR_NUM, TIMER_DEVICE);
    init_timer(&(my_data.timer));

    return SUCCESS;
}

static void __exit timer_device_exit(void) {
    iounmap(fnd_addr);
    iounmap(led_addr);
    iounmap(dot_addr);
    iounmap(lcd_addr);

    del_timer_sync(&my_data.timer);
    unregister_chrdev(MAJOR_NUM, TIMER_DEVICE);
    
    printk("timer_device_exit!\n");
    return;
}
module_init(timer_device_init);
module_exit(timer_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Inhye Kye");
