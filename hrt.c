#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oleksii Shavykin");

#define MS_TO_NS(x)     (x * 1000000)

struct timer_holder {
	struct seq_file *seq_file;
	struct hrtimer hr_timer;
} hrtimer_holder;

struct wq_holder {
	struct seq_file *seq_file;
	struct delayed_work dwork;
	struct work_struct work;
} wqtimer_holder;

static unsigned int count;
static unsigned int wq_count;
static unsigned int repeats = 5;
static unsigned long delay_in_ms = 200L;
static struct dentry *repeats_dentry;
static struct dentry *delay_dentry;
static struct dentry *timer_dentry;
static struct dentry *wq_dentry;

// hrt_timer callback
enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
	if (--count) {
		seq_printf(hrtimer_holder.seq_file,
				"%s called (%lld).\n", __func__,
				ktime_to_ms(timer->base->get_time()));
		hrtimer_forward_now(timer, ns_to_ktime(MS_TO_NS(delay_in_ms)));
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

// hrt_timer seq_file show
static int hrt_show(struct seq_file *m, void *v)
{
	ktime_t ktime;

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	hrtimer_holder.seq_file = m;

	count = repeats + 1;

	seq_printf(m, "Starting timer to fire in %lld ms (%ld)\n",
			ktime_to_ms(hrtimer_holder.hr_timer.base->get_time())
			+ delay_in_ms, jiffies);

	hrtimer_start(&hrtimer_holder.hr_timer, ktime, HRTIMER_MODE_REL);

	while (count)
		schedule();

	return 0;
}

// hrt_timer open
static int hrt_open(struct inode *inode, struct file *file)
{
	return single_open(file, hrt_show, inode->i_private);
}

const struct file_operations hrt_fops = {
	.open = hrt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

// hrt_wq_timer work callback
static void wq_print(struct work_struct *work)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	seq_printf(wqtimer_holder.seq_file,
			"%s called (%10i.%06i).\n", __func__,
			(int) tv.tv_sec, (int) tv.tv_usec);

	if (--wq_count)
		schedule_delayed_work(&wqtimer_holder.dwork, delay_in_ms);
}

// hrt_wq_timer seq_file show
static int wq_show(struct seq_file *m, void *v)
{
	ktime_t ktime;

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	wqtimer_holder.seq_file = m;

	wq_count = repeats;

	seq_printf(m, "Starting workqueue to fire %d times every %ldms\n",
			repeats, delay_in_ms);

	schedule_delayed_work(&wqtimer_holder.dwork, delay_in_ms);

	while (wq_count)
		schedule();

	return 0;
}

// hrt_wq_timer open
static int wq_open(struct inode *inode, struct file *file)
{
	return single_open(file, wq_show, inode->i_private);
}

const struct file_operations wq_fops = {
	.open = wq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

// Module init
int init_module(void)
{
	INIT_DELAYED_WORK(&wqtimer_holder.dwork, wq_print);

	repeats_dentry = debugfs_create_u32("hrt_repeats", 0644, NULL,
						&repeats);
	delay_dentry = debugfs_create_ulong("hrt_delay", 0644, NULL,
						&delay_in_ms);
	timer_dentry = debugfs_create_file("hrt_timer", 0644, NULL, NULL,
						&hrt_fops);
	wq_dentry = debugfs_create_file("hrt_wq_timer", 0644, NULL, NULL,
						&wq_fops);

	hrtimer_init(&hrtimer_holder.hr_timer, CLOCK_MONOTONIC,
						HRTIMER_MODE_REL);
	hrtimer_holder.hr_timer.function = &my_hrtimer_callback;

	if (!repeats_dentry || !delay_dentry || !timer_dentry || !wq_dentry)
		return -EINVAL;

	return 0;
}

// Module exit
void cleanup_module(void)
{
	int ret;

	debugfs_remove(repeats_dentry);
	debugfs_remove(delay_dentry);
	debugfs_remove(timer_dentry);
	debugfs_remove(wq_dentry);

	ret = hrtimer_cancel(&hrtimer_holder.hr_timer);
	if (ret)
		pr_info("The timer was still in use...\n");

	pr_info("HR Timer module uninstalling\n");
}

