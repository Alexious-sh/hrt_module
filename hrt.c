#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

MODULE_LICENSE("GPL");

#define MS_TO_NS(x)     (x * 1E6L)

static struct hrtimer hr_timer;

static int restart = 5;
static unsigned long delay_in_ms = 200L;

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
	pr_info( "my_hrtimer_callback called (%lld).\n", ktime_to_ms(timer->base->get_time()));
	if (restart--) {
		hrtimer_forward_now(timer, ns_to_ktime(MS_TO_NS(delay_in_ms)));
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

int init_module(void)
{
	ktime_t ktime;

	pr_info("HR Timer module installing\n");

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hr_timer.function = &my_hrtimer_callback;

	pr_info( "Starting timer to fire in %lld ms (%ld)\n", ktime_to_ms(hr_timer.base->get_time()) + delay_in_ms, jiffies);

	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

	return 0;
}

void cleanup_module(void)
{
	int ret;

	ret = hrtimer_cancel(&hr_timer);
	if (ret)
		pr_info("The timer was still in use...\n");

	pr_info("HR Timer module uninstalling\n");

	return;
}

