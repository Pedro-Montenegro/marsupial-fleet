// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/point_cloud2.hpp>
// #include <sensor_msgs/msg/laser_scan.hpp>
// #include <sensor_msgs/point_cloud2_iterator.hpp>
// #include <std_msgs/msg/int32_multi_array.hpp>

// #include <gpiod.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/ioctl.h>
// #include <linux/i2c-dev.h>
// #include <cmath>
// #include <vector>
// #include <limits>
// #include <array>

// extern "C" {
// #include "vl53l8cx_api.h"
// #include "platform.h"
// }

// #define SENSOR_COUNT    3
// #define GRID_W          8
// #define GRID_H          8
// #define GRID            (GRID_W * GRID_H)
// #define FOV_DEG         63.0f
// #define FOV_RAD         (FOV_DEG * static_cast<float>(M_PI) / 180.0f)
// #define TARGET_RAYS     32
// #define REMAP_RETRIES   3
// #define I2C_DEV         "/dev/i2c-1"

// class VL53L8CXNode : public rclcpp::Node
// {
// public:

//     VL53L8CXNode() : Node("vl53l8cx_node")
//     {
//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             pc_pub[i] = create_publisher<sensor_msgs::msg::PointCloud2>(
//                 "/vl53/sensor" + std::to_string(i) + "/points", rclcpp::SensorDataQoS());

//             scan_pub[i] = create_publisher<sensor_msgs::msg::LaserScan>(
//                 "/vl53/sensor" + std::to_string(i) + "/scan", rclcpp::SensorDataQoS());

//             raw_pub[i] = create_publisher<std_msgs::msg::Int32MultiArray>(
//                 "/vl53/sensor" + std::to_string(i) + "/raw_matrix", 10);
//         }

//         if (!init_gpios())
//         {
//             RCLCPP_FATAL(get_logger(), "Falha GPIO. Verifique as permissões (sudo ou grupo gpio).");
//             throw std::runtime_error("GPIO init failed");
//         }

//         init_sensors();

//         // Timer rodando a ~30Hz (suficiente para ler sensores a 10Hz ou 15Hz sem gargalo)
//         timer = create_wall_timer(
//             std::chrono::milliseconds(33),
//             std::bind(&VL53L8CXNode::update, this));

//         RCLCPP_INFO(get_logger(), "VL53L8CX driver iniciado (%d sensores)", SENSOR_COUNT);
//     }

//     ~VL53L8CXNode()
//     {
//         for (int i = 0; i < SENSOR_COUNT; i++)
//             vl53l8cx_stop_ranging(&Dev[i]);

//         if (i2c_fd >= 0)
//             close(i2c_fd);

//         for (int i = 0; i < SENSOR_COUNT; i++)
//             if (lines[i]) gpiod_line_release(lines[i]);

//         if (chip)
//             gpiod_chip_close(chip);
//     }

// private:

//     VL53L8CX_Configuration  Dev[SENSOR_COUNT];
//     VL53L8CX_ResultsData    Results[SENSOR_COUNT];

//     struct gpiod_chip *chip   = nullptr;
//     struct gpiod_line *lines[SENSOR_COUNT] = {};

//     int i2c_fd = -1;

//     // Offsets de GPIO (LPn) e endereços I2C de destino para o seu hardware
//     int     lpn_offsets[SENSOR_COUNT]  = {144, 50, 85};
//     uint8_t sensor_addr[SENSOR_COUNT]  = {0x30, 0x2A, 0x28};

//     rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pc_pub[SENSOR_COUNT];
//     rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr   scan_pub[SENSOR_COUNT];
//     rclcpp::Publisher<std_msgs::msg::Int32MultiArray>::SharedPtr raw_pub[SENSOR_COUNT];
//     rclcpp::TimerBase::SharedPtr timer;

//     /* ================================================================
//      * INICIALIZAÇÃO DE GPIO
//      * ================================================================ */

//     bool init_gpios()
//     {
//         chip = gpiod_chip_open_by_name("gpiochip0");
//         if (!chip) return false;

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             lines[i] = gpiod_chip_get_line(chip, lpn_offsets[i]);
//             if (!lines[i]) return false;

//             // Força LOW (reset) em todos os sensores imediatamente
//             if (gpiod_line_request_output(lines[i], "vl53_lpn", 0) < 0)
//             {
//                 RCLCPP_ERROR(get_logger(), "Pino %d ocupado", lpn_offsets[i]);
//                 return false;
//             }
//         }

//         sleep(1); // Aguarda descarga total dos capacitores na placa
//         return true;
//     }

//     /* ================================================================
//      * INICIALIZAÇÃO DOS SENSORES (Gestão do Barramento I2C)
//      * ================================================================ */

