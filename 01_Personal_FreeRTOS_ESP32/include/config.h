/*
 * config.h - Cấu hình phần cứng + thông số các task
 * Project cá nhân RTOS: FreeRTOS trên ESP32 - Đặng Đình Mạnh, MSSV 24119055
 */
#pragma once

// ============ Chân kết nối ============
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// ESP32-S3 DevKitC-1 - dùng chung mạch testboard với nhóm (cùng chân với bài Lê Ngọc Hoàn).
// Tránh: 0/3/45/46 (strapping), 19/20 (USB), 26..37 (flash/PSRAM), 43/44 (UART0)
#define PIN_LED1        4   // Task 1 - nháy 200 ms
#define PIN_LED2        5   // Task 3 - BTN1 đảo trạng thái
#define PIN_LED3        6   // Task 5 - sáng 3 s khi nhận semaphore
#define PIN_BTN1       15   // nút nối GND, INPUT_PULLUP
#define PIN_BTN2       16
#define PIN_LDR         1   // AO module cảm biến -> ADC1_CH0
#define PIN_I2C_SDA    10   // OLED SSD1306. Không dùng GPIO8: trên kit này GPIO8 bị kéo xuống GND
#define PIN_I2C_SCL    11
#else
// ESP32 DevKit thường (WROOM-32)
#define PIN_LED1       19
#define PIN_LED2       18
#define PIN_LED3        5
#define PIN_BTN1        4
#define PIN_BTN2       13
#define PIN_LDR        35   // ADC1_CH7, chân chỉ vào
#define PIN_I2C_SDA    21
#define PIN_I2C_SCL    22
#endif

#define OLED_W        128
#define OLED_H         64
#define OLED_ADDR    0x3C

// Module quang trở thường cho AO CAO khi TỐI -> đảo để 100% = sáng nhất.
// Nếu dùng mạch chia áp tự ráp (LDR lên 3V3, điện trở xuống GND) thì đặt 0.
#define LDR_INVERT      1

// ============ Thời gian (ms) ============
#define LED1_PERIOD_MS      200   // Task 1: chu kỳ đầy đủ (100 ms sáng + 100 ms tắt)
#define SENSOR_PERIOD_MS    500   // Task 2
#define BTN_SCAN_MS          10   // Task 3, 4: chu kỳ quét nút
#define BTN_STABLE_COUNT      3   // 3 mẫu liên tiếp giống nhau (30 ms) mới chấp nhận
#define LED3_ON_MS         3000   // Task 5
#define OLED_PERIOD_MS      500   // Task 6
#define STAT_TIMER_MS     10000   // software timer in thống kê hệ thống
#define ADC_SAMPLES           8   // Task 2 lấy trung bình 8 mẫu

// Ngưỡng báo tối (Task 8), có trễ (hysteresis) để không bật/tắt liên tục quanh ngưỡng
#define DARK_ON_PCT          25
#define DARK_OFF_PCT         35

// ============ Priority (số lớn = ưu tiên cao) ============
#define PRIO_T1_LED1      2
#define PRIO_T2_SENSOR    2
#define PRIO_T3_BTN1      3
#define PRIO_T4_BTN2      3
#define PRIO_T5_LED3      4   // phản hồi semaphore nhanh nhất
#define PRIO_T6_OLED      1
#define PRIO_T7_CONTROL   2   // lệnh người dùng: phản hồi nhanh hơn OLED
#define PRIO_T8_ALARM     3

// ============ Stack (đơn vị BYTE trên ESP-IDF) ============
#define STK_SMALL      3072   // 2048 chỉ còn ~650 byte dư ở T5/T8 (đo bằng lệnh stat) -> tăng lên
#define STK_SENSOR     3072
#define STK_OLED       4096
#define STK_CONTROL    4096

// ============ Core (ESP32 2 lõi) ============
#define CORE_IO   1   // LED, nút: cần đáp ứng nhanh
#define CORE_BG   0   // cảm biến, OLED, điều khiển: tác vụ nền

// ============ Log + lệnh ============
#define LOG_TEXT_LEN     64   // độ dài tối đa 1 dòng log
#define LOG_WAIT_MS      10   // chờ mutex Serial tối đa; quá thì bỏ dòng log, task không bị treo
#define CMD_QUEUE_LEN     4   // số dòng lệnh chờ xử lý
#define CMD_LEN          32
