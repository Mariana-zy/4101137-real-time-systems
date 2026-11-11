#!/usr/bin/env python3
"""
Lab 6 - Utilizacion y Response-Time Analysis (RTA).

IMPORTANTE:
Los parametros de Daniela no fueron entregados al grupo.
Se usan supuestos documentados para completar el escenario de analisis:
  sensor A: C=2400 us, T=D=8000 us
  sensor B: C=8000 us, T=D=20000 us
"""

from dataclasses import dataclass
from math import ceil, prod


@dataclass(frozen=True)
class Task:
    name: str
    c_us: int
    t_us: int
    d_us: int
    b_us: int = 0


# Tareas periodicas medidas en la ESP32-C6.
sampling = Task("sampling", c_us=12, t_us=1000, d_us=1000)

# Supuestos: parametros de Daniela no entregados en clase.
sensor_a = Task("sensor Daniela A (supuesto)",
                c_us=2400, t_us=8000, d_us=8000)

control = Task("control", c_us=5, t_us=10000, d_us=10000,
               b_us=3046)

sensor_b = Task("sensor Daniela B (supuesto)",
                c_us=8000, t_us=20000, d_us=20000)

telemetry = Task("telemetry", c_us=165,
                 t_us=1_000_000, d_us=1_000_000)

base_tasks = [sampling, control, telemetry]
all_tasks = [sampling, sensor_a, control, sensor_b, telemetry]


def utilization(tasks):
    return sum(task.c_us / task.t_us for task in tasks)


def liu_layland_bound(number_of_tasks):
    return number_of_tasks * (2 ** (1 / number_of_tasks) - 1)


def hyperbolic_product(tasks):
    return prod(1 + task.c_us / task.t_us for task in tasks)


def response_time(task, higher_priority_tasks):
    """R = C + B + sum(ceil(R / T_h) * C_h)."""
    response = task.c_us + task.b_us
    iterations = [response]

    while True:
        next_response = task.c_us + task.b_us

        for higher in higher_priority_tasks:
            next_response += ceil(response / higher.t_us) * higher.c_us

        iterations.append(next_response)

        if next_response == response:
            return next_response, iterations, next_response <= task.d_us

        if next_response > task.d_us:
            return next_response, iterations, False

        response = next_response


def print_utilization(name, tasks):
    u_value = utilization(tasks)
    ll_bound = liu_layland_bound(len(tasks))
    hyperbolic = hyperbolic_product(tasks)

    print(f"\n{name}")
    print(f"  U = {u_value:.6f} ({u_value * 100:.4f} %)")
    print(f"  Liu-Layland = {ll_bound:.6f}")
    print(f"  Hiperbolico = {hyperbolic:.6f}")

    print("  RM por Liu-Layland:",
          "GARANTIZADO" if u_value <= ll_bound else "NO GARANTIZADO")
    print("  RM por test hiperbolico:",
          "GARANTIZADO" if hyperbolic <= 2 else "NO GARANTIZADO")


def print_rta(tasks):
    print("\nRTA bajo RM con mutex e herencia de prioridad")

    ordered_tasks = sorted(tasks, key=lambda task: task.t_us)

    for index, task in enumerate(ordered_tasks):
        higher_priority = ordered_tasks[:index]
        response, iterations, meets = response_time(task, higher_priority)

        print(f"\n  {task.name}")
        print(f"    C={task.c_us} us, T={task.t_us} us, "
              f"D={task.d_us} us, B={task.b_us} us")
        print("    R = " + " -> ".join(map(str, iterations)) + " us")
        print(f"    Resultado: R={response} us "
              f"{'CUMPLE' if meets else 'NO CUMPLE'}")


def main():
    print("Lab 6 - Analisis de planificabilidad")
    print("Nota: las dos tareas de Daniela son supuestos documentados.")

    print_utilization("Nodo base medido", base_tasks)
    print_utilization("Nodo con tareas de Daniela supuestas", all_tasks)
    print_rta(all_tasks)


if __name__ == "__main__":
    main()