//     void init_sensors()
//     {
//         i2c_fd = open(I2C_DEV, O_RDWR);
//         if (i2c_fd < 0)
//         {
//             RCLCPP_FATAL(get_logger(), "Falha ao abrir %s", I2C_DEV);
//             throw std::runtime_error("I2C open failed");
//         }

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             // Para ranging dos sensores já ativos para não poluir o barramento I2C
//             // durante o download de firmware do próximo sensor
//             for (int prev = 0; prev < i; prev++)
//                 vl53l8cx_stop_ranging(&Dev[prev]);

//             if (!init_single_sensor(i))
//             {
//                 RCLCPP_FATAL(get_logger(), "Falha ao inicializar sensor %d", i);
//                 throw std::runtime_error("Sensor init failed");
//             }

//             // Retoma ranging dos sensores anteriores
//             for (int prev = 0; prev < i; prev++)
//                 vl53l8cx_start_ranging(&Dev[prev]);
//         }
//     }

//     bool init_single_sensor(int idx)
//     {
//         RCLCPP_INFO(get_logger(), "[%d/%d] Ativando sensor %d...", idx + 1, SENSOR_COUNT, idx);

//         // Liga o sensor atual (LPn = HIGH)
//         gpiod_line_set_value(lines[idx], 1);
//         usleep(500000);

//         Dev[idx].platform.fd      = i2c_fd;
//         Dev[idx].platform.address = 0x29; // Endereço I2C padrão de fábrica
//         ioctl(i2c_fd, I2C_SLAVE, 0x29);

//         // Processo de Remap de Endereço com Retry
//         bool alive = false;
//         for (int retry = 0; retry < REMAP_RETRIES; retry++)
//         {
//             vl53l8cx_set_i2c_address(&Dev[idx], sensor_addr[idx] << 1);
//             usleep(150000);

//             Dev[idx].platform.address = sensor_addr[idx];
//             ioctl(i2c_fd, I2C_SLAVE, sensor_addr[idx]);

//             uint8_t is_alive = 0;
//             uint8_t status   = vl53l8cx_is_alive(&Dev[idx], &is_alive);

//             if (status == 0 && is_alive) { alive = true; break; }

//             RCLCPP_WARN(get_logger(), "  Retry remap sensor %d (%d/%d)", idx, retry + 1, REMAP_RETRIES);

//             // Pulso de reset no hardware se falhar
//             gpiod_line_set_value(lines[idx], 0);
//             usleep(200000);
//             gpiod_line_set_value(lines[idx], 1);
//             usleep(500000);

//             Dev[idx].platform.address = 0x29;
//             ioctl(i2c_fd, I2C_SLAVE, 0x29);
//         }

//         if (!alive)
//         {
//             RCLCPP_ERROR(get_logger(), "Sensor %d não responde em 0x%02X", idx, sensor_addr[idx]);
//             return false;
//         }

//         // Envia o firmware base
//         uint8_t status = vl53l8cx_init(&Dev[idx]);
//         if (status)
//         {
//             RCLCPP_ERROR(get_logger(), "vl53l8cx_init falhou: sensor %d, status %d", idx, status);
//             return false;
//         }

//         /* ---------------------------------------------------------
//          * CONFIGURAÇÃO DE ALCANCE (4 METROS)
//          * --------------------------------------------------------- */
//         vl53l8cx_set_resolution(&Dev[idx], VL53L8CX_RESOLUTION_8X8);
        
//         // Modo Autônomo permite melhor gestão de energia para pulsos longos
//         vl53l8cx_set_ranging_mode(&Dev[idx], VL53L8CX_RANGING_MODE_AUTONOMOUS);
        
//         // Frequência reduzida para 10Hz (necessário para ler 4m em 8x8)
//         // Se o robô estiver muito rápido e colidindo, suba para 15Hz (perde alcance)
//         vl53l8cx_set_ranging_frequency_hz(&Dev[idx], 10);
        
//         // Tempo de integração (obturador) aumentado para captar ecos fracos de longe
//         vl53l8cx_set_integration_time_ms(&Dev[idx], 20);

//         vl53l8cx_start_ranging(&Dev[idx]);

//         RCLCPP_INFO(get_logger(), "Sensor %d pronto em 0x%02X", idx, sensor_addr[idx]);
//         return true;
//     }

//     /* ================================================================
//      * CONVERSÃO DE DADOS: PointCloud2
//      * ================================================================ */

//     void build_cloud(int s, sensor_msgs::msg::PointCloud2 & msg)
//     {
//         std::vector<std::array<float, 3>> points;
//         points.reserve(GRID);

//         for (int row = 0; row < GRID_H; row++)
//         {
//             for (int col = 0; col < GRID_W; col++)
//             {
//                 float d = Results[s].distance_mm[row * GRID_W + col] / 1000.0f;

//                 // Ignora leituras fora do range seguro
//                 if (d <= 0.02f || d > 4.0f) continue;

