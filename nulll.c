
// https://github.com/vsinitsyn/reverse
// https://github.com/torvalds/linux/blob/master/include/linux/fs.h
// http://www.tldp.org/LDP/lkmpg/2.6/html/x569.html
// https://raw.githubusercontent.com/torvalds/linux/master/drivers/char/mem.c
// http://unix.stackexchange.com/questions/4711/what-is-the-difference-between-ioctl-unlocked-ioctl-and-compat-ioctl

#include <linux/init.h>		/* __init and __exit macroses */
#include <linux/kernel.h>	/* KERN_INFO macros */
#include <linux/module.h>	/* required for all kernel modules */
#include <linux/moduleparam.h>	/* module_param() and MODULE_PARM_DESC() */

#include <linux/fs.h>		/* struct file_operations, struct file */
#include <linux/miscdevice.h>	/* struct miscdevice and misc_[de]register() */
#include <linux/mutex.h>	/* mutexes */
#include <linux/string.h>	/* memchr() function */
#include <linux/slab.h>		/* kzalloc() function */
#include <linux/sched.h>	/* wait queues */
#include <linux/uaccess.h>	/* copy_{to,from}_user() */


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me and myself");
MODULE_DESCRIPTION("Throttled /dev/null device");

static unsigned long capacity = 0;// defaults to no limit
module_param(capacity, ulong, (S_IRUSR | S_IRGRP | S_IROTH));
MODULE_PARM_DESC(capacity, "Capacity in bytes");


static unsigned long written = 0;
static struct mutex lock;


static ssize_t nulll_read(struct file *file, char __user * out, size_t size, loff_t * off)
{
	return 0;
}

static ssize_t nulll_write(struct file *file, const char __user * in, size_t size, loff_t * off)
{
	ssize_t result;

	if (mutex_lock_interruptible(&lock)) {
		result = -ERESTARTSYS;
		goto out;
	}

	if (capacity && written + size > capacity) {// check within critical section to avoid racing
		result = -ENOSPC;
		goto out_unlock;
	}

	written += size;

	result = size;

out_unlock:
	mutex_unlock(&lock);

out:
	return result;
}

static loff_t nulll_lseek(struct file *file, loff_t offset, int orig)
{
	return file->f_pos = 0;
}

static long nulll_ioctl(struct file *file, unsigned int cmd, unsigned long arg) 
{
	long result = 0;// success

	switch (cmd) 
	{
		case BLKGETSIZE64:
			if (mutex_lock_interruptible(&lock)) {
				result = -ERESTARTSYS;
				goto out;
			}

			if (copy_to_user((void __user *)arg, &written, sizeof(written))) {
				result = -EFAULT;
			}

			mutex_unlock(&lock);
			
			break;
		default:
			result = -ENOIOCTLCMD;// unsupported ops
	}

out:        
	return result;
}

static struct file_operations nulll_fops = {
	.owner = THIS_MODULE,
	.read = nulll_read,
	.write = nulll_write,
	.llseek = nulll_lseek,
	.unlocked_ioctl = nulll_ioctl
};

static struct miscdevice nulll_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "nulll",
	.fops = &nulll_fops
};

static int __init nulll_init(void)
{
	if (misc_register(&nulll_misc_device)) {
		printk(KERN_ERR "misc_register failure");// error is fatal here
		return -1;
	}

	printk(KERN_INFO "nulll device has been registered, capacity = %lu byte(s) [0 stands for no limit]\n", capacity);// 0 for no limit

	mutex_init(&lock);

	return 0;
}

static void __exit nulll_exit(void)
{
	mutex_destroy(&lock);

	misc_deregister(&nulll_misc_device);
	
	printk(KERN_INFO "nulll device has been unregistered\n");
}

module_init(nulll_init);
module_exit(nulll_exit);
