/*
 * task_io.cpp - Task 1, 3, 4, 5: LED và nút nhấn (chạy trên core 1)
 */
#include "app.h"

// =====================================================================
// Task 1: LED1 nháy với chu kỳ T = 200 ms (100 ms sáng, 100 ms tắt)
// Chu kỳ nhận qua pvParameters [K2], giữ nhịp bằng vTaskDelayUntil [K3]
// =====================================================================
void task1_led1(void *pv) {
  const uint32_t   periodMs = (uint32_t)(uintptr_t)pv;
  const TickType_t half     = pdMS_TO_TICKS(periodMs / 2);
  bool level = false;

  app_sync_start(EV_T1);
  TickType_t wake = xTaskGetTickCount();        // mốc thời gian tuyệt đối
  for (;;) {
    level = !level;
    digitalWrite(PIN_LED1, level);
    // Ngủ tới (wake + half): thời gian chạy thân vòng lặp không làm chu kỳ dài thêm.
    // Nếu dùng vTaskDelay(half) thì chu kỳ thực = half + thời gian chạy -> trôi dần.
    vTaskDelayUntil(&wake, half);
  }
}

// =====================================================================
// Bộ chống dội kiểu "đếm mẫu": quét mỗi BTN_SCAN_MS, chỉ chấp nhận mức mới
// khi đọc giống nhau BTN_STABLE_COUNT lần liên tiếp. Trả true 1 lần mỗi lần NHẤN.
// =====================================================================
typedef struct {
  uint8_t pin;
  uint8_t stable;    // mức đã chấp nhận (HIGH = nhả, do pull-up)
  uint8_t cnt;       // số mẫu liên tiếp khác 'stable'
} Debouncer_t;

static bool debounce_pressed(Debouncer_t *d) {
  uint8_t now = (uint8_t)digitalRead(d->pin);
  if (now == d->stable) {
    d->cnt = 0;
    return false;
  }
  if (++d->cnt < BTN_STABLE_COUNT) return false;
  d->stable = now;
  d->cnt = 0;
  return now == LOW;           // cạnh xuống = nhấn
}

// =====================================================================
// Task 3: BTN1 đảo trạng thái LED2 - vòng lặp quét (polling loop)
// =====================================================================
void task3_btn1(void *pv) {
  const ButtonCfg_t *cfg = (const ButtonCfg_t *)pv;     // [K2]
  // Lấy mức thật lúc khởi động làm mức ổn định: nút đang bị giữ khi cấp điện không bị tính là 1 lần nhấn
  Debouncer_t db = {cfg->pin, (uint8_t)digitalRead(cfg->pin), 0};

  app_sync_start(EV_T3);
  for (;;) {
    if (debounce_pressed(&db)) {
      g_led2On = !g_led2On;
      digitalWrite(PIN_LED2, g_led2On ? HIGH : LOW);
      app_log("%s nhan -> LED2 %s", cfg->name, g_led2On ? "ON" : "OFF");
    }
    // [K4] BẮT BUỘC: không có vTaskDelay thì task prio 3 này chạy liên tục,
    // Task 1 (prio 2) cùng core 1 sẽ không bao giờ được chạy -> LED1 đứng yên.
    vTaskDelay(pdMS_TO_TICKS(BTN_SCAN_MS));
  }
}

// =====================================================================
// Task 4: nhấn BTN2 để "cấp" (give) binary semaphore cho Task 5 [K5]
// =====================================================================
void task4_btn2(void *pv) {
  const ButtonCfg_t *cfg = (const ButtonCfg_t *)pv;
  Debouncer_t db = {cfg->pin, (uint8_t)digitalRead(cfg->pin), 0};
  uint32_t presses = 0;

  app_sync_start(EV_T4);
  for (;;) {
    if (debounce_pressed(&db)) {
      presses++;
      // Binary semaphore chỉ có 0 hoặc 1. Nếu Task 5 đang bận (LED3 đang sáng) và đã có
      // 1 token chờ sẵn thì give trả pdFALSE: lần nhấn này "gộp" vào lần trước.
      BaseType_t r = xSemaphoreGive(g_btn2Sem);
      app_log("%s nhan #%lu -> give sem %s", cfg->name, (unsigned long)presses,
              r == pdTRUE ? "OK" : "(da co token, bo qua)");
    }
    vTaskDelay(pdMS_TO_TICKS(BTN_SCAN_MS));
  }
}

// =====================================================================
// Task 5: đợi binary semaphore; có thì bật LED3 3 s rồi tắt, đợi tiếp
// =====================================================================
void task5_led3(void *pv) {
  app_sync_start(EV_T5);
  for (;;) {
    // Blocked vô thời hạn: task không tốn CPU cho tới khi Task 4 give [K5]
    xSemaphoreTake(g_btn2Sem, portMAX_DELAY);

    TickType_t offAt = xTaskGetTickCount() + pdMS_TO_TICKS(LED3_ON_MS);
    // [K7] 3 trường của g_led3 phải đổi CÙNG LÚC: Task 6 ở core 0 có thể đọc xen vào giữa.
    // Spinlock + tắt ngắt ngắn hạn => Task 6 thấy hoặc trạng thái cũ, hoặc trạng thái mới, không lẫn.
    taskENTER_CRITICAL(&g_led3Lock);
    g_led3.on    = true;
    g_led3.offAt = offAt;
    g_led3.timesOn++;
    taskEXIT_CRITICAL(&g_led3Lock);
    digitalWrite(PIN_LED3, HIGH);
    app_log("T5 nhan sem -> LED3 ON 3s (lan %lu)", (unsigned long)g_led3.timesOn);

    vTaskDelay(pdMS_TO_TICKS(LED3_ON_MS));     // [K4]

    digitalWrite(PIN_LED3, LOW);
    taskENTER_CRITICAL(&g_led3Lock);
    g_led3.on = false;
    taskEXIT_CRITICAL(&g_led3Lock);
    app_log("T5 LED3 OFF, doi semaphore tiep");
  }
}
