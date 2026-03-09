# LeRobot

低成本机器人项目，基于 ESP32-S3 开发板和舵机驱动。

## 项目结构

```
lerobot/
├── firmware/                    # 固件子项目
│   └── esp32_servo_control/     # ESP32-S3 双舵机 PWM 控制
└── README.md
```

## 硬件

- **主控**: Waveshare ESP32-S3-LCD-1.47B
- **舵机**: CyberBrick 9g 360度连续旋转舵机 x2
- **开发框架**: ESP-IDF v5.5.1

## 子项目

### firmware/esp32_servo_control

ESP32-S3 通过 MCPWM 外设控制两个 360 度连续旋转舵机。

- IO2 → 舵机A PWM
- IO3 → 舵机B PWM
- VBUS → 舵机 VCC (5V)
- GND → 舵机 GND

构建与烧录：

```bash
cd firmware/esp32_servo_control
source ~/esp/v5.5.1/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor
```
