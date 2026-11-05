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
| `sllidar_ros2` | Driver do RPLIDAR A2, o lidar em uso (`/dev/rplidar`, tópico `/scan`) |
| `m1ct_d2` | Driver do lidar antigo, sem uso, mantido para voltar atrás se preciso |
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

## Dependências externas

A câmera RealSense D435i não está neste repositório. Ela usa `realsense-ros`
4.58.2 e `librealsense` 2.58.2. No Jetson, a `librealsense` foi compilada do
código-fonte com RSUSB e CUDA, para não precisar modificar o kernel:

```bash
cmake .. -DBUILD_EXAMPLES=OFF -DFORCE_RSUSB_BACKEND=ON -DBUILD_WITH_CUDA=ON
```

O `setup_nuc.sh` instala a biblioteca por `apt` e vale para o NUC, não para o
Jetson.

## Regras de udev

```bash
sudo cp tools/udev/*.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Teleoperação

```bash
source ~/marsupial_fleet_ws/install/setup.bash
ros2 launch marsupial_robot_base gamepad_teleop_4wd.launch.py
```
