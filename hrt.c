#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oleksii Shavykin");

#define MS_TO_NS(x)     (x * 1000000)

struct hrtimer_holder {
	struct hrtimer hr_timer;
	struct seq_file *seq_file;
};

static struct hrtimer_holder timer_holder;

static unsigned int count;
static unsigned int repeats = 5;
static unsigned long delay_in_ms = 200L;
static struct dentry *repeats_dentry;
static struct dentry *delay_dentry;
static struct dentry *timer_dentry;

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
	struct hrtimer_holder *holder = container_of(timer,
			struct hrtimer_holder, hr_timer);

	if (--count) {
		seq_printf(holder->seq_file,
				"%s called (%lld).\n", __func__,
				ktime_to_ms(timer->base->get_time()));
		hrtimer_forward_now(timer, ns_to_ktime(MS_TO_NS(delay_in_ms)));
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

static int hrt_show(struct seq_file *m, void *v)
{
	ktime_t ktime;

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	timer_holder.seq_file = m;

	count = repeats + 1;

	seq_printf(m, "Starting timer to fire in %lld ms (%ld)\n",
			ktime_to_ms(timer_holder.hr_timer.base->get_time())
			+ delay_in_ms, jiffies);

	hrtimer_start(&timer_holder.hr_timer, ktime, HRTIMER_MODE_REL);

	while (count)
		schedule();

	return 0;
}

static int hrt_open(struct inode *inode, struct file *file)
{
	return single_open(file, hrt_show, inode->i_private);
}

const struct file_operations timer_fops = {
	.open = hrt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int init_module(void)
{
	repeats_dentry = debugfs_create_u32("hrt_repeats", 0644, NULL,
						&repeats);
	delay_dentry = debugfs_create_ulong("hrt_delay", 0644, NULL,
						&delay_in_ms);
	timer_dentry = debugfs_create_file("hrt_timer", 0644, NULL, NULL,
						&timer_fops);

	hrtimer_init(&timer_holder.hr_timer, CLOCK_MONOTONIC,
						HRTIMER_MODE_REL);
	timer_holder.hr_timer.function = &my_hrtimer_callback;

	if (!repeats_dentry || !delay_dentry || !timer_dentry)
		return -EINVAL;

	return 0;
}

void cleanup_module(void)
{
	int ret;

	debugfs_remove(repeats_dentry);
	debugfs_remove(delay_dentry);
	debugfs_remove(timer_dentry);

	ret = hrtimer_cancel(&timer_holder.hr_timer);
	if (ret)
		pr_info("The timer was still in use...\n");

	pr_info("HR Timer module uninstalling\n");
}

