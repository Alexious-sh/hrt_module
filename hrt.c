#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

#define MAX_BUF_SIZE	1024
#define MS_TO_NS(x)     (x * 1E6L)

static struct hrtimer hr_timer;

static unsigned int count = 5;
static unsigned long delay_in_ms = 200L;
static struct dentry *count_dentry;
static struct dentry *delay_dentry;
static struct dentry *timer_dentry;
static unsigned long buffer_size = 0;
static char timer_buf[MAX_BUF_SIZE];

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
	pr_info("my_hrtimer_callback called (%lld).\n", ktime_to_ms(timer->base->get_time()));
	if (count--) {
		hrtimer_forward_now(timer, ns_to_ktime(MS_TO_NS(delay_in_ms)));
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

static int hrt_show(struct seq_file *m, void *v)
{
	ktime_t ktime;

	//ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hr_timer.function = &my_hrtimer_callback;

	//pr_info("Starting timer to fire in %lld ms (%ld)\n", ktime_to_ms(hr_timer.base->get_time()) + delay_in_ms, jiffies);

	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
	seq_printf(m, "hello world\n");
	return 0;
}

static int hrt_open(struct inode *inode, struct file *file)
{
	return single_open(file, hrt_show, inode->i_private);
}

struct file_operations timer_fops = {
	.open = hrt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int init_module(void)
{
	count_dentry = debugfs_create_u32("hrt_count", 0644, NULL, &count);
	delay_dentry = debugfs_create_u32("hrt_delay", 0644, NULL, &delay_in_ms);
	timer_dentry = debugfs_create_file("hrt_timer", 0644, NULL, NULL, &timer_fops);

	if (!count_dentry || !delay_dentry || !timer_dentry)
		return -EINVAL;

	return 0;
}

void cleanup_module(void)
{
	int ret;

	debugfs_remove(count_dentry);
	debugfs_remove(delay_dentry);
	debugfs_remove(timer_dentry);

	ret = hrtimer_cancel(&hr_timer);
	if (ret)
		pr_info("The timer was still in use...\n");

	pr_info("HR Timer module uninstalling\n");

	return;
}

