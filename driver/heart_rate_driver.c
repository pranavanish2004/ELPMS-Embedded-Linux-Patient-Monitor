// SPDX-License-Identifier: GPL-2.0
/*
 * heart_rate_driver.c  --  ELPMS Phase 1: the "fake heart" sensor
 *
 * A Linux character device driver (loadable kernel module) that simulates a
 * heart-rate sensor. Every time user space read()s the device file, the
 * driver returns one fresh random heart rate between 60 and 100 BPM,
 * formatted as text, e.g. "78\n".
 *
 * How it fits in the system:
 *
 *   user space:   cat /dev/heart_rate   or   logger.cpp  (read() syscall)
 *                          |
 *   kernel space:   file_operations.read  ->  hr_read()   (this file)
 *
 * Real sensor drivers follow exactly this pattern; the only difference is
 * that they would talk to hardware registers (I2C/SPI/GPIO) instead of
 * calling get_random_bytes().
 */

#include <linux/module.h>   /* module_init, module_exit, MODULE_* macros */
#include <linux/kernel.h>   /* pr_info, pr_err, scnprintf                */
#include <linux/fs.h>       /* struct file_operations, register_chrdev   */
#include <linux/random.h>   /* get_random_bytes                          */

#define DEVICE_NAME "heart_rate"   /* name shown in /proc/devices        */
#define MIN_BPM     60
#define MAX_BPM     100

/* Major number handed to us by the kernel when we register. */
static int major_number;

/*
 * hr_read - called by the kernel on every read() of /dev/heart_rate.
 *
 * @file:     the open file (unused, we keep no per-open state)
 * @user_buf: destination buffer in USER space (the __user tag means the
 *            kernel must never dereference it directly)
 * @count:    how many bytes user space asked for
 * @ppos:     current file offset; used to signal end-of-file
 *
 * Returns the number of bytes copied, or 0 for end-of-file.
 */
static ssize_t hr_read(struct file *file, char __user *user_buf,
		       size_t count, loff_t *ppos)
{
	char msg[16];
	unsigned int rnd;
	int bpm, len;

	/*
	 * End-of-file handling: one open() yields exactly one reading.
	 * `cat` calls read() repeatedly until it gets 0. Without this check
	 * it would loop forever printing numbers.
	 */
	if (*ppos > 0)
		return 0;

	/* Random number -> value in the range [MIN_BPM, MAX_BPM] inclusive. */
	get_random_bytes(&rnd, sizeof(rnd));
	bpm = MIN_BPM + (rnd % (MAX_BPM - MIN_BPM + 1));

	/* Render as text with a trailing newline, e.g. "78\n". */
	len = scnprintf(msg, sizeof(msg), "%d\n", bpm);

	/*
	 * simple_read_from_buffer() does the safe kernel->user copy
	 * (copy_to_user under the hood), honours `count` and advances *ppos.
	 */
	return simple_read_from_buffer(user_buf, count, ppos, msg, len);
}

/* Tells the kernel which function handles which system call. */
static const struct file_operations hr_fops = {
	.owner = THIS_MODULE,
	.read  = hr_read,
};

/* Runs once when the module is loaded (sudo insmod). */
static int __init hr_init(void)
{
	/* Major number 0 means "kernel, please pick a free one". */
	major_number = register_chrdev(0, DEVICE_NAME, &hr_fops);
	if (major_number < 0) {
		pr_err("heart_rate: register_chrdev failed (%d)\n", major_number);
		return major_number;
	}

	pr_info("heart_rate: loaded, major number = %d\n", major_number);
	pr_info("heart_rate: create node with: sudo mknod /dev/heart_rate c %d 0\n",
		major_number);
	return 0;
}

/* Runs once when the module is removed (sudo rmmod). */
static void __exit hr_exit(void)
{
	unregister_chrdev(major_number, DEVICE_NAME);
	pr_info("heart_rate: unloaded\n");
}

module_init(hr_init);
module_exit(hr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ELPMS Portfolio Project");
MODULE_DESCRIPTION("Simulated heart-rate sensor (character device)");
MODULE_VERSION("1.0");
