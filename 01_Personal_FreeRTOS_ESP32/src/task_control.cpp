/*
 * task_control.cpp - Task 7 (điều khiển hệ thống bằng lệnh) + log qua mutex + software timer
 *
 * Task 7 KHÔNG in log thay cho task khác. Nó chỉ làm 2 việc, chờ CÙNG LÚC bằng một queue set [K8]:
 *   - dòng lệnh người dùng gõ ở Serial Monitor (callback UART RX đẩy vào g_cmdQueue)
 *   - tín hiệu của software timer (g_statSem) -> in thống kê định kỳ
 * Log của các task được in trực tiếp, Serial được bảo vệ bằng recursive mutex [K6b].
 */
#include "app.h"
#include <stdarg.h>

volatile uint32_t g_logDropped = 0;
static portMUX_TYPE s_dropLock = portMUX_INITIALIZER_UNLOCKED;

// =====================================================================
// Log [K6b]: mỗi task tự in, nhưng phải giữ g_serialMutex trong lúc in.
// - Mutex (không phải critical section) vì in UART mất hàng trăm µs, không được tắt ngắt lâu như vậy.
// - Có kế thừa ưu tiên: Task 5 (prio 4) chờ mutex mà Task 6 (prio 1) đang giữ thì Task 6 được nâng
//   lên prio 4 cho tới khi nhả -> Task 5 không bị task prio 2..3 chen ngang chặn lâu (đảo ưu tiên).
// - RECURSIVE: Task 7 giữ mutex suốt lúc in bảng "stat" nhiều dòng, bên trong vẫn gọi được app_log()
//   (cùng task lấy lại mutex lần 2 không bị tự khoá).
// - Chờ tối đa LOG_WAIT_MS: quá thì bỏ dòng log và đếm, task gọi không bị treo.
// Không gọi trong ISR / callback software timer.
// =====================================================================
void app_log(const char *fmt, ...) {
  char text[LOG_TEXT_LEN];
  TickType_t tick = xTaskGetTickCount();
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);

  if (xSemaphoreTakeRecursive(g_serialMutex, pdMS_TO_TICKS(LOG_WAIT_MS)) == pdTRUE) {
    Serial.printf("[%7lu] %s\n", (unsigned long)(tick * portTICK_PERIOD_MS), text);
    xSemaphoreGiveRecursive(g_serialMutex);
  } else {
    taskENTER_CRITICAL(&s_dropLock);
    g_logDropped = g_logDropped + 1;
    taskEXIT_CRITICAL(&s_dropLock);
  }
}

// =====================================================================
// Nhận lệnh: callback chạy trong task sự kiện UART của Arduino-ESP32 mỗi khi có byte tới.
// Ghép ký tự thành dòng; đủ dòng thì đẩy vào g_cmdQueue (timeout 0: queue đầy thì bỏ lệnh).
// Task 7 nhờ vậy Blocked hoàn toàn khi không có lệnh, không phải quét Serial định kỳ.
// =====================================================================
static void on_uart_rx() {
  static CmdLine_t line;
  static size_t len = 0;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (len) {
        line.text[len] = 0;
        xQueueSend(g_cmdQueue, &line, 0);       // copy theo giá trị -> dùng lại 'line' ngay
      }
      len = 0;
    } else if (len < sizeof(line.text) - 1) {
      line.text[len++] = ch;
    }
  }
}

void serial_begin_rx() {
  Serial.onReceive(on_uart_rx);
}

// =====================================================================
// Software timer [K11]: callback chạy trong timer daemon task, KHÔNG được block.
// Chỉ "give" semaphore để đánh thức Task 7; việc in (chậm) để Task 7 làm.
// =====================================================================
void stat_timer_cb(TimerHandle_t t) {
  xSemaphoreGive(g_statSem);
}

// =====================================================================
// Lệnh [K12][K13]
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

