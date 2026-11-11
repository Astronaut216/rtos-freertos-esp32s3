/*
 * task_console.cpp - Task 7: gatekeeper của Serial + console lệnh; software timer thống kê
 *
 * Vì sao cần gatekeeper [K8]: Serial là tài nguyên dùng chung. Nếu 8 task cùng Serial.printf
 * thì chữ bị lẫn vào nhau, và task ưu tiên cao bị chặn chờ UART. Ở đây mọi task chỉ đẩy
 * chuỗi vào queue (không block), DUY NHẤT Task 7 lấy ra và in.
 */
#include "app.h"
#include <stdarg.h>

volatile uint32_t g_logDropped = 0;
static portMUX_TYPE s_dropLock = portMUX_INITIALIZER_UNLOCKED;

// Gọi được từ mọi task và từ callback software timer. KHÔNG gọi trong ISR.
void app_log(const char *fmt, ...) {
  LogItem_t it;
  it.tick = xTaskGetTickCount();
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(it.text, sizeof(it.text), fmt, ap);
  va_end(ap);
  // timeout 0: queue đầy thì bỏ log chứ không làm chậm task gọi (vd Task 5 prio 4)
  if (xQueueSend(g_logQueue, &it, 0) != pdPASS) {
    taskENTER_CRITICAL(&s_dropLock);
    g_logDropped = g_logDropped + 1;
    taskEXIT_CRITICAL(&s_dropLock);
  }
}

// =====================================================================
// Software timer [K11]: callback chạy trong "timer daemon task" của FreeRTOS,
// không phải ISR -> được gọi API thường, nhưng KHÔNG được block (sẽ chặn mọi timer khác).
// =====================================================================
void stat_timer_cb(TimerHandle_t t) {
  uint32_t upS = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
  app_log("[STAT] up %lus heap %lu min %lu drop %lu", (unsigned long)upS,
          (unsigned long)esp_get_free_heap_size(),
          (unsigned long)esp_get_minimum_free_heap_size(), (unsigned long)g_logDropped);
}

// =====================================================================
// Lệnh console [K12][K13]
// =====================================================================
static const char *state_str(eTaskState s) {
  switch (s) {
    case eRunning:   return "Running";
    case eReady:     return "Ready";
    case eBlocked:   return "Blocked";
    case eSuspended: return "Suspend";
    case eDeleted:   return "Deleted";
    default:         return "?";
  }
}

static void print_help() {
  Serial.println("Lenh:  help | stat | light | give");
  Serial.println("       s <1..8>    suspend task (vd: s 1 -> LED1 dung nhay)");
  Serial.println("       r <1..8>    resume task");
  Serial.println("       p <1..8> <prio>  doi priority (0..24)");
}

static void print_stat() {
  Serial.println("ID  Ten          Trang thai  Prio  Stack con(byte)  Core");
  for (int i = 0; i < NUM_TASKS; i++) {
    TaskHandle_t h = g_task[i];
    if (!h) continue;
    eTaskState st = (h == xTaskGetCurrentTaskHandle()) ? eRunning : eTaskGetState(h);
    BaseType_t core = -1;
#if ESP_IDF_VERSION_MAJOR >= 5
    core = xTaskGetCoreID(h);
#endif
    Serial.printf("%-3d %-12s %-11s %-5u %-16u %ld\n", i + 1, g_taskName[i], state_str(st),
                  (unsigned)uxTaskPriorityGet(h), (unsigned)uxTaskGetStackHighWaterMark(h),
                  (long)core);
  }
  Serial.printf("Heap trong: %lu byte, thap nhat tu luc chay: %lu byte, log bi bo: %lu\n",
                (unsigned long)esp_get_free_heap_size(),
                (unsigned long)esp_get_minimum_free_heap_size(), (unsigned long)g_logDropped);
}

static void print_light() {
  LightData_t l;
  if (xSemaphoreTake(g_lightMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    Serial.println("Khong lay duoc mutex");
    return;
  }
  l = g_light;
  xSemaphoreGive(g_lightMutex);
  Serial.printf("g_light: raw=%u  %lumV  %u%%  lan doc=%lu  tick=%lu  toi=%d\n", l.raw,
                (unsigned long)l.mv, l.lightPct, (unsigned long)l.count, (unsigned long)l.tick,
                (int)g_dark);
}

static void handle_cmd(char *line) {
  int a = 0, b = 0;
  char c = 0;
  if (!strcmp(line, "help") || !strcmp(line, "?")) { print_help(); return; }
  if (!strcmp(line, "stat"))  { print_stat();  return; }
  if (!strcmp(line, "light")) { print_light(); return; }
  if (!strcmp(line, "give")) {
    Serial.printf("give sem tu console: %s\n", xSemaphoreGive(g_btn2Sem) == pdTRUE ? "OK" : "da co token");
    return;
  }
  int n = sscanf(line, "%c %d %d", &c, &a, &b);
  if (n >= 2 && a >= 1 && a <= NUM_TASKS && g_task[a - 1]) {
    TaskHandle_t h = g_task[a - 1];
    if (c == 's') {
      if (a - 1 == ID_T7) { Serial.println("Khong suspend chinh Task 7 (se mat console)"); return; }
      vTaskSuspend(h);
      Serial.printf("Da suspend %s\n", g_taskName[a - 1]);
      return;
    }
    if (c == 'r') {
      vTaskResume(h);
      Serial.printf("Da resume %s\n", g_taskName[a - 1]);
      return;
    }
    if (c == 'p' && n == 3 && b >= 0 && b < configMAX_PRIORITIES - 1) {
      vTaskPrioritySet(h, b);
      Serial.printf("%s prio -> %d\n", g_taskName[a - 1], b);
      return;
    }
  }
  Serial.printf("Lenh khong hop le: '%s' (go help)\n", line);
}

// =====================================================================
// Task 7: in log từ queue + đọc lệnh từ Serial
// =====================================================================
void task7_console(void *pv) {
  char line[32];
  size_t len = 0;
  LogItem_t it;

  app_sync_start(EV_T7);
  Serial.println("[T7] 8 task da qua diem hen (xEventGroupSync), bat dau chay.");

  for (;;) {
    // Chờ log tối đa 20 ms; hết giờ thì quay lại kiểm tra bàn phím.
    // => Task 7 Blocked gần như toàn bộ thời gian, chỉ chạy khi có việc.
    if (xQueueReceive(g_logQueue, &it, pdMS_TO_TICKS(20)) == pdPASS) {
      Serial.printf("[%7lu] %s\n", (unsigned long)(it.tick * portTICK_PERIOD_MS), it.text);
    }

    while (Serial.available()) {
      char ch = (char)Serial.read();
      if (ch == '\r') continue;
      if (ch == '\n') {
        line[len] = 0;
        if (len) handle_cmd(line);
        len = 0;
      } else if (len < sizeof(line) - 1) {
        line[len++] = ch;
      }
    }
  }
}
