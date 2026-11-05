#!/usr/bin/env python3
"""
Teste físico direto via USB nativo (python-odrive) de UM eixo por vez.
Usa os seriais configurados em config/robot_4wd.yaml. Não salva nada na
ODrive (sem save_configuration) — ao final cada eixo volta para IDLE e
velocidade 0, sem deixar nenhum estado persistente alterado.

Uso:
    python3 test_usb_spin.py --check                  # só lê estado/erro, não move nada
    python3 test_usb_spin.py right 0 1.0               # ODrive direita, axis0, 1.0 rev/s, 2s
    python3 test_usb_spin.py left 1 -1.0 --duration 3  # ODrive esquerda, axis1, sentido contrário, 3s
"""
import argparse
import sys
import time

import odrive
from odrive.enums import AXIS_STATE_CLOSED_LOOP_CONTROL, AXIS_STATE_IDLE

SERIALS = {
    "right": "3553346E3034",
    "left": "3678387D3333",
}


def connect(label, timeout=5):
    odrv = odrive.find_any(serial_number=SERIALS[label], timeout=timeout)
    return odrv


def axis_status(label, odrv):
    lines = []
    for ax_name in ("axis0", "axis1"):
        ax = getattr(odrv, ax_name)
        lines.append(
            f"[{label}.{ax_name}] state={ax.current_state} error={ax.error} "
            f"encoder.error={ax.encoder.error} vel_estimate={ax.encoder.vel_estimate:.3f} rev/s"
        )
    return lines


def check():
    for label in SERIALS:
        try:
            odrv = connect(label)
            print(f"[{label}] serial={hex(odrv.serial_number)} vbus={odrv.vbus_voltage:.2f}V")
            for line in axis_status(label, odrv):
                print("  " + line)
        except Exception as e:
            print(f"[{label}] NAO ENCONTRADA: {e!r}")


def spin(label, axis_idx, vel, duration, clear_errors):
    odrv = connect(label)
    ax = getattr(odrv, f"axis{axis_idx}")

    if clear_errors:
        ax.clear_errors()
        time.sleep(0.1)

    if ax.error != 0:
        print(f"[{label}.axis{axis_idx}] ERRO ativo={ax.error}, abortando antes de mover "
              f"(use --clear-errors se for um erro latched esperado).")
        return

    try:
        ax.requested_state = AXIS_STATE_CLOSED_LOOP_CONTROL
        time.sleep(0.3)
        if ax.current_state != AXIS_STATE_CLOSED_LOOP_CONTROL:
            print(f"[{label}.axis{axis_idx}] NAO ENTROU em closed loop "
                  f"(state={ax.current_state}, error={ax.error}). Abortando.")
            return

        print(f"[{label}.axis{axis_idx}] girando a {vel} rev/s por {duration}s...")
        ax.controller.input_vel = vel
        t_end = time.time() + duration
        while time.time() < t_end:
            print(f"    vel_estimate={ax.encoder.vel_estimate:.3f} rev/s  error={ax.error}")
            time.sleep(0.3)
    finally:
        ax.controller.input_vel = 0.0
        time.sleep(0.1)
        ax.requested_state = AXIS_STATE_IDLE
        print(f"[{label}.axis{axis_idx}] parado e voltou para IDLE (state={ax.current_state}).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("label", nargs="?", choices=["right", "left"])
    parser.add_argument("axis", nargs="?", type=int, choices=[0, 1])
    parser.add_argument("vel", nargs="?", type=float, help="rev/s, pode ser negativo")
    parser.add_argument("--duration", type=float, default=2.0)
    parser.add_argument("--clear-errors", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    if args.check or args.label is None:
        check()
        return

    spin(args.label, args.axis, args.vel, args.duration, args.clear_errors)


if __name__ == "__main__":
    main()
