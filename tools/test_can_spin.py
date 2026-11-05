#!/usr/bin/env python3
"""
Teste físico direto via CAN (CANSimple) de UMA roda por vez.
Não depende de ROS — serve para validar fiação/CAN/node_id antes de subir o odom_node.

node_id por roda (ver config/robot_4wd_can.yaml):
    0 = traseira direita    1 = dianteira direita
    2 = dianteira esquerda  3 = traseira esquerda

Uso:
    # 1) só checar heartbeat (erro, estado) das quatro rodas, sem mover nada
    python3 test_can_spin.py --check

    # 2) girar a roda 0 (traseira direita) a 1.0 rev/s por 2s
    python3 test_can_spin.py 0 1.0

    # 3) girar a roda 3 (traseira esquerda) a -1.0 rev/s por 3s
    python3 test_can_spin.py 3 -1.0 --duration 3
"""
import argparse
import sys
import time
from pathlib import Path

import can
import cantools

DBC_PATH = str(Path(__file__).resolve().parents[1] / "marsupial_robot_base" / "odrive-cansimple.dbc")
CLOSED_LOOP_CONTROL = 8
IDLE = 1
VELOCITY_CONTROL = 2
PASSTHROUGH = 1
NODE_IDS = (0, 1, 2, 3)


def send(bus, db, name, axis, **signals):
    msg = db.get_message_by_name(f"Axis{axis}_{name}")
    data = msg.encode(signals) if signals else b""
    bus.send(can.Message(arbitration_id=msg.frame_id, data=data, is_extended_id=False))


def read_heartbeat(bus, db, axis, timeout=1.0):
    hb_msg = db.get_message_by_name(f"Axis{axis}_Heartbeat")
    deadline = time.time() + timeout
    while time.time() < deadline:
        frame = bus.recv(timeout=0.1)
        if frame is not None and frame.arbitration_id == hb_msg.frame_id:
            decoded = hb_msg.decode(frame.data)
            return decoded
    return None


def check(bus, db):
    ok = True
    for axis in NODE_IDS:
        hb = read_heartbeat(bus, db, axis, timeout=1.5)
        if hb is None:
            print(f"[axis {axis}] SEM heartbeat — verifique fiação/terminação/node_id no ODrive.")
            ok = False
            continue
        state = hb["Axis_State"]
        err = hb["Axis_Error"]
        print(f"[axis {axis}] Axis_State={state}  Axis_Error={err}")
        if str(err) not in ("0", "NONE"):
            ok = False
    return ok


def spin(bus, db, axis, vel, duration, clear_errors):
    if clear_errors:
        send(bus, db, "Clear_Errors", axis)
        time.sleep(0.1)

    hb = read_heartbeat(bus, db, axis, timeout=1.0)
    if hb is None:
        print(f"[axis {axis}] ERRO: nenhum heartbeat recebido, abortando antes de mover.")
        return
    print(f"[axis {axis}] estado antes: Axis_State={hb['Axis_State']} Axis_Error={hb['Axis_Error']}")

    try:
        send(bus, db, "Set_Axis_State", axis, Axis_Requested_State=CLOSED_LOOP_CONTROL)
        time.sleep(0.3)
        send(bus, db, "Set_Controller_Mode", axis, Control_Mode=VELOCITY_CONTROL, Input_Mode=PASSTHROUGH)
        time.sleep(0.05)

        print(f"[axis {axis}] girando a {vel} rev/s por {duration}s...")
        t_end = time.time() + duration
        while time.time() < t_end:
            send(bus, db, "Set_Input_Vel", axis, Input_Vel=vel, Input_Torque_FF=0.0)
            time.sleep(0.05)
    finally:
        send(bus, db, "Set_Input_Vel", axis, Input_Vel=0.0, Input_Torque_FF=0.0)
        time.sleep(0.1)
        send(bus, db, "Set_Axis_State", axis, Axis_Requested_State=IDLE)
        print(f"[axis {axis}] parado e voltou para IDLE.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("axis", type=int, nargs="?", choices=NODE_IDS, help="node_id da roda: 0=RR, 1=FR, 2=FL, 3=RL")
    parser.add_argument("vel", type=float, nargs="?", help="velocidade em rev/s (pode ser negativa)")
    parser.add_argument("--duration", type=float, default=2.0, help="duração em segundos (default 2.0)")
    parser.add_argument("--channel", default="can0")
    parser.add_argument("--dbc", default=DBC_PATH)
    parser.add_argument("--check", action="store_true", help="só lê heartbeat das 4 rodas, não move nada")
    parser.add_argument("--clear-errors", action="store_true", help="limpa erros acumulados antes de girar")
    args = parser.parse_args()
    if args.axis is not None and args.vel is None and not args.check:
        parser.error("informe a velocidade em rev/s")

    db = cantools.database.load_file(args.dbc)
    bus = can.interface.Bus(channel=args.channel, interface="socketcan")

    try:
        if args.check or args.axis is None:
            ok = check(bus, db)
            sys.exit(0 if ok else 1)
        spin(bus, db, args.axis, args.vel, args.duration, args.clear_errors)
    finally:
        bus.shutdown()


if __name__ == "__main__":
    main()
