/*
 * app.h - Đối tượng FreeRTOS dùng chung giữa các task
 * Mỗi đối tượng ghi rõ: ai TẠO, ai GHI/GIVE, ai ĐỌC/TAKE.
 */
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include "config.h"

// ---------- Biến toàn cục của cảm biến (đề: "lưu vào biến toàn cục") ----------
typedef struct {
  bool       ok;         // lần đọc gần nhất thành công?
  uint16_t   raw;        // giá trị thô 16 bit của BH1750
  float      lux;        // cường độ sáng (lx) = raw / 1.2
  uint8_t    barPct;     // 0..100 %, cho thanh mức trên OLED (đầy ở LUX_BAR_MAX)
  uint32_t   count;      // số lần đọc thành công
  uint32_t   errors;     // số lần đọc lỗi (mất dây, không thấy cảm biến)
  TickType_t tick;       // thời điểm đọc
} LightData_t;

extern LightData_t       g_light;        // Task 2 ghi, Task 6 + Task 7 đọc
extern SemaphoreHandle_t g_lightMutex;   // mutex bảo vệ g_light
extern SemaphoreHandle_t g_i2cMutex;     // mutex bảo vệ bus I2C: Task 2 (BH1750) + Task 6 (OLED)

// ---------- LED2 / LED3 ----------
extern volatile bool g_led2On;           // chỉ Task 3 ghi (bool = 1 byte, đọc/ghi nguyên tử)

typedef struct {
  bool       on;
  TickType_t offAt;      // tick sẽ tắt -> Task 6 tính thời gian đếm ngược
  uint32_t   timesOn;    // số lần LED3 đã sáng
} Led3State_t;
extern Led3State_t   g_led3;             // Task 5 ghi, Task 6 đọc
extern portMUX_TYPE  g_led3Lock;         // spinlock cho critical section (ESP32 2 lõi)

extern volatile bool g_dark;             // Task 8 ghi, Task 6 đọc

// ---------- Đồng bộ / giao tiếp ----------
extern SemaphoreHandle_t  g_btn2Sem;     // BINARY semaphore: Task 4 give -> Task 5 take
extern SemaphoreHandle_t  g_serialMutex; // RECURSIVE mutex: mọi task in Serial phải giữ nó
extern QueueHandle_t      g_cmdQueue;    // dòng lệnh: callback UART RX -> Task 7
extern SemaphoreHandle_t  g_statSem;     // BINARY semaphore: software timer -> Task 7
extern QueueSetHandle_t   g_ctrlSet;     // QUEUE SET {g_cmdQueue, g_statSem}: Task 7 chờ cả hai
extern EventGroupHandle_t g_events;      // đồng bộ khởi động + cờ trạng thái
extern TimerHandle_t      g_statTimer;   // software timer auto-reload

// Bit event group
#define EV_T1      (1u << 0)
#define EV_T2      (1u << 1)
#define EV_T3      (1u << 2)
#define EV_T4      (1u << 3)
#define EV_T5      (1u << 4)
#define EV_T6      (1u << 5)
#define EV_T7      (1u << 6)
#define EV_T8      (1u << 7)
#define EV_ALL_TASKS  0xFFu              // điểm hẹn: đủ 8 task mới cùng chạy
#define EV_SENSOR_OK  (1u << 8)          // Task 2 đã có mẫu đầu tiên
#define EV_OLED_OK    (1u << 9)          // OLED khởi tạo thành công

// ---------- Handle task (Task 7 dùng để suspend/resume/prio) ----------
enum { ID_T1 = 0, ID_T2, ID_T3, ID_T4, ID_T5, ID_T6, ID_T7, ID_T8, NUM_TASKS };   // T1..T8 đã bị Arduino dùng cho chân touch
extern TaskHandle_t g_task[NUM_TASKS];
extern const char  *const g_taskName[NUM_TASKS];

// Tham số truyền vào task nút nhấn qua pvParameters
typedef struct {
  uint8_t     pin;
  const char *name;
} ButtonCfg_t;

// Phần tử của queue lệnh (copy theo giá trị vào queue)
typedef struct {
  char text[CMD_LEN];
} CmdLine_t;
extern volatile uint32_t g_logDropped;   // số log bị bỏ do không lấy được mutex Serial kịp

// ---------- Hàm ----------
void app_log(const char *fmt, ...);      // in 1 dòng log dưới g_serialMutex (chờ tối đa LOG_WAIT_MS)
void serial_begin_rx();                  // gắn callback nhận lệnh UART -> g_cmdQueue
void app_sync_start(EventBits_t myBit);  // điểm hẹn khởi động (xEventGroupSync)

void task1_led1(void *pv);
void task2_sensor(void *pv);
void task3_btn1(void *pv);
void task4_btn2(void *pv);
void task5_led3(void *pv);
void task6_oled(void *pv);
void task7_control(void *pv);
void task8_alarm(void *pv);
void stat_timer_cb(TimerHandle_t t);
