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

`tools/` tem os testes de giro, os scripts de diagnóstico do ODrive, as regras
de udev e a instalação do Jetson. Anotações antigas estão em `Documentacao/`.

## Estado atual

Compila, mas este workspace ainda não foi testado no robô.

- **CAN:** não rode `odom_node_4wd_can` antes de conferir a placa esquerda,
  trocada em 2026-10-26. O último registro dela mostra `can_node_id` 16 e 1, e
  o esperado é 2 e 3. O ID 1 colide com a roda dianteira direita.
- **Testes de desempenho:** `performance_logger` espera `drive/telemetry`, que
  o `odom_node_4wd` por USB não publica.
- **Mapas:** ficam em `~/maps`, fora do repositório. Os launches de localização
  usam `Lab_30032026.yaml` como padrão; se ele não existir, passe outro mapa.
- **ODrive:** depois de `save_configuration()`, chame `reboot()`. A placa nem
  sempre reinicia sozinha.

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

Depois, compile o `realsense-ros` no workspace, fora de `src/`. Evite o pacote
`apt` no Jetson: ele pode trazer outra `librealsense`.

```bash
cd ~/marsupial_fleet_ws
git clone --depth 1 --branch 4.58.2 https://github.com/IntelRealSense/realsense-ros.git external/realsense-ros
colcon build --packages-up-to realsense2_camera realsense2_description --cmake-args -DCMAKE_BUILD_TYPE=Release
```

O `setup_nuc.sh` instala a biblioteca por `apt` e vale para o NUC, não para o
Jetson.

## Instalação no Jetson

`tools/setup_jetson.sh` instala as dependências, compila a `librealsense` e
copia as regras de udev. Baixe antes o código da `librealsense` 2.58.2 em
`~/marsupial_fleet_ws/`.

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
