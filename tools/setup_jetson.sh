#!/bin/bash
# Passos de instalação/compilação do workspace que exigem sudo.
# Rode este script diretamente no terminal do Jetson (NAO via chat):
#   bash ~/marsupial_fleet_ws/src/tools/setup_jetson.sh
# Antes, baixe o código da librealsense 2.58.2 em ~/marsupial_fleet_ws/.
set -e

WORKSPACE="$HOME/marsupial_fleet_ws"
ROS_SETUP="/opt/ros/humble/setup.bash"

echo "════════════════════════════════════════════════════════"
echo "  Marsupial Fleet — instalação (passos com sudo)"
echo "════════════════════════════════════════════════════════"

# ── [1] Pacotes apt que ainda faltam ──────────────────────────────────────────
echo ""
echo "=== [1/5] Instalando pacotes apt faltantes ==="
sudo apt-get update
sudo apt-get install -y \
  ros-humble-pointcloud-to-laserscan \
  libssl-dev libusb-1.0-0-dev libudev-dev \
  pkg-config cmake build-essential

# ── [2] librealsense2 SDK — build from source (RSUSB backend, sem patch de kernel) ──
echo ""
echo "=== [2/5] Compilando e instalando librealsense2 (source já presente) ==="
LRS_DIR="$WORKSPACE/librealsense-2.58.2"
[ -d "$LRS_DIR" ] || { echo "Faltando: $LRS_DIR"; exit 1; }
cd "$LRS_DIR"
# regras udev da RealSense (sem tocar no kernel)
sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/
sudo cp config/99-realsense-d4xx-mipi-dfu.rules /etc/udev/rules.d/ 2>/dev/null || true

export PATH="/usr/local/cuda/bin:$PATH"
CUDA_FLAG="-DBUILD_WITH_CUDA=OFF"
if command -v nvcc >/dev/null 2>&1; then
  CUDA_FLAG="-DBUILD_WITH_CUDA=ON -DCMAKE_CUDA_COMPILER=$(command -v nvcc)"
fi

mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DFORCE_RSUSB_BACKEND=ON \
  $CUDA_FLAG \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_GRAPHICAL_EXAMPLES=OFF
make -j$(($(nproc) - 1))
sudo make install
sudo ldconfig

# ── [3] regras udev (RPLIDAR, ODrive e gamepad) ───────────────────────────────
echo ""
echo "=== [3/5] Configurando regras udev ==="
sudo cp "$WORKSPACE"/src/tools/udev/*.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger

# ── [4] rosdep ─────────────────────────────────────────────────────────────────
echo ""
echo "=== [4/5] rosdep install ==="
source "$ROS_SETUP"
sudo rosdep init 2>/dev/null || true
rosdep update
rosdep install --from-paths "$WORKSPACE/src" --ignore-src -y \
  --skip-keys "librealsense2"

# ── [5] pip (não precisa sudo, mas fica aqui pra rodar tudo de uma vez) ───────
# Versões fixas: odrive mais novo pode não funcionar com o firmware 0.5.1.
echo ""
echo "=== [5/5] pip install odrive, python-can e cantools ==="
pip3 install --user odrive==0.5.4 python-can==4.6.1 cantools==41.0.2

echo ""
echo "════════════════════════════════════════════════════════"
echo "  Passos com sudo concluídos."
echo "  Agora rode o colcon build no workspace."
echo "════════════════════════════════════════════════════════"