//                 // Cálculo angular distribuindo os pontos no FOV
//                 float ax = (col - (GRID_W - 1) / 2.0f) * (FOV_RAD / (GRID_W - 1));
//                 float az = (row - (GRID_H - 1) / 2.0f) * (FOV_RAD / (GRID_H - 1));

//                 float X =  d;
//                 float Y = -d * std::tan(ax); // -Y no ROS = Direita
//                 float Z = -d * std::tan(az); // -Z no ROS = Para baixo

//                 points.push_back({X, Y, Z});
//             }
//         }

//         msg.height = 1;
//         msg.width  = static_cast<uint32_t>(points.size());
//         msg.is_dense = false;

//         sensor_msgs::PointCloud2Modifier mod(msg);
//         mod.setPointCloud2FieldsByString(1, "xyz");
//         mod.resize(points.size());

//         sensor_msgs::PointCloud2Iterator<float> ix(msg, "x");
//         sensor_msgs::PointCloud2Iterator<float> iy(msg, "y");
//         sensor_msgs::PointCloud2Iterator<float> iz(msg, "z");

//         for (auto & p : points)
//         {
//             *ix = p[0]; *iy = p[1]; *iz = p[2];
//             ++ix; ++iy; ++iz;
//         }
//     }

//     /* ================================================================
//      * CONVERSÃO DE DADOS: LaserScan 1D (Para VFH+ / Nav2)
//      * ================================================================ */

//     /* ================================================================
//      * LASER SCAN 1D (Fatia Central - 8 Raios)
//      * ================================================================ */
//     /* ================================================================
//      * LASER SCAN 1D (Fatia Central - 8 Raios COM FILTRO DE STATUS)
//      * ================================================================ */
//     void build_scan(int s, sensor_msgs::msg::LaserScan & scan)
//     {
//         scan.ranges.resize(GRID_W);

//         for (int col = 0; col < GRID_W; col++)
//         {
//             // Índices das linhas centrais na matriz linear de 64 posições
//             int idx3 = 3 * GRID_W + col;
//             int idx4 = 4 * GRID_W + col;

//             // Extrai as distâncias
//             float d3 = Results[s].distance_mm[idx3] / 1000.0f;
//             float d4 = Results[s].distance_mm[idx4] / 1000.0f;

//             // Extrai o STATUS de confiança de cada pixel
//             uint8_t status3 = Results[s].target_status[idx3];
//             uint8_t status4 = Results[s].target_status[idx4];

//             // Variáveis para guardar os valores válidos
//             std::vector<float> valid_distances;

//             // A MÁGICA ACONTECE AQUI: Só aceita se o status for confiável (5, 6 ou 9)
//             if ((status3 == 5 || status3 == 6 || status3 == 9) && (d3 > 0.02f && d3 <= 4.0f)) {
//                 valid_distances.push_back(d3);
//             }
//             if ((status4 == 5 || status4 == 6 || status4 == 9) && (d4 > 0.02f && d4 <= 4.0f)) {
//                 valid_distances.push_back(d4);
//             }

//             // Define o que publicar para este raio (coluna)
//             if (valid_distances.empty()) {
//                 // Se nenhum pixel for válido, a área está limpa (Infinito apaga o obstáculo no Nav2)
//                 scan.ranges[col] = std::numeric_limits<float>::infinity();
//             } else {
                
//                 /* ESCOLHA SUA ESTRATÉGIA AQUI: */

//                 // Opção A: Pegar o MENOR valor (Mais seguro, não ignora pés de mesa) - RECOMENDADO
//                 float chosen_d = std::numeric_limits<float>::infinity();
//                 for(float v : valid_distances) chosen_d = std::min(chosen_d, v);
                
//                 // Opção B: Pegar o MAIOR valor (O que você pediu) - DESCOMENTE ABAIXO SE QUISER USAR
//                 /*
//                 float chosen_d = -1.0f;
//                 for(float v : valid_distances) chosen_d = std::max(chosen_d, v);
//                 */

//                 scan.ranges[col] = chosen_d;
//             }
//         }

//         scan.angle_min       = -FOV_RAD / 2.0f;
//         scan.angle_max       =  FOV_RAD / 2.0f;
//         scan.angle_increment =  FOV_RAD / (GRID_W - 1.0f); 
//         scan.range_min       = 0.02f;
//         scan.range_max       = 4.0f; 
//     }

//     /* ================================================================
//      * LOOP PRINCIPAL DE LEITURA
//      * ================================================================ */

//     void update()
//     {
//         for (int s = 0; s < SENSOR_COUNT; s++)
//         {
//             uint8_t ready = 0;
//             vl53l8cx_check_data_ready(&Dev[s], &ready);
//             if (!ready) continue;

//             vl53l8cx_get_ranging_data(&Dev[s], &Results[s]);

