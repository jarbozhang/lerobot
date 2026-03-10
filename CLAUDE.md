# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

低成本机器人项目，基于 ESP32-S3 开发板，包含多个 ESP-IDF 固件子项目。

## 构建与烧录

所有子项目均为 ESP-IDF 项目，构建流程相同：

```bash
# 初始化 ESP-IDF 环境（每个终端会话执行一次）
source ~/esp/v5.5.1/esp-idf/export.sh

# 进入子项目目录后执行
idf.py set-target esp32s3    # 首次或切换目标时
idf.py build                 # 编译
idf.py -p /dev/cu.usbmodem1101 flash monitor  # 烧录并监控
idf.py -p /dev/cu.usbmodem1101 flash          # 仅烧录
idf.py -p /dev/cu.usbmodem1101 monitor        # 仅监控（Ctrl+] 退出）
idf.py menuconfig            # 修改 SDK 配置（coffee_machine_controller 有自定义菜单）
```

烧录后需按 RST 按钮手动重启（DTR/RTS 重置会进入下载模式）。

## 子项目

### firmware/esp32_servo_control

MCPWM 驱动双舵机。架构简单：`main.c` → `servo.h/c`。

- `servo_init()` 创建 MCPWM 定时器/操作器/比较器
- `servo_set_speed(servo, -100~+100)` 映射到 1000~2000μs 脉宽
- 依赖: `esp_driver_mcpwm`

### coffee_machine_controller

多通道咖啡机控制器，通过 UART 或 ESP-NOW 接收命令，驱动电机。

模块依赖关系：
```
controller_main.c (入口，初始化所有模块)
  ├── motor_controller  — LEDC PWM 电机驱动 (IO35/IO36, 2kHz)
  ├── command_handler   — 命令解析，调用 motor_controller 执行
  ├── uart_controller   — UART 接收命令 → command_handler
  └── espnow_controller — ESP-NOW 无线接收 → command_handler
```

- 电机长运行命令通过 FreeRTOS 任务实现，同通道互斥
- ESP-NOW 使用 CRC16 校验，支持配对发现
- UART/ESP-NOW 参数可通过 `idf.py menuconfig` 调整（Kconfig.projbuild）

## 硬件信息

### 开发板: Waveshare ESP32-S3-LCD-1.47B

已占用 GPIO（板载外设）：
- LCD: IO39(RST), IO40(SCLK), IO41(DC), IO42(CS), IO45(DIN), IO46(BL)
- SD卡: IO14(SCK), IO15(CMD), IO16(D0), IO17(D2), IO18(D1), IO21(CS)
- IMU: IO47(SCL), IO48(SDA), IO12(INT2), IO13(INT1)
- RGB LED: IO38
- BAT: IO1(ADC)
- UART: IO43(TXD), IO44(RXD)

可用 GPIO: IO0(有BOOT键), IO2, IO3, IO4, IO5, IO6, IO7, IO8, IO9, IO10, IO11, IO19, IO20

当前引脚分配：
- esp32_servo_control: IO2 → 舵机A, IO3 → 舵机B
- coffee_machine_controller: IO35 → 电机1, IO36 → 电机2, IO19 → UART RXD, IO20 → UART TXD

### 舵机: CyberBrick 9g 360度连续旋转

- 50Hz PWM, 1000-2000μs 脉宽
- 1500μs = 停止, <1500 = 正转, >1500 = 反转
- 供电: VBUS (USB 5V)，大电流时可能导致 USB 断连

## 开发环境

- ESP-IDF v5.5.1, 路径: `~/esp/v5.5.1/esp-idf`
- 串口: `/dev/cu.usbmodem1101` (USB-Serial/JTAG)
- 控制台: `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`
- 无测试框架，通过串口 Monitor 验证行为
