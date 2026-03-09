# LeRobot 项目指南

## 项目概述

低成本机器人项目，包含多个子项目（固件、上位机等）。

## 目录结构

- `firmware/` — ESP32 固件子项目
  - `esp32_servo_control/` — 双舵机 MCPWM 控制（ESP-IDF）

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
- IO2 → 舵机A PWM
- IO3 → 舵机B PWM

### 舵机: CyberBrick 9g 360度连续旋转

- 50Hz PWM, 1000-2000us 脉宽
- 1500us = 停止, <1500 = 正转, >1500 = 反转
- 供电: VBUS (USB 5V)，注意电流冲击可能导致 USB 断连

## 开发环境

- ESP-IDF v5.5.1, 路径: ~/esp/v5.5.1/esp-idf
- 串口: /dev/cu.usbmodem1101 (USB-Serial/JTAG)
- 控制台: CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
- 烧录后需按 RST 按钮手动重启（DTR/RTS 重置会进入下载模式）

## 注意事项

- 舵机从 VBUS 取电，大电流时可能导致 USB 断连，需考虑外接 5V 电源
- ESP32-S3 USB-Serial/JTAG 重启时会断开重连，日志需等端口恢复后读取