//             const std::string frame_id = "vl53l8cx_" + std::to_string(s) + "_link";
//             auto stamp = now();
            
//             // 1. Matriz Bruta (Int32MultiArray)
//             std_msgs::msg::Int32MultiArray raw_msg;
//             raw_msg.data.resize(GRID); 
//             for (int i = 0; i < GRID; i++)
//             {
//                 raw_msg.data[i] = Results[s].distance_mm[i];
//             }
//             raw_pub[s]->publish(raw_msg);

//             // 2. PointCloud2 (Visualização 3D detalhada)
//             sensor_msgs::msg::PointCloud2 cloud;
//             cloud.header.stamp    = stamp;
//             cloud.header.frame_id = frame_id;
//             build_cloud(s, cloud);
//             pc_pub[s]->publish(cloud);

//             // 3. LaserScan (Para uso com Navigation2 ou VFH+)
//             sensor_msgs::msg::LaserScan scan;
//             scan.header.stamp    = stamp;
//             scan.header.frame_id = frame_id;
//             build_scan(s, scan);
//             scan_pub[s]->publish(scan);
//         }
//     }
// };

// int main(int argc, char ** argv)
// {
//     rclcpp::init(argc, argv);
//     auto node = std::make_shared<VL53L8CXNode>();
//     rclcpp::spin(node);
//     rclcpp::shutdown();
//     return 0;
// }




// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/laser_scan.hpp>
// #include <gpiod.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/ioctl.h>
// #include <linux/i2c-dev.h>
// #include <cmath>
// #include <limits>

// extern "C" {
// #include "vl53l8cx_api.h"
// #include "platform.h"
// }

// #define SENSOR_COUNT 3
// #define GRID_W 8
// #define GRID_H 8
// #define GRID (GRID_W * GRID_H)

// #define FOV_DEG 63.0f
// #define FOV_RAD (FOV_DEG * M_PI / 180.0f)

// #define I2C_DEV "/dev/i2c-1"

// class VL53FastNode : public rclcpp::Node
// {
// public:
//     VL53FastNode() : Node("vl53_fast_node")
//     {
        
//         // Pré-calcula tangentes
//         for (int i = 0; i < GRID_W; i++)
//         {
//             float ax = (i - (GRID_W - 1) / 2.0f) * (FOV_RAD / (GRID_W - 1));
//             tan_x[i] = std::tan(ax);
//         }

//         // Publishers + mensagens pré-alocadas
//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             scan_pub[i] = create_publisher<sensor_msgs::msg::LaserScan>(
//                 "/vl53/sensor" + std::to_string(i) + "/scan",
//                 rclcpp::SensorDataQoS());

//             scan_msg[i].ranges.resize(GRID_W);

//             scan_msg[i].angle_min = -FOV_RAD / 2.0f;
//             scan_msg[i].angle_max =  FOV_RAD / 2.0f;
//             scan_msg[i].angle_increment = FOV_RAD / (GRID_W - 1);
//             scan_msg[i].range_min = 0.02f;
//             scan_msg[i].range_max = 4.0f;
//         }

//         init_gpios();
//         init_sensors();

//         timer = create_wall_timer(
//             std::chrono::milliseconds(100),  // 10 Hz
//             std::bind(&VL53FastNode::update, this));
//     }

// private:

//     VL53L8CX_Configuration Dev[SENSOR_COUNT];
//     VL53L8CX_ResultsData Results[SENSOR_COUNT];

//     rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub[SENSOR_COUNT];
//     sensor_msgs::msg::LaserScan scan_msg[SENSOR_COUNT];

//     float tan_x[GRID_W];

//     rclcpp::TimerBase::SharedPtr timer;

//     int i2c_fd;
//     struct gpiod_chip *chip;
//     struct gpiod_line *lines[SENSOR_COUNT];

//     int lpn_offsets[SENSOR_COUNT] = {144, 50, 85};
//     uint8_t sensor_addr[SENSOR_COUNT] = {0x30, 0x2A, 0x28};

//     /* ================= GPIO ================= */

//     void init_gpios()
//     {
//         chip = gpiod_chip_open_by_name("gpiochip0");

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             lines[i] = gpiod_chip_get_line(chip, lpn_offsets[i]);
//             gpiod_line_request_output(lines[i], "vl53", 0);
//         }

//         sleep(1);
//     }

//     /* ================= SENSORES ================= */

