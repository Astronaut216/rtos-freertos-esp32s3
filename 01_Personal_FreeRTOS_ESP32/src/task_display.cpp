/*
 * task_display.cpp - Task 6: hiển thị lên OLED SSD1306 mỗi 500 ms
 *   - trạng thái LED2 (On/Off)
 *   - cảm biến ánh sáng (%, mV)
 *   - LED3: đếm ngược thời gian còn sáng -> "OFF"
 */
#include "app.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 s_oled(OLED_W, OLED_H, &Wire, -1);

// Dò địa chỉ 0x3C / 0x3D rồi khởi tạo. Chỉ Task 6 dùng bus I2C nên không cần mutex cho Wire.
static bool oled_begin() {
  static const uint8_t cand[] = {OLED_ADDR, 0x3D};
  uint8_t addr = 0;
  for (uint8_t a : cand) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { addr = a; break; }
  }
  if (addr == 0) return false;
  if (!s_oled.begin(SSD1306_SWITCHCAPVCC, addr, true, false)) return false;
  s_oled.setTextColor(SSD1306_WHITE);
  s_oled.setTextWrap(false);
  app_log("T6 OLED OK tai 0x%02X", addr);
  return true;
}

void task6_oled(void *pv) {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  bool oledOk = oled_begin();
  if (oledOk) xEventGroupSetBits(g_events, EV_OLED_OK);
  else app_log("T6 khong thay OLED (SDA=%d SCL=%d), se thu lai moi 5s", PIN_I2C_SDA, PIN_I2C_SCL);

  app_sync_start(EV_T6);
  // [K9] chờ mẫu cảm biến đầu tiên (tối đa 1 s) để màn hình không hiện số 0 giả.
  // xClearOnExit = pdFALSE: giữ nguyên bit, chỉ đọc.
  xEventGroupWaitBits(g_events, EV_SENSOR_OK, pdFALSE, pdTRUE, pdMS_TO_TICKS(1000));

  TickType_t wake = xTaskGetTickCount();
  uint32_t frame = 0, retry = 0;
  for (;;) {
    // --- 1. Chụp nhanh dữ liệu dùng chung (giữ khoá càng ngắn càng tốt) ---
    LightData_t light = {};
    if (xSemaphoreTake(g_lightMutex, pdMS_TO_TICKS(20)) == pdTRUE) {   // [K6]
      light = g_light;                 // copy struct rồi nhả mutex ngay
      xSemaphoreGive(g_lightMutex);
    }
    taskENTER_CRITICAL(&g_led3Lock);                                    // [K7]
    Led3State_t led3 = g_led3;
    taskEXIT_CRITICAL(&g_led3Lock);
    bool led2 = g_led2On;
    bool dark = g_dark;

    // Thời gian LED3 còn sáng (ms). Phép trừ unsigned vẫn đúng khi tick tràn số.
    TickType_t now = xTaskGetTickCount();
    int32_t remainMs = 0;
    if (led3.on) {
      remainMs = (int32_t)(led3.offAt - now) * portTICK_PERIOD_MS;
      if (remainMs < 0) remainMs = 0;
    }

    // --- 2. Vẽ (ngoài vùng khoá: I2C chậm ~25 ms, không giữ mutex lúc này) ---
    if (oledOk) {
      s_oled.clearDisplay();
      s_oled.setTextSize(1);
      s_oled.setCursor(0, 0);
      s_oled.print("RTOS 24119055   #");
      s_oled.print(frame % 1000);
      s_oled.drawFastHLine(0, 9, OLED_W, SSD1306_WHITE);

      s_oled.setCursor(0, 13);
      s_oled.print("LED2 : ");
      s_oled.print(led2 ? "ON" : "OFF");

      s_oled.setCursor(0, 24);
      s_oled.printf("Light: %3u%%  %4lumV", light.lightPct, (unsigned long)light.mv);
      s_oled.drawRect(0, 34, OLED_W, 6, SSD1306_WHITE);                 // thanh ánh sáng
      s_oled.fillRect(1, 35, (OLED_W - 2) * light.lightPct / 100, 4, SSD1306_WHITE);

      s_oled.setCursor(0, 44);
      if (led3.on) {
        s_oled.printf("LED3 : ON  %lu.%lus", (unsigned long)(remainMs / 1000),
                      (unsigned long)((remainMs % 1000) / 100));
        s_oled.fillRect(0, 54, (int32_t)OLED_W * remainMs / LED3_ON_MS, 4, SSD1306_WHITE);  // thanh đếm ngược
      } else {
        s_oled.print("LED3 : OFF");
      }
      if (dark) {
        s_oled.setCursor(92, 13);
        s_oled.print("[TOI]");
      }
      s_oled.display();
    } else if (++retry >= 5000 / OLED_PERIOD_MS) {
      // Mất màn hình (lỏng dây...) -> thử lại định kỳ, hệ thống vẫn chạy bình thường
      retry = 0;
      oledOk = oled_begin();
      if (oledOk) xEventGroupSetBits(g_events, EV_OLED_OK);
    }
    frame++;
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(OLED_PERIOD_MS));   // [K3] 500 ms/lần
  }
}
