# Registro de calibração ODrive e bringup 4WD — 2026-07-22

## Hardware identificado

- Jetson: `mjv-robot-2`, acessível pelo USB em `192.168.55.1`.
- Firmware das ODrives MKS: `0.5.1`.
- ODrive direita: serial `3553346E3034`.
- ODrive esquerda: serial `3678387D3333`.
- Motores hoverboard: 15 pares de polos.
- Encoder Hall: CPR 90.

## Problema de calibração e solução

O eixo direito falhava com:

- `axis.error = 0x100`
- `encoder.error = 0x2`
- `ENCODER_ERROR_CPR_POLEPAIRS_MISMATCH`

Com `calib_scan_omega = 4*pi rad/s`, a varredura media apenas 96 das 144
transições Hall esperadas. A tentativa de adaptar a relação para 15/60 fez a
checagem matemática passar, mas causou vibração forte por usar escala de
comutação incorreta; essa configuração foi descartada e não foi salva.

A solução correta foi manter 15/90 e reduzir a velocidade da varredura:

```python
axis.motor.config.pole_pairs = 15
axis.encoder.config.mode = ENCODER_MODE_HALL
axis.encoder.config.cpr = 90
axis.encoder.config.calib_scan_distance = 48 * math.pi
axis.encoder.config.calib_scan_omega = 2 * math.pi
axis.encoder.config.calib_range = 0.05
```

Com isso, os dois eixos da ODrive direita mediram 144/144 transições,
terminaram com `motor.is_calibrated=True`, `encoder.is_ready=True` e todos os
erros zerados.

Configuração final salva na ODrive direita:

```python
axis.motor.config.pre_calibrated = True
axis.encoder.config.pre_calibrated = True
axis.config.startup_encoder_offset_calibration = True
axis.config.startup_closed_loop_control = False
```

O controle fechado automático permanece desativado para evitar que os motores
sejam armados durante o boot.

## Bringup ROS 2

O arquivo disponível no pacote é:

```bash
ros2 launch marsupial_robot_base bringup_base_4wd.launch.py
```

O arquivo `config/robot_4wd.yaml` foi atualizado com os seriais explícitos:

```yaml
left_odrive_serial:  "3678387D3333"
right_odrive_serial: "3553346E3034"
```

O loop foi reduzido de 30 Hz para 10 Hz. A 30 Hz, a sessão Fibre da ODrive
esquerda virou `EmptyInterface` apó aproximadamente 25 segundos. A 10 Hz, os
quatro eixos permaneceram conectados, em malha fechada e sem erros durante o
teste de 45 segundos.

Validação observada:

- `/wheel/odom`: aproximadamente 10 Hz apó o ajuste.
- `/ui/drive_status`: FL, RL, FR e RR conectados.
- Todos os eixos em estado 8 (`CLOSED_LOOP_CONTROL`) durante o bringup.
- Todos os `axis_error` iguais a zero.
- Ao encerrar o teste, os quatro eixos foram colocados em estado 1 (`IDLE`).

## Vibração em velocidade zero

Com os ganhos anteriores:

```python
axis.controller.config.vel_gain = 3.0
axis.controller.config.vel_integrator_gain = 1.0
```

foi observada oscilação em baixa velocidade. Em um teste com referência de
0.5 rev/s, a estimativa Hall oscilou e a corrente medida variou aproximadamente
entre -2.4 A e +1.8 A. A hipótese principal é PI agressivo combinado com a
baixa resolução do Hall e pouca carga mecânica nas rodas suspensas.

Plano de tuning, sempre com as rodas suspensas:

1. Testar em RAM `vel_gain=0.5` e `vel_integrator_gain=0.0`.
2. Confirmar ausência de tremor em comando zero.
3. Testar movimento curto e lento.
4. Se estável, introduzir integrador entre 0.05 e 0.1.
5. Salvar na flash somente depois da validação.

## Reinícios da Jetson

A Jetson sofreu diversos resets abruptos. Evidências:

- entradas de `crash` em `last -x`;
- ausência de shutdown normal, kernel panic ou proteção térmica;
- relógio retornando para 1969 durante o boot;
- temperaturas normais, em torno de 52 C;
- consumo em repouso em torno de 4.6–4.9 W.

Isso aponta para perda de alimentação, conversor DC-DC, cabo, conector ou
aterramento. A alimentação da Jetson deve ser corrigida antes de testes com o
robô apoiado no chão.

## Serviço de vídeo defeituoso

O `mjv-web-video.service` tinha aspas desbalanceadas e gerava erros a cada
segundo. Foram desabilitados:

- `mjv-video.target`
- `mjv-ui-mjpeg.service`
- `mjv-web-video.service`

O unit file original foi preservado na Jetson em:

```text
/etc/systemd/mjv-web-video.service.disabled
```

## Estado seguro

Antes de desconectar ou encerrar testes:

```python
axis.controller.input_vel = 0.0
axis.requested_state = AXIS_STATE_IDLE
```

Não usar `pole_pairs=15, CPR=60`: essa relação eliminou o mismatch por
coincidência numérica, mas produziu comutação incorreta e vibração severa.
