#!/usr/bin/env python3
"""
Lab 6 - Response-Time Analysis (RTA)

Valores medidos:
- sampling: C = 12 us, T = 1000 us, prioridad alta.
- control:  C = 5 us,  T = D = 10000 us.
- B con mutex y herencia de prioridad: 3046 us.

El caso con k_sem no tiene una cota formal segura de bloqueo, porque
un semáforo no tiene dueño y no puede aplicar herencia de prioridad.
Se usa el bloqueo observado de 13086 us solo para comparar el experimento.
"""

from math import ceil


SAMPLE_C_US = 12
SAMPLE_T_US = 1000

CONTROL_C_US = 5
CONTROL_T_US = 10000
CONTROL_D_US = 10000

B_NONE_US = 0
B_SEM_OBSERVED_US = 13086
B_MUTEX_US = 3046


def response_time(c_us, deadline_us, blocking_us, higher_priority_tasks):
    """Itera R = C + B + sum(ceil(R/T_h) * C_h)."""
    response_us = c_us + blocking_us
    iterations = [response_us]

    while True:
        next_response_us = c_us + blocking_us

        for higher_c_us, higher_t_us in higher_priority_tasks:
            next_response_us += ceil(response_us / higher_t_us) * higher_c_us

        iterations.append(next_response_us)

        if next_response_us == response_us:
            return next_response_us, iterations, next_response_us <= deadline_us

        if next_response_us > deadline_us:
            return next_response_us, iterations, False

        response_us = next_response_us


def report_case(name, blocking_us):
    response_us, iterations, meets = response_time(
        CONTROL_C_US,
        CONTROL_D_US,
        blocking_us,
        [(SAMPLE_C_US, SAMPLE_T_US)],
    )

    verdict = "CUMPLE" if meets else "NO CUMPLE"

    print(f"\n{name}")
    print(f"  B = {blocking_us} us")
    print(f"  iteraciones R = {' -> '.join(map(str, iterations))} us")
    print(f"  R_control = {response_us} us")
    print(f"  deadline  = {CONTROL_D_US} us")
    print(f"  veredicto = {verdict}")


def main():
    print("Lab 6 - RTA de control")
    print(
        f"sampling: C={SAMPLE_C_US} us, T={SAMPLE_T_US} us; "
        f"control: C={CONTROL_C_US} us, D={CONTROL_D_US} us"
    )

    report_case("Sin contencion", B_NONE_US)

    report_case(
        "k_sem: bloqueo observado durante inversion "
        "(no es una cota formal segura)",
        B_SEM_OBSERVED_US,
    )

    report_case(
        "k_mutex con herencia de prioridad",
        B_MUTEX_US,
    )


if __name__ == "__main__":
    main()