//     void init_sensors()
//     {
//         i2c_fd = open(I2C_DEV, O_RDWR);
//         if (i2c_fd < 0)
//         {
//             RCLCPP_FATAL(get_logger(), "Falha ao abrir %s", I2C_DEV);
//             throw std::runtime_error("I2C open failed");
//         }

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             RCLCPP_INFO(get_logger(),
//             "[%d/%d] Inicializando sensor %d...",
//             i + 1, SENSOR_COUNT, i);

//             gpiod_line_set_value(lines[i], 1);
//             usleep(500000);

//             Dev[i].platform.fd = i2c_fd;
//             Dev[i].platform.address = 0x29;
//             ioctl(i2c_fd, I2C_SLAVE, 0x29);

//             vl53l8cx_set_i2c_address(&Dev[i], sensor_addr[i] << 1);

//             Dev[i].platform.address = sensor_addr[i];
//             ioctl(i2c_fd, I2C_SLAVE, sensor_addr[i]);

//             uint8_t is_alive = 0;
//             vl53l8cx_is_alive(&Dev[i], &is_alive);

//             if (!is_alive)
//             {
//                 RCLCPP_ERROR(get_logger(),
//                     "Sensor %d NÃO respondeu em 0x%02X",
//                     i, sensor_addr[i]);
//                 throw std::runtime_error("Sensor not detected");
//             }

//             RCLCPP_INFO(get_logger(),
//                 "Sensor %d detectado com sucesso!", i);

//             uint8_t status = vl53l8cx_init(&Dev[i]);

//             if (status)
//             {
//                 RCLCPP_ERROR(get_logger(),
//                     "Erro no init do sensor %d (status %d)",
//                     i, status);
//                 throw std::runtime_error("Init failed");
//             }

//             vl53l8cx_set_resolution(&Dev[i], VL53L8CX_RESOLUTION_8X8);
//             vl53l8cx_set_ranging_frequency_hz(&Dev[i], 10);
//             vl53l8cx_start_ranging(&Dev[i]);

//             RCLCPP_INFO(get_logger(),
//             "Sensor %d pronto em 0x%02X",
//             i, sensor_addr[i]);
//         }
//         RCLCPP_INFO(get_logger(), "Todos os sensores inicializados com sucesso!");
//     }

//     /* ================= BUILD SCAN ================= */

//     inline void build_scan(int s)
//     {
//         auto &scan = scan_msg[s];

//         for (int col = 0; col < GRID_W; col++)
//         {
//             int idx3 = 3 * GRID_W + col;
//             int idx4 = 4 * GRID_W + col;

//             float d3 = Results[s].distance_mm[idx3] * 0.001f;
//             float d4 = Results[s].distance_mm[idx4] * 0.001f;

//             uint8_t st3 = Results[s].target_status[idx3];
//             uint8_t st4 = Results[s].target_status[idx4];

//             float best = std::numeric_limits<float>::infinity();

//             if ((st3 == 5 || st3 == 6 || st3 == 9) && d3 > 0.02f && d3 <= 4.0f)
//                 best = d3;

//             if ((st4 == 5 || st4 == 6 || st4 == 9) && d4 > 0.02f && d4 <= 4.0f)
//                 best = (best < d4) ? best : d4;

//             scan.ranges[col] = best;
//         }
//     }

//     /* ================= LOOP ================= */

//     void update()
//     {
//         auto stamp = now();

//         for (int s = 0; s < SENSOR_COUNT; s++)
//         {
//             uint8_t ready = 0;
//             vl53l8cx_check_data_ready(&Dev[s], &ready);

//             if (!ready) continue;

//             vl53l8cx_get_ranging_data(&Dev[s], &Results[s]);

//             build_scan(s);

//             scan_msg[s].header.stamp = stamp;
//             scan_msg[s].header.frame_id = "vl53_" + std::to_string(s);

//             scan_pub[s]->publish(scan_msg[s]);
//         }
//     }
// };

// int main(int argc, char ** argv)
// {
//     rclcpp::init(argc, argv);

//     auto node = std::make_shared<VL53FastNode>();

//     rclcpp::executors::MultiThreadedExecutor exec;
//     exec.add_node(node);
//     exec.spin();

//     rclcpp::shutdown();
//     return 0;
// }






#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <gpiod.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cmath>
#include <limits>
#include <thread>
#include <mutex>
#include <atomic>

extern "C" {
#include "vl53l8cx_api.h"
#include "platform.h"
}

#define SENSOR_COUNT 3
#define GRID_W 8
#define GRID_H 8
#define GRID (GRID_W * GRID_H)

#define FOV_DEG 63.0f
#define FOV_RAD (FOV_DEG * M_PI / 180.0f)

#define I2C_DEV "/dev/i2c-1"

class VL53ThreadedNode : public rclcpp::Node
{
public:
    VL53ThreadedNode() : Node("vl53_threaded_node")
    {
        running = true;

        for (int i = 0; i < GRID_W; i++)
        {
            float ax = (i - (GRID_W - 1) / 2.0f) * (FOV_RAD / (GRID_W - 1));
            tan_x[i] = std::tan(ax);
        }

        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            scan_pub[i] = create_publisher<sensor_msgs::msg::LaserScan>(
                "/vl53/sensor" + std::to_string(i) + "/scan",
                rclcpp::SensorDataQoS().keep_last(1).best_effort());

            scan_msg[i].ranges.resize(GRID_W);

            scan_msg[i].angle_min = -FOV_RAD / 2.0f;
            scan_msg[i].angle_max =  FOV_RAD / 2.0f;
            scan_msg[i].angle_increment = FOV_RAD / (GRID_W - 1);
            scan_msg[i].range_min = 0.2f;
            scan_msg[i].range_max = 4.0f;
        }

        init_gpios();
        init_sensors();

        // cria threads
        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            threads[i] = std::thread(&VL53ThreadedNode::sensor_loop, this, i);
        }
    }

    ~VL53ThreadedNode()
    {
        running = false;

        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            if (threads[i].joinable())
                threads[i].join();

            vl53l8cx_stop_ranging(&Dev[i]);
        }

        if (i2c_fd >= 0) close(i2c_fd);
    }

