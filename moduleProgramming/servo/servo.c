#include<linux/gpio.h>
#include<linux/module.h>
#include<linux/init.h>
#include<linux/delay.h>
#include<linux/fs.h>
#include<linux/kernel.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>
#include<linux/slab.h>
#include<linux/jiffies.h>
#include<linux/timer.h>

#define MY_SERVO_NUMBER 100
#define MY_SERVO_WRITE _IOW ( MY_SERVO_NUMBER, 1, int )

MODULE_LICENSE("GPL");

extern unsigned long volatile __jiffy_data jiffies;

struct cdev my_cdev;
dev_t devno;
int major = 210;
char *msg = NULL;
int servo_mode;
int pin = 18;

struct timer_list my_timer;

void servo_control_func(unsigned long arg){
    gpio_set_value(pin, 1);
    mdelay(servo_mode);
    gpio_set_value(pin, 0);
    if(servo_mode){ //if servo mode is stop -> stop timer
        mod_timer(&my_timer, jiffies + 2);
    }
    return;
}

int servo_dev_open(struct inode *pinode, struct file *pfile){
    printk("Open servo drv\n");
    return 0;
}

int servo_dev_close(struct inode *pinode, struct file *pfile){
    printk("Close servo drv\n");
    return 0;
}

long servo_dev_write(struct file *pfile, unsigned int command, unsigned long arg){
    if(command == MY_SERVO_WRITE){
        memset(msg, 0, 32);
        if(copy_from_user((void *)msg, (const void *)arg, (ssize_t)strlen((char *)arg))){
            printk("Write err\n");
            return -1;
        }
        printk("copy_from_user: %s\n", msg);

        //parse int from msg
        int temp = 0, i = 0;
        while(msg[i]){
            temp *= 10; temp += msg[i] - '0';
            i++;
        }
        if(temp >= 0 && temp <= 2){ //useable mode 0, 1, 2(0: stop, 1: cw, 2: ccw)
            servo_mode = temp; //set mode

            if(temp){ // if mode is not stop setting timer
                mod_timer(&my_timer, jiffies + 2);
            }
        }

        printk("servo speed set done %d\n", temp);
    }
    else{
        printk("Read fail\n");
    }
    return 0;
}

struct file_operations fop = {
    .owner = THIS_MODULE,
    .open = servo_dev_open,
    .unlocked_ioctl = servo_dev_write,
    .release = servo_dev_close,
};

int __init servo_init(void){
    printk("INIT servo moter\n");

    devno = MKDEV(major, 0);
    register_chrdev_region(devno, 1, "servo_dev");
    cdev_init(&my_cdev, &fop);
    my_cdev.owner = THIS_MODULE;

    msg = (char *)kmalloc(32, GFP_KERNEL);
    printk("MAJOR: %d\n", MAJOR(devno));

    init_timer(&my_timer);

    my_timer.function = servo_control_func;
    my_timer.data = &servo_mode;
    my_timer.expires = jiffies + 2;

    add_timer(&my_timer);

    if(cdev_add(&my_cdev, devno, 1) < 0){
        printk("Device add err\n");
        return -1;
    }

    if(gpio_request(pin, "SERVO_PIN") < 0){
        printk("gpio_request fail\n");
    }

    if(gpio_direction_output(pin, 0) < 0){
        printk("gpio_direction_output fail\n");
    }
    printk("HZ: %d\n", HZ);

    return 0;
}

void __exit servo_exit(void){
    printk("EXIT servo module\n");
    if(msg) kfree(msg);
    del_timer(&my_timer);
    gpio_free(pin);
    cdev_del(&my_cdev);
}

module_init(servo_init);
module_exit(servo_exit);