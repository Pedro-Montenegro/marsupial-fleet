#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <gpiod.h>

#include "vl53l8cx_api.h"
#include "platform.h"

#define I2C_DEV "/dev/i2c-1"
#define SENSOR_COUNT 3
#define REMAP_RETRIES 3

/* Mapeamento Orin Nano Super */
int lpn_offsets[SENSOR_COUNT] = {144, 50, 85};
uint8_t sensor_addr[SENSOR_COUNT] = {0x30, 0x2A, 0x28};

VL53L8CX_Configuration Dev[SENSOR_COUNT];
VL53L8CX_ResultsData Results[SENSOR_COUNT];

/* Variáveis persistentes para a libgpiod */
struct gpiod_chip *chip;
struct gpiod_line *lines[SENSOR_COUNT];

// ================= GPIO INITIALIZATION =================

int init_system_gpios()
{
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) return -1;

    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        lines[i] = gpiod_chip_get_line(chip, lpn_offsets[i]);
        if (!lines[i]) return -1;

        if (gpiod_line_request_output(lines[i], "vl53_block", 0) < 0)
            return -2; // Provavelmente Busy
    }
    return 0;
}

// ================= SENSOR INITIALIZATION =================

int init_sensor(int idx, int i2c_fd)
{
    uint8_t status;
    uint8_t is_alive = 0;
    int retry;

    printf("\n\033[1;33m[Passo %d/%d] Ativando Sensor %d...\033[0m\n",
           idx + 1, SENSOR_COUNT, idx);

    /* Ativa LPn do sensor e aguarda boot */
    gpiod_line_set_value(lines[idx], 1);
    usleep(500000);

    /* Usa fd compartilhado para evitar race conditions no kernel driver */
    Dev[idx].platform.fd      = i2c_fd;
    Dev[idx].platform.address = 0x29;
    ioctl(i2c_fd, I2C_SLAVE, 0x29);

    /*
     * Remap de endereço com retry.
     * Erros I2C aqui são esperados (sensor pode não ter bootado 100% ainda).
     * O loop de retry reseta o LPn e tenta novamente se necessário.
     */
    for (retry = 0; retry < REMAP_RETRIES; retry++)
    {
        vl53l8cx_set_i2c_address(&Dev[idx], sensor_addr[idx] << 1);
        usleep(150000);

        Dev[idx].platform.address = sensor_addr[idx];
        ioctl(i2c_fd, I2C_SLAVE, sensor_addr[idx]);

        is_alive = 0;
        status = vl53l8cx_is_alive(&Dev[idx], &is_alive);

        if (status == 0 && is_alive)
            break;

        printf("  Retry remap sensor %d (%d/%d)...\n", idx, retry + 1, REMAP_RETRIES);

        /* Pulso de reset no LPn antes de tentar novamente */
        gpiod_line_set_value(lines[idx], 0);
        usleep(200000);
        gpiod_line_set_value(lines[idx], 1);
        usleep(500000);

        Dev[idx].platform.address = 0x29;
        ioctl(i2c_fd, I2C_SLAVE, 0x29);
    }

    if (!is_alive)
    {
        printf("\033[1;31mERRO: Sensor %d não responde em 0x%02X após %d tentativas.\033[0m\n",
               idx, sensor_addr[idx], REMAP_RETRIES);
        return -1;
    }

    printf("  Sensor %d vivo em 0x%02X. Carregando firmware...\n", idx, sensor_addr[idx]);

    status = vl53l8cx_init(&Dev[idx]);
    if (status)
    {
        printf("\033[1;31mERRO: init sensor %d: status=%d\033[0m\n", idx, status);
        return -1;
    }

    vl53l8cx_set_resolution(&Dev[idx], VL53L8CX_RESOLUTION_8X8);
    vl53l8cx_set_ranging_frequency_hz(&Dev[idx], 30);
    vl53l8cx_set_integration_time_ms(&Dev[idx], 10);

    vl53l8cx_start_ranging(&Dev[idx]);

    printf("  \033[1;32mSensor %d pronto em 0x%02X!\033[0m\n", idx, sensor_addr[idx]);
    return 0;
}

// ================= MAIN SEQUENCE =================

int main()
{
    int i, j, k;

    printf("\n\033[1;36m--- VL53L8CX: Startup por Isolamento Total ---\033[0m\n");

    if (init_system_gpios() != 0)
    {
        printf("\033[1;31mErro GPIO: Rode com sudo ou verifique se pinos estão ocupados.\033[0m\n");
        return -1;
    }

    /* Garante que todos os sensores estão desligados antes de começar */
    for (i = 0; i < SENSOR_COUNT; i++) gpiod_line_set_value(lines[i], 0);
    sleep(1);

    /*
     * Abre o fd de I2C UMA única vez e compartilha entre todos os sensores.
     * Múltiplos open() no mesmo barramento causam race conditions no driver.
     */
    int i2c_fd = open(I2C_DEV, O_RDWR);
    if (i2c_fd < 0)
    {
        perror("Erro abrindo I2C");
        return -1;
    }

    for (i = 0; i < SENSOR_COUNT; i++)
    {
        /*
         * Para o ranging dos sensores já ativos antes de inicializar o próximo.
         * Sensores em ranging contínuo geram tráfego I2C assíncrono que
         * interfere no firmware download (~80KB) do sensor sendo inicializado.
         */
        for (int prev = 0; prev < i; prev++)
            vl53l8cx_stop_ranging(&Dev[prev]);

        if (init_sensor(i, i2c_fd) != 0)
            return -1;

        /* Retoma ranging dos sensores anteriores */
        for (int prev = 0; prev < i; prev++)
            vl53l8cx_start_ranging(&Dev[prev]);
    }

    /* --- LOOP DE LEITURA --- */
    printf("\n\033[1;35mTodos os sensores ativos. Iniciando dashboard...\033[0m\n");
    usleep(1000000);
    printf("\033[2J"); // Clear screen

    while (1)
    {
        printf("\033[H"); // Cursor Home

        for (i = 0; i < SENSOR_COUNT; i++)
        {
            uint8_t isReady = 0;
            vl53l8cx_check_data_ready(&Dev[i], &isReady);

            if (isReady)
            {
                vl53l8cx_get_ranging_data(&Dev[i], &Results[i]);
                printf("\nSensor %d (0x%02X):\n", i, sensor_addr[i]);

                for (j = 0; j < 8; j++)
                {
                    for (k = 0; k < 8; k++)
                    {
                        uint16_t d = Results[i].distance_mm[j * 8 + k];
                        printf("%4d ", (d == 0 || d > 4000) ? 4000 : d);
                    }
                    printf("\n");
                }
            }
        }
    }

    close(i2c_fd);
    return 0;
}