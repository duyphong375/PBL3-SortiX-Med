#pragma once

#include "driver/gpio.h"

/**
 * =============================================================================
 * SortiX Hardware Pinout Definitions
 * Target Board: WeAct Studio ESP32-C5-A-N8R8 (ESP32-C5-WROOM-1-N8R8)
 * Standard BOM Reference: danh-sach-linh-kien.md
 * =============================================================================
 *
 * ⚠ FORBIDDEN PINS (DO NOT USE UNDER ANY CIRCUMSTANCE):
 *  - GPIO15 (SPICS1): Hardwired internally to 8 MB Quad SPI PSRAM under shield.
 *  - GPIO13, GPIO14: Native USB (D-, D+) for flashing, JTAG, and USB-CDC console.
 *  - GPIO11, GPIO12: UART0 Debug Console (TX0, RX0 via CH343).
 *  - GPIO26, GPIO27, GPIO28: Strapping pins, Boot Mode, ROM print, RGB/BOOT button.
 */

// --- Sensors ---
#define PIN_IR_S1               GPIO_NUM_0   // Left Header Pin 5: Cảm biến vật cản IR S1 (Active Low)
#define PIN_ENCODER_A           GPIO_NUM_1   // Left Header Pin 6: Encoder Phase A (Xung)
#define PIN_ENCODER_B           GPIO_NUM_4   // Right Header Pin 8: Encoder Phase B (Chiều)
#define PIN_LDR_DO              GPIO_NUM_5   // Right Header Pin 9: Cảm biến ánh sáng LDR (DO)

// --- Actuators & Motor Driver ---
#define PIN_SERVO_1             GPIO_NUM_6   // Left Header Pin 7: Servo 1 PWM (Gạt 1 - Bin 1)
#define PIN_MOTOR_IN1           GPIO_NUM_8   // Left Header Pin 9: L298N IN1 (Direction/PWM)
#define PIN_MOTOR_IN2           GPIO_NUM_9   // Left Header Pin 10: L298N IN2 (Direction/PWM)
#define PIN_SERVO_2             GPIO_NUM_24  // Right Header Pin 4: Servo 2 PWM (Gạt 2 - Bin 2)

// --- User Interface & Indicators ---
#define PIN_LED_STATUS          GPIO_NUM_10  // Left Header Pin 11: LED trạng thái (Xanh lá)
#define PIN_BTN_START           GPIO_NUM_23  // Right Header Pin 5: Nút nhấn Start (NO, Pull-up 10k + RC)