private:

    VL53L8CX_Configuration Dev[SENSOR_COUNT];
    VL53L8CX_ResultsData Results[SENSOR_COUNT];

    rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub[SENSOR_COUNT];
    sensor_msgs::msg::LaserScan scan_msg[SENSOR_COUNT];

    float tan_x[GRID_W];

    std::thread threads[SENSOR_COUNT];
    std::atomic<bool> running;

    std::mutex i2c_mutex;

    int i2c_fd;
    struct gpiod_chip *chip;
    struct gpiod_line *lines[SENSOR_COUNT];

    int lpn_offsets[SENSOR_COUNT] = {144, 50, 85};
    uint8_t sensor_addr[SENSOR_COUNT] = {0x30, 0x2A, 0x28};

    /* ================= INIT ================= */

    void init_gpios()
    {
        chip = gpiod_chip_open_by_name("gpiochip0");

        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            lines[i] = gpiod_chip_get_line(chip, lpn_offsets[i]);
            gpiod_line_request_output(lines[i], "vl53", 0);
        }

        sleep(1);
    }

    void init_sensors()
    {
        i2c_fd = open(I2C_DEV, O_RDWR);

        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            RCLCPP_INFO(get_logger(), "Inicializando sensor %d...", i);

            gpiod_line_set_value(lines[i], 1);
            usleep(500000);

            Dev[i].platform.fd = i2c_fd;

            {
                std::lock_guard<std::mutex> lock(i2c_mutex);

                Dev[i].platform.address = 0x29;
                ioctl(i2c_fd, I2C_SLAVE, 0x29);

                vl53l8cx_set_i2c_address(&Dev[i], sensor_addr[i] << 1);

                Dev[i].platform.address = sensor_addr[i];
                ioctl(i2c_fd, I2C_SLAVE, sensor_addr[i]);

                vl53l8cx_init(&Dev[i]);
            }

            vl53l8cx_set_resolution(&Dev[i], VL53L8CX_RESOLUTION_8X8);
            vl53l8cx_set_ranging_frequency_hz(&Dev[i], 10);
            vl53l8cx_start_ranging(&Dev[i]);

            RCLCPP_INFO(get_logger(), "Sensor %d pronto!", i);
        }
    }

    /* ================= LOOP POR SENSOR ================= */

    void sensor_loop(int s)
    {
        rclcpp::Rate rate(100); // loop leve (não trava CPU)

        while (running && rclcpp::ok())
        {
            uint8_t ready = 0;

            {
                std::lock_guard<std::mutex> lock(i2c_mutex);
                vl53l8cx_check_data_ready(&Dev[s], &ready);
            }

            if (!ready)
            {
                rate.sleep();
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(i2c_mutex);
                vl53l8cx_get_ranging_data(&Dev[s], &Results[s]);
            }

            build_scan(s);

            scan_msg[s].header.stamp = now();
            scan_msg[s].header.frame_id = "vl53_" + std::to_string(s);

            scan_pub[s]->publish(scan_msg[s]);

            rate.sleep();
        }
    }

    /* ================= BUILD SCAN ================= */

    inline void build_scan(int s)
    {
        auto &scan = scan_msg[s];

        for (int col = 0; col < GRID_W; col++)
        {
            int idx3 = 3 * GRID_W + col;
            int idx4 = 4 * GRID_W + col;
            int idx5 = 5 * GRID_W + col;

            float d3 = Results[s].distance_mm[idx3] * 0.001f;
            float d4 = Results[s].distance_mm[idx4] * 0.001f;
            float d5 = Results[s].distance_mm[idx5] * 0.001f;

            uint8_t st3 = Results[s].target_status[idx3];
            uint8_t st4 = Results[s].target_status[idx4];
            uint8_t st5 = Results[s].target_status[idx5];

            float best = std::numeric_limits<float>::infinity();

            // if ((st3 == 5 || st3 == 6 || st3 == 9) && d3 > 0.2f && d3 <= 4.0f)
            //     best = d3;

            if ((st4 == 5 || st4 == 6 || st4 == 9) && d4 > 0.2f && d4 <= 4.0f)
                best = (best < d4) ? best : d4;

            // if ((st5 == 5 || st5 == 6 || st5 == 9) && d5 > 0.2f && d5 <= 4.0f)
            //     best = (best < d5) ? best : d5;

            scan.ranges[col] = best;
        }
    }
};

