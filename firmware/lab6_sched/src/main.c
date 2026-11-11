#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

/* Figura 4.13: tau1=(C=2 ms, T=5 ms), tau2=(C=4 ms, T=7 ms). */

#define TAU1_C_US 2000U
#define TAU1_T_US 5000U
#define TAU2_C_US 4000U
#define TAU2_T_US 7000U

/*
 * EDF solamente compara deadlines entre hilos con igual prioridad estática.
 */
#define EDF_PRIO 2
#define REPORT_PRIO 9

K_MSGQ_DEFINE(tau1_q, sizeof(uint32_t), 8, 4);
K_MSGQ_DEFINE(tau2_q, sizeof(uint32_t), 8, 4);

static atomic_t tau1_misses;
static atomic_t tau2_misses;
static atomic_t tau2_first_finish_us;
static atomic_t tau2_first_deadline_us;

static uint32_t first_release_cycle;

static void preemptible_work(uint32_t total_us)
{
	uint32_t start = k_cycle_get_32();

	while (k_cyc_to_us_floor32(k_cycle_get_32() - start) < total_us) {
		/* Trabajo breve y punto para que el planificador actúe. */
		k_busy_wait(50);
		k_yield();
	}
}

static void tau1_timer_expiry(struct k_timer *timer)
{
	uint32_t released = k_cycle_get_32();

	ARG_UNUSED(timer);
	(void)k_msgq_put(&tau1_q, &released, K_NO_WAIT);
}

static void tau2_timer_expiry(struct k_timer *timer)
{
	uint32_t released = k_cycle_get_32();

	ARG_UNUSED(timer);
	(void)k_msgq_put(&tau2_q, &released, K_NO_WAIT);
}

K_TIMER_DEFINE(tau1_timer, tau1_timer_expiry, NULL);
K_TIMER_DEFINE(tau2_timer, tau2_timer_expiry, NULL);

static void tau1_thread(void *p1, void *p2, void *p3)
{
	uint32_t next_deadline;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	next_deadline = first_release_cycle + k_us_to_cyc_ceil32(TAU1_T_US);

	while (true) {
		uint32_t released;

		/*
		 * El deadline se asigna antes de esperar la siguiente liberación.
		 * Primer job: deadline = t0 + 5 ms.
		 */
		k_thread_absolute_deadline_set(k_current_get(),
					       (int)next_deadline);

		k_msgq_get(&tau1_q, &released, K_FOREVER);
		preemptible_work(TAU1_C_US);

		if (k_cyc_to_us_floor32(k_cycle_get_32() - released) > TAU1_T_US) {
			atomic_inc(&tau1_misses);
		}

		next_deadline += k_us_to_cyc_ceil32(TAU1_T_US);
	}
}

static void tau2_thread(void *p1, void *p2, void *p3)
{
	uint32_t next_deadline;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	next_deadline = first_release_cycle + k_us_to_cyc_ceil32(TAU2_T_US);

	while (true) {
		uint32_t released;
		uint32_t response_us;

		/*
		 * Primer job: deadline = t0 + 7 ms.
		 * En t0 + 5 ms, EDF conserva tau2 ejecutándose porque 7 < 10.
		 */
		k_thread_absolute_deadline_set(k_current_get(),
					       (int)next_deadline);

		k_msgq_get(&tau2_q, &released, K_FOREVER);
		preemptible_work(TAU2_C_US);

		response_us = k_cyc_to_us_floor32(k_cycle_get_32() - released);

		if (response_us > TAU2_T_US) {
			atomic_inc(&tau2_misses);

			if (atomic_cas(&tau2_first_finish_us, 0, response_us)) {
				atomic_set(&tau2_first_deadline_us, TAU2_T_US);
			}
		}

		next_deadline += k_us_to_cyc_ceil32(TAU2_T_US);
	}
}

static void report_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sleep(K_SECONDS(1));

		printk("EDF: tau1_misses=%ld tau2_misses=%ld",
		       (long)atomic_get(&tau1_misses),
		       (long)atomic_get(&tau2_misses));

		if (atomic_get(&tau2_first_finish_us) != 0) {
			printk(" first_tau2_response=%ld us deadline=%ld us",
			       (long)atomic_get(&tau2_first_finish_us),
			       (long)atomic_get(&tau2_first_deadline_us));
		}

		printk("\n");
	}
}

K_THREAD_DEFINE(tau1_tid, 1024, tau1_thread,
		NULL, NULL, NULL, EDF_PRIO, 0, 0);

K_THREAD_DEFINE(tau2_tid, 1024, tau2_thread,
		NULL, NULL, NULL, EDF_PRIO, 0, 0);

K_THREAD_DEFINE(report_tid, 1024, report_thread,
		NULL, NULL, NULL, REPORT_PRIO, 0, 0);

int main(void)
{
	printk("Lab 6: Fig. 4.13, EDF\n");
	printk("tau1: C=2 ms T=5 ms, priority=%d\n", EDF_PRIO);
	printk("tau2: C=4 ms T=7 ms, priority=%d\n", EDF_PRIO);

	/* Ambas tareas tienen la misma primera liberación lógica. */
	first_release_cycle = k_cycle_get_32() +
		k_us_to_cyc_ceil32(50 * 1000U);

	k_timer_start(&tau1_timer, K_MSEC(50), K_USEC(TAU1_T_US));
	k_timer_start(&tau2_timer, K_MSEC(50), K_USEC(TAU2_T_US));

	return 0;
}