// Mọi hàm in nhiều dòng giữ g_serialMutex cho cả khối -> các dòng không bị log task khác chen vào giữa
static void print_help() {
  xSemaphoreTakeRecursive(g_serialMutex, portMAX_DELAY);
  Serial.println("Lenh:  help | stat | light | give");
  Serial.println("       s <1..8>    suspend task (vd: s 1 -> LED1 dung nhay)");
  Serial.println("       r <1..8>    resume task");
  Serial.println("       p <1..8> <prio>  doi priority (0..24)");
  xSemaphoreGiveRecursive(g_serialMutex);
}

static void print_stat() {
  xSemaphoreTakeRecursive(g_serialMutex, portMAX_DELAY);
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
  app_log("heap %lu B, thap nhat %lu B, log bo %lu",     // gọi app_log khi ĐANG giữ mutex: nhờ recursive
          (unsigned long)esp_get_free_heap_size(),
          (unsigned long)esp_get_minimum_free_heap_size(), (unsigned long)g_logDropped);
  xSemaphoreGiveRecursive(g_serialMutex);
}

static void print_light() {
  LightData_t l;
  if (xSemaphoreTake(g_lightMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    app_log("Khong lay duoc mutex g_light");
    return;
  }
  l = g_light;
  xSemaphoreGive(g_lightMutex);
  app_log("g_light: raw=%u %lumV %u%% lan=%lu toi=%d", l.raw, (unsigned long)l.mv, l.lightPct,
          (unsigned long)l.count, (int)g_dark);
}

static void handle_cmd(const char *line) {
  int a = 0, b = 0;
  char c = 0;
  if (!strcmp(line, "help") || !strcmp(line, "?")) { print_help(); return; }
  if (!strcmp(line, "stat"))  { print_stat();  return; }
  if (!strcmp(line, "light")) { print_light(); return; }
  if (!strcmp(line, "give")) {
    app_log("give sem tu lenh: %s", xSemaphoreGive(g_btn2Sem) == pdTRUE ? "OK" : "da co token");
    return;
  }
  int n = sscanf(line, "%c %d %d", &c, &a, &b);
  if (n >= 2 && a >= 1 && a <= NUM_TASKS && g_task[a - 1]) {
    TaskHandle_t h = g_task[a - 1];
    if (c == 's') {
      if (a - 1 == ID_T7) { app_log("Khong suspend chinh Task 7 (se mat lenh)"); return; }
      vTaskSuspend(h);
      app_log("Da suspend %s", g_taskName[a - 1]);
      return;
    }
    if (c == 'r') {
      vTaskResume(h);
      app_log("Da resume %s", g_taskName[a - 1]);
      return;
    }
    if (c == 'p' && n == 3 && b >= 0 && b < configMAX_PRIORITIES - 1) {
      vTaskPrioritySet(h, b);
      app_log("%s prio -> %d", g_taskName[a - 1], b);
      return;
    }
  }
  app_log("Lenh khong hop le: '%s' (go help)", line);
}

// =====================================================================
// Task 7: điều khiển. Chờ queue set -> biết sự kiện đến từ đâu -> lấy đúng đối tượng đó ra.
// =====================================================================
void task7_control(void *pv) {
  app_sync_start(EV_T7);
  app_log("T7 8 task da qua diem hen (xEventGroupSync), bat dau chay");

  for (;;) {
    // [K8] Blocked tới khi g_cmdQueue có dòng lệnh HOẶC g_statSem được give. 0 % CPU khi rảnh.
    QueueSetMemberHandle_t who = xQueueSelectFromSet(g_ctrlSet, portMAX_DELAY);

    if (who == (QueueSetMemberHandle_t)g_cmdQueue) {
      CmdLine_t cmd;
      xQueueReceive(g_cmdQueue, &cmd, 0);        // chắc chắn có, không cần chờ
      handle_cmd(cmd.text);
    } else if (who == (QueueSetMemberHandle_t)g_statSem) {
      xSemaphoreTake(g_statSem, 0);
      uint32_t upS = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
      app_log("[STAT] up %lus heap %lu min %lu drop %lu", (unsigned long)upS,
              (unsigned long)esp_get_free_heap_size(),
              (unsigned long)esp_get_minimum_free_heap_size(), (unsigned long)g_logDropped);
    }
  }
}
