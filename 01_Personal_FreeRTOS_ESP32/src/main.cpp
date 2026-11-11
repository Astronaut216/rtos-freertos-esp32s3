/*
 * Project cá nhân RTOS (70%) - FreeRTOS trên ESP32
 * Sinh viên: Đặng Đình Mạnh - MSSV 24119055 - GVHD: Huỳnh Hoàng Hà
 *
 * main.cpp: khai báo đối tượng FreeRTOS dùng chung, tạo task, khởi động hệ thống.
 *
 * Bảng tra yêu cầu đề -> code (minh chứng cho report):
 *   Task 1 LED1 nháy T=200ms ........................ task_io.cpp      task1_led1()
 *   Task 2 đọc cảm biến 500ms, lưu biến toàn cục .... task_sensor.cpp  task2_sensor()   -> g_light
 *   Task 3 BTN1 đảo LED2 (vòng lặp quét) ............ task_io.cpp      task3_btn1()
 *   Task 4 BTN2 cấp binary semaphore ................ task_io.cpp      task4_btn2()     -> g_btn2Sem
 *   Task 5 chờ semaphore, LED3 sáng 3s rồi tắt ...... task_io.cpp      task5_led3()
 *   Task 6 OLED: LED2, ánh sáng, LED3 đếm ngược 500ms task_display.cpp task6_oled()
 *
 * Kỹ thuật FreeRTOS mở rộng:
 *   [K1]  xTaskCreatePinnedToCore: priority, stack, core ........ main.cpp setup()
 *   [K2]  pvParameters: truyền cấu hình vào task ................ setup() -> task1/3/4
 *   [K3]  vTaskDelayUntil: chu kỳ cố định, không trôi ........... task1, task2, task6
 *   [K4]  vTaskDelay: nhường CPU (Blocked) ...................... task3, task4, task5
 *   [K5]  Binary semaphore: đồng bộ task-task ................... task4 give -> task5 take
 *   [K6]  Mutex: bảo vệ biến toàn cục g_light ................... task2 ghi, task6/task7 đọc
 *   [K7]  Critical section (spinlock 2 lõi) ..................... task5 ghi g_led3, task6 đọc
 *   [K8]  Queue + gatekeeper task: chỉ Task 7 dùng Serial ....... app_log() -> task7
 *   [K9]  Event group: xEventGroupSync (điểm hẹn) + cờ trạng thái app_sync_start(), task6
 *   [K10] Task notification làm "mailbox" 1 giá trị ............. task2 -> task8
 *   [K11] Software timer auto-reload (timer daemon task) ........ stat_timer_cb()
 *   [K12] Điều khiển task lúc chạy: suspend/resume/prio ......... task7 lệnh console
 *   [K13] Giám sát: stack high-water mark, heap, trạng thái task  task7 lệnh "stat"
 *   [K14] vTaskDelete(NULL): xoá loopTask của Arduino ........... loop()
 */
#include "app.h"

// ================== Định nghĩa đối tượng dùng chung ==================
LightData_t       g_light      = {0, 0, 0, 0, 0};
SemaphoreHandle_t g_lightMutex = nullptr;

volatile bool g_led2On = false;
Led3State_t   g_led3   = {false, 0, 0};
portMUX_TYPE  g_led3Lock = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_dark   = false;

SemaphoreHandle_t  g_btn2Sem   = nullptr;
QueueHandle_t      g_logQueue  = nullptr;
EventGroupHandle_t g_events    = nullptr;
TimerHandle_t      g_statTimer = nullptr;

TaskHandle_t g_task[NUM_TASKS] = {nullptr};
const char *const g_taskName[NUM_TASKS] = {
  "T1_led1", "T2_sensor", "T3_btn1", "T4_btn2", "T5_led3", "T6_oled", "T7_console", "T8_alarm"
};

// Tham số cho các task (static: phải còn sống suốt đời task, KHÔNG dùng biến cục bộ của setup) [K2]
static const ButtonCfg_t s_btn1Cfg = {PIN_BTN1, "BTN1"};
static const ButtonCfg_t s_btn2Cfg = {PIN_BTN2, "BTN2"};

