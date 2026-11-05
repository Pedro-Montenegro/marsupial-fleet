# Marsupial Fleet

Software do Marsupial, robô 4WD skid-steer que funciona como base móvel e de
controle de uma frota autônoma: ele transporta um drone e um segundo robô e
coordena os três em missões de reconhecimento, resgate e supervisão.

Plataforma atual: Jetson Orin Nano, ROS 2 Humble, 2× ODrive v3.6 (fw 0.5.1).

## Pacotes

| Pacote | Função |
|---|---|
| `marsupial_robot_base` | Odometria, ODrive (USB e CAN), gamepad, calibração |
| `marsupial_robot_interfaces` | Mensagens e serviços |
| `marsupial_robot_localization` | EKF e AMCL |
| `marsupial_robot_navigation` | Nav2, mapeamento e missões |
| `marsupial_robot_sensors` | Bringup dos sensores |
| `marsupial_ui_bridge` | Ponte com a interface |
| `m1ct_d2`, `sllidar_ros2` | Drivers de lidar |
| `vl53l8cx_driver` | Driver do sensor VL53L8CX |

`tools/` tem os testes de giro e os scripts de diagnóstico do ODrive.

## Build

```bash
mkdir -p ~/marsupial_fleet_ws
git clone https://github.com/Pedro-Montenegro/marsupial-fleet.git ~/marsupial_fleet_ws/src
cd ~/marsupial_fleet_ws
source /opt/ros/humble/setup.bash
colcon build
```

A câmera RealSense não está neste repositório. Use `librealsense` 2.58.2 e
`realsense-ros` 4.58.2; `setup_nuc.sh` tem os passos de instalação do SDK.

## Teleoperação

```bash
source ~/marsupial_fleet_ws/install/setup.bash
ros2 launch marsupial_robot_base gamepad_teleop_4wd.launch.py
```
