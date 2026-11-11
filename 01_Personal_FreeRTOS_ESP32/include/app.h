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
  uint16_t   raw;        // ADC 12 bit (trung bình ADC_SAMPLES mẫu)
  uint32_t   mv;         // điện áp AO (mV)
  uint8_t    lightPct;   // 0..100 %, 100 = sáng nhất
  uint32_t   count;      // số lần đã đọc
  TickType_t tick;       // thời điểm đọc
} LightData_t;

extern LightData_t       g_light;        // Task 2 ghi, Task 6 + console đọc
extern SemaphoreHandle_t g_lightMutex;   // mutex bảo vệ g_light

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
extern QueueHandle_t      g_logQueue;    // log: mọi task -> Task 7 (gatekeeper của Serial)
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

// ---------- Handle task (console dùng để suspend/resume/prio) ----------
enum { ID_T1 = 0, ID_T2, ID_T3, ID_T4, ID_T5, ID_T6, ID_T7, ID_T8, NUM_TASKS };   // T1..T8 đã bị Arduino dùng cho chân touch
extern TaskHandle_t g_task[NUM_TASKS];
extern const char  *const g_taskName[NUM_TASKS];

// Tham số truyền vào task nút nhấn qua pvParameters
typedef struct {
  uint8_t     pin;
  const char *name;
} ButtonCfg_t;

// Phần tử của queue log (copy theo giá trị vào queue)
typedef struct {
  TickType_t tick;
  char       text[LOG_TEXT_LEN];
} LogItem_t;
extern volatile uint32_t g_logDropped;   // số log bị bỏ do queue đầy

// ---------- Hàm ----------
void app_log(const char *fmt, ...);      // gửi log vào queue, không block
void app_sync_start(EventBits_t myBit);  // điểm hẹn khởi động (xEventGroupSync)

void task1_led1(void *pv);
void task2_sensor(void *pv);
void task3_btn1(void *pv);
void task4_btn2(void *pv);
void task5_led3(void *pv);
void task6_oled(void *pv);
void task7_console(void *pv);
void task8_alarm(void *pv);
void stat_timer_cb(TimerHandle_t t);
