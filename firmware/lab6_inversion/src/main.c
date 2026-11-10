/*
 * Lab 6 — Task B: priority inversion and priority inheritance.
 *
 * high (priority 3)  = control
 * medium (priority 6) = telemetry
 * low (priority 9)   = slow console command
 *
 * First test: k_sem as a lock, which has no priority inheritance.
 * Second test: k_mutex, which has priority inheritance in Zephyr.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#define LOW_CRITICAL_US  3000U
#define MEDIUM_WORK_US  10000U

#define INSTR(node) GPIO_DT_SPEC_GET_OR(DT_NODELABEL(node), gpios, {0})

static const struct gpio_dt_spec instr_high = INSTR(inv_high);
static const struct gpio_dt_spec instr_medium = INSTR(inv_medium);
static const struct gpio_dt_spec instr_low = INSTR(inv_low);
static const struct gpio_dt_spec instr_b = INSTR(inv_b);

/* Semaphores used only to orchestrate the classic inversion sequence. */
K_SEM_DEFINE(low_go, 0, 1);
K_SEM_DEFINE(high_go, 0, 1);
K_SEM_DEFINE(medium_go, 0, 1);
K_SEM_DEFINE(case_done, 0, 1);

/* The two alternatives for the shared resource. */
K_SEM_DEFINE(shared_sem, 1, 1);
K_MUTEX_DEFINE(shared_mutex);

static atomic_t use_mutex;
static atomic_t high_wait_us;
static atomic_t low_hold_us;

static inline void instr_set(const struct gpio_dt_spec *pin, int value)
{
	if (pin->port != NULL) {
		gpio_pin_set_dt(pin, value);
	}
}

static void lock_shared(void)
{
	if (atomic_get(&use_mutex)) {
		(void)k_mutex_lock(&shared_mutex, K_FOREVER);
	} else {
		(void)k_sem_take(&shared_sem, K_FOREVER);
	}
}

static void unlock_shared(void)
{
	if (atomic_get(&use_mutex)) {
		k_mutex_unlock(&shared_mutex);
	} else {
		k_sem_give(&shared_sem);
	}
}

static void high_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		uint32_t requested;
		uint32_t acquired;

		k_sem_take(&high_go, K_FOREVER);

		instr_set(&instr_high, 1);
		requested = k_cycle_get_32();

		lock_shared();
		acquired = k_cycle_get_32();

		atomic_set(&high_wait_us,
			(atomic_val_t)k_cyc_to_us_floor32(acquired - requested));

		unlock_shared();
		instr_set(&instr_high, 0);

		k_sem_give(&case_done);
	}
}

static void medium_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&medium_go, K_FOREVER);

		instr_set(&instr_medium, 1);
		k_busy_wait(MEDIUM_WORK_US);
		instr_set(&instr_medium, 0);
	}
}

static void low_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		uint32_t held_start;
		uint32_t held_end;

		k_sem_take(&low_go, K_FOREVER);

		instr_set(&instr_low, 1);
		lock_shared();

		held_start = k_cycle_get_32();
		instr_set(&instr_b, 1);

		/*
		 * High wakes and tries the locked resource. It blocks.
		 * Then low makes medium ready and yields again.
		 */
		k_sem_give(&high_go);
		k_yield();

		k_sem_give(&medium_go);
		k_yield();

		/*
		 * With k_sem, medium (priority 6) runs before low (priority 9).
		 * With k_mutex, low inherits high's priority and medium cannot run.
		 */
		k_busy_wait(LOW_CRITICAL_US);

		held_end = k_cycle_get_32();
		atomic_set(&low_hold_us,
			(atomic_val_t)k_cyc_to_us_floor32(held_end - held_start));

		instr_set(&instr_b, 0);
		unlock_shared();
		instr_set(&instr_low, 0);
	}
}

static void run_case(bool mutex_mode)
{
	atomic_set(&use_mutex, mutex_mode ? 1 : 0);
	atomic_set(&high_wait_us, 0);
	atomic_set(&low_hold_us, 0);

	k_sem_give(&low_go);

	if (k_sem_take(&case_done, K_MSEC(100)) != 0) {
		printk("%s: timeout\n", mutex_mode ? "MUTEX" : "SEM");
		return;
	}

	printk("%s: high_wait_us=%ld low_lock_held_us=%ld "
	       "B_work_us=%u medium_work_us=%u\n",
	       mutex_mode ? "MUTEX" : "SEM",
	       (long)atomic_get(&high_wait_us),
	       (long)atomic_get(&low_hold_us),
	       LOW_CRITICAL_US,
	       MEDIUM_WORK_US);
}

static void run_no_contention(void)
{
	atomic_set(&use_mutex, 0);
	atomic_set(&high_wait_us, 0);
	atomic_set(&low_hold_us, 0);

	/*
	 * Solo despierta el hilo alto: el recurso está libre.
	 * Por tanto, esta es la referencia sin contención.
	 */
	k_sem_give(&high_go);

	if (k_sem_take(&case_done, K_MSEC(100)) != 0) {
		printk("NONE: timeout\n");
		return;
	}

	printk("NONE: high_wait_us=%ld low_lock_held_us=0\n",
	       (long)atomic_get(&high_wait_us));
}

static void runner_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sleep(K_SECONDS(1));

	while (true) {
	run_no_contention();
	k_sleep(K_SECONDS(2));

	run_case(false); /* k_sem: inversión, sin herencia. */
	k_sleep(K_SECONDS(2));

	run_case(true);  /* k_mutex: herencia de prioridad. */
	k_sleep(K_SECONDS(3));
	}
}

K_THREAD_DEFINE(high_tid, 1024, high_thread,
		NULL, NULL, NULL, 3, 0, 0);

K_THREAD_DEFINE(medium_tid, 1024, medium_thread,
		NULL, NULL, NULL, 6, 0, 0);

K_THREAD_DEFINE(low_tid, 1024, low_thread,
		NULL, NULL, NULL, 9, 0, 0);

K_THREAD_DEFINE(runner_tid, 1024, runner_thread,
		NULL, NULL, NULL, 10, 0, 0);

int main(void)
{
	const struct gpio_dt_spec *pins[] = {
		&instr_high, &instr_medium, &instr_low, &instr_b
	};

	for (size_t i = 0; i < ARRAY_SIZE(pins); i++) {
		if (pins[i]->port != NULL) {
			gpio_pin_configure_dt(pins[i], GPIO_OUTPUT_INACTIVE);
		}
	}

	printk("Lab 6 Task B: priority inversion\n");
	printk("SEM = broken; MUTEX = priority inheritance\n");
	printk("high=control, medium=telemetry, low=console\n");

	return 0;
}