/* ================= MAIN ================= */

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    rclcpp::executors::MultiThreadedExecutor exec;
    auto node = std::make_shared<VL53ThreadedNode>();

    exec.add_node(node);
    exec.spin();

    rclcpp::shutdown();
    return 0;
}


// #include <rclcpp/rclcpp.hpp>
// #include <sensor_msgs/msg/laser_scan.hpp>

// #include <gpiod.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/ioctl.h>
// #include <linux/i2c-dev.h>

// #include <cmath>
// #include <limits>
// #include <thread>
// #include <mutex>
// #include <atomic>

// extern "C" {
// #include "vl53l8cx_api.h"
// #include "platform.h"
// }

// #define SENSOR_COUNT 3
// #define GRID_W 8
// #define GRID_H 8
// #define GRID (GRID_W * GRID_H)

// #define FOV_DEG 63.0f
// #define FOV_RAD (FOV_DEG * M_PI / 180.0f)

// #define I2C_DEV "/dev/i2c-1"
// #define MERGED_RAYS 60

// class VL53MergedNode : public rclcpp::Node
// {
// public:
//     VL53MergedNode() : Node("vl53_merged_node")
//     {
//         running = true;

//         // offsets dos sensores (do seu TF)
//         sensor_x[0] = 0.31f; sensor_y[0] = -0.08f;
//         sensor_x[1] = 0.31f; sensor_y[1] =  0.00f;
//         sensor_x[2] = 0.31f; sensor_y[2] =  0.08f;

//         // pré-cálculo angular (performance)
//         for (int col = 0; col < GRID_W; col++)
//         {
//             float theta =
//                 (col - (GRID_W - 1) / 2.0f) *
//                 (FOV_RAD / (GRID_W - 1));

//             cos_table[col] = std::cos(theta);
//             sin_table[col] = std::sin(theta);
//         }

//         // publisher único
//         merged_pub = create_publisher<sensor_msgs::msg::LaserScan>(
//             "/vl53/merged_scan",
//             rclcpp::SensorDataQoS().keep_last(1).best_effort());

//         merged_scan.ranges.resize(MERGED_RAYS);

//         merged_scan.angle_min = -1.1f;
//         merged_scan.angle_max =  1.1f;
//         merged_scan.angle_increment =
//             (merged_scan.angle_max - merged_scan.angle_min) / (MERGED_RAYS - 1);

//         merged_scan.range_min = 0.02f;
//         merged_scan.range_max = 4.0f;

//         init_gpios();
//         init_sensors();

//         // threads independentes por sensor
//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             threads[i] = std::thread(&VL53MergedNode::sensor_loop, this, i);
//         }

//         // timer de fusão (10 Hz)
//         timer = create_wall_timer(
//             std::chrono::milliseconds(100),
//             std::bind(&VL53MergedNode::publish_merged, this));
//     }

//     ~VL53MergedNode()
//     {
//         running = false;

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             if (threads[i].joinable())
//                 threads[i].join();

//             vl53l8cx_stop_ranging(&Dev[i]);
//         }

//         if (i2c_fd >= 0) close(i2c_fd);
//     }

// private:

//     /* ======== VL53 ======== */
//     VL53L8CX_Configuration Dev[SENSOR_COUNT];
//     VL53L8CX_ResultsData Results[SENSOR_COUNT];

//     /* ======== ROS ======== */
//     rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr merged_pub;
//     sensor_msgs::msg::LaserScan merged_scan;
//     rclcpp::TimerBase::SharedPtr timer;

//     /* ======== THREAD ======== */
//     std::thread threads[SENSOR_COUNT];
//     std::atomic<bool> running;
//     std::mutex i2c_mutex;
//     std::mutex data_mutex;

//     /* ======== HARDWARE ======== */
//     int i2c_fd;
//     struct gpiod_chip *chip;
//     struct gpiod_line *lines[SENSOR_COUNT];

//     int lpn_offsets[SENSOR_COUNT] = {144, 50, 85};
//     uint8_t sensor_addr[SENSOR_COUNT] = {0x30, 0x2A, 0x28};

//     /* ======== GEOMETRIA ======== */
//     float sensor_x[SENSOR_COUNT];
//     float sensor_y[SENSOR_COUNT];

//     float cos_table[GRID_W];
//     float sin_table[GRID_W];

//     /* ================= GPIO ================= */