// ================== Điểm hẹn khởi động [K9] ==================
// Mỗi task bật bit của mình rồi chờ đủ 8 bit. Kết quả: mọi task chu kỳ bắt đầu cùng 1 tick,
// không task nào chạy khi đối tượng mà nó cần chưa sẵn sàng.
void app_sync_start(EventBits_t myBit) {
  xEventGroupSync(g_events, myBit, EV_ALL_TASKS, portMAX_DELAY);
}

static void create_task(TaskFunction_t fn, int id, uint32_t stack, void *param,
                        UBaseType_t prio, BaseType_t core) {
  BaseType_t ok = xTaskCreatePinnedToCore(fn, g_taskName[id], stack, param, prio, &g_task[id], core);  // [K1]
  if (ok != pdPASS) {
    Serial.printf("[BOOT] Khong tao duoc %s (thieu heap)\n", g_taskName[id]);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== RTOS project - Dang Dinh Manh 24119055 - FreeRTOS on ESP32 ===");
  Serial.printf("[BOOT] FreeRTOS %s, tick %d Hz, %d core\n", tskKERNEL_VERSION_NUMBER,
                configTICK_RATE_HZ, portNUM_PROCESSORS);

  // GPIO
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED3, OUTPUT);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  digitalWrite(PIN_LED3, LOW);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);   // đo được 0..~3.1 V

  // Tạo đối tượng kernel TRƯỚC khi tạo task dùng chúng
  g_btn2Sem    = xSemaphoreCreateBinary();                          // [K5] tạo ra ở trạng thái rỗng (0)
  g_lightMutex = xSemaphoreCreateMutex();                           // [K6] có priority inheritance
  g_logQueue   = xQueueCreate(LOG_QUEUE_LEN, sizeof(LogItem_t));    // [K8]
  g_events     = xEventGroupCreate();                               // [K9]
  g_statTimer  = xTimerCreate("stat", pdMS_TO_TICKS(STAT_TIMER_MS), pdTRUE, nullptr, stat_timer_cb);  // [K11]

  if (!g_btn2Sem || !g_lightMutex || !g_logQueue || !g_events || !g_statTimer) {
    Serial.println("[BOOT] LOI: khong du heap de tao doi tuong kernel");
    for (;;) delay(1000);
  }

  // In trước khi tạo task: sau khi task chạy, chỉ Task 7 được dùng Serial (tránh lẫn chữ)
  Serial.println("[BOOT] Tao 8 task + 1 software timer. Go 'help' trong Serial Monitor de xem lenh.");
  Serial.flush();

  // Tạo task [K1] + truyền tham số [K2]
  //           hàm            id   stack        tham số                                    priority          core
  create_task(task1_led1,    ID_T1, STK_SMALL,   (void *)(uintptr_t)LED1_PERIOD_MS,          PRIO_T1_LED1,    CORE_IO);
  create_task(task2_sensor,  ID_T2, STK_SENSOR,  nullptr,                                    PRIO_T2_SENSOR,  CORE_BG);
  create_task(task3_btn1,    ID_T3, STK_SMALL,   (void *)&s_btn1Cfg,                         PRIO_T3_BTN1,    CORE_IO);
  create_task(task4_btn2,    ID_T4, STK_SMALL,   (void *)&s_btn2Cfg,                         PRIO_T4_BTN2,    CORE_IO);
  create_task(task5_led3,    ID_T5, STK_SMALL,   nullptr,                                    PRIO_T5_LED3,    CORE_IO);
  create_task(task6_oled,    ID_T6, STK_OLED,    nullptr,                                    PRIO_T6_OLED,    CORE_BG);
  create_task(task7_console, ID_T7, STK_CONSOLE, nullptr,                                    PRIO_T7_CONSOLE, CORE_BG);
  create_task(task8_alarm,   ID_T8, STK_SMALL,   nullptr,                                    PRIO_T8_ALARM,   CORE_BG);

  xTimerStart(g_statTimer, 0);
}

// loopTask của Arduino không còn việc gì -> tự xoá để trả lại RAM [K14]
void loop() {
  vTaskDelete(NULL);
}