//     void init_gpios()
//     {
//         chip = gpiod_chip_open_by_name("gpiochip0");

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             lines[i] = gpiod_chip_get_line(chip, lpn_offsets[i]);
//             gpiod_line_request_output(lines[i], "vl53", 0);
//         }

//         sleep(1);
//     }

//     /* ================= INIT SENSORES ================= */

//     void init_sensors()
//     {
//         i2c_fd = open(I2C_DEV, O_RDWR);

//         for (int i = 0; i < SENSOR_COUNT; i++)
//         {
//             RCLCPP_INFO(get_logger(), "Inicializando sensor %d...", i);

//             gpiod_line_set_value(lines[i], 1);
//             usleep(500000);

//             Dev[i].platform.fd = i2c_fd;

//             {
//                 std::lock_guard<std::mutex> lock(i2c_mutex);

//                 Dev[i].platform.address = 0x29;
//                 ioctl(i2c_fd, I2C_SLAVE, 0x29);

//                 vl53l8cx_set_i2c_address(&Dev[i], sensor_addr[i] << 1);

//                 Dev[i].platform.address = sensor_addr[i];
//                 ioctl(i2c_fd, I2C_SLAVE, sensor_addr[i]);

//                 vl53l8cx_init(&Dev[i]);
//             }

//             vl53l8cx_set_resolution(&Dev[i], VL53L8CX_RESOLUTION_8X8);
//             vl53l8cx_set_ranging_frequency_hz(&Dev[i], 10);
//             vl53l8cx_start_ranging(&Dev[i]);

//             RCLCPP_INFO(get_logger(), "Sensor %d pronto!", i);
//         }
//     }

//     /* ================= THREAD SENSOR ================= */

//     void sensor_loop(int s)
//     {
//         rclcpp::Rate rate(100);

//         while (running && rclcpp::ok())
//         {
//             uint8_t ready = 0;

//             {
//                 std::lock_guard<std::mutex> lock(i2c_mutex);
//                 vl53l8cx_check_data_ready(&Dev[s], &ready);
//             }

//             if (!ready)
//             {
//                 rate.sleep();
//                 continue;
//             }

//             {
//                 std::lock_guard<std::mutex> lock(i2c_mutex);
//                 vl53l8cx_get_ranging_data(&Dev[s], &Results[s]);
//             }

//             rate.sleep();
//         }
//     }

//     /* ================= FUSÃO REAL ================= */

//     void build_merged_scan()
//     {
//         for (int i = 0; i < MERGED_RAYS; i++)
//             merged_scan.ranges[i] = std::numeric_limits<float>::infinity();

//         std::lock_guard<std::mutex> lock(data_mutex);

//         for (int s = 0; s < SENSOR_COUNT; s++)
//         {
//             for (int col = 0; col < GRID_W; col++)
//             {
//                 int idx3 = 3 * GRID_W + col;
//                 int idx4 = 4 * GRID_W + col;

//                 float d3 = Results[s].distance_mm[idx3] * 0.001f;
//                 float d4 = Results[s].distance_mm[idx4] * 0.001f;

//                 uint8_t st3 = Results[s].target_status[idx3];
//                 uint8_t st4 = Results[s].target_status[idx4];

//                 float d = std::numeric_limits<float>::infinity();

//                 if ((st3 == 5 || st3 == 6 || st3 == 9) && d3 > 0.02f && d3 <= 4.0f)
//                     d = d3;

//                 if ((st4 == 5 || st4 == 6 || st4 == 9) && d4 > 0.02f && d4 <= 4.0f)
//                     d = std::min(d, d4);

//                 if (!std::isfinite(d)) continue;

//                 float x = sensor_x[s] + d * cos_table[col];
//                 float y = sensor_y[s] + d * sin_table[col];

//                 float theta = std::atan2(y, x);
//                 float r = std::sqrt(x * x + y * y);

//                 int idx = (theta - merged_scan.angle_min) /
//                           merged_scan.angle_increment;

//                 if (idx < 0 || idx >= MERGED_RAYS) continue;

//                 if (r < merged_scan.ranges[idx])
//                     merged_scan.ranges[idx] = r;
//             }
//         }
//     }

//     /* ================= PUBLICAÇÃO ================= */

//     void publish_merged()
//     {
//         build_merged_scan();

//         merged_scan.header.stamp = now();
//         merged_scan.header.frame_id = "vl53_merged_link";

//         merged_pub->publish(merged_scan);
//     }
// };

// /* ================= MAIN ================= */

// int main(int argc, char ** argv)
// {
//     rclcpp::init(argc, argv);

//     rclcpp::executors::MultiThreadedExecutor exec;
//     auto node = std::make_shared<VL53MergedNode>();

//     exec.add_node(node);
//     exec.spin();

//     rclcpp::shutdown();
//     return 0;
// }