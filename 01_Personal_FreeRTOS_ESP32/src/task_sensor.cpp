/*
 * task_sensor.cpp - Task 2 (đọc cảm biến ánh sáng) + Task 8 (báo tối)
 */
#include "app.h"

// =====================================================================
// Task 2: đọc cảm biến ánh sáng 500 ms/lần, lưu vào biến toàn cục g_light
// =====================================================================
void task2_sensor(void *pv) {
  bool dark = false;          // trạng thái báo tối cục bộ (có hysteresis)

  app_sync_start(EV_T2);
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    // Đo: trung bình ADC_SAMPLES mẫu để giảm nhiễu
    uint32_t sumRaw = 0, sumMv = 0;
#ifdef SIM_NO_ADC
    // Chỉ dùng khi chạy giả lập QEMU (env qemu_esp32): QEMU không giả lập ADC.
    // Tạo tín hiệu "sáng dần - tối dần" chu kỳ 20 s để thử ngưỡng báo tối của Task 8.
    {
      uint32_t ph = (xTaskGetTickCount() * portTICK_PERIOD_MS / 500) % 40;   // 0..39
      uint32_t r  = (ph < 20 ? ph : 39 - ph) * 4095 / 19;                    // tam giác 0..4095
      sumRaw = r * ADC_SAMPLES;
      sumMv  = r * 3100 / 4095 * ADC_SAMPLES;
    }
#else
    for (int i = 0; i < ADC_SAMPLES; i++) {
      sumRaw += analogRead(PIN_LDR);
      sumMv  += analogReadMilliVolts(PIN_LDR);
    }
#endif
    uint16_t raw = sumRaw / ADC_SAMPLES;
    uint32_t mv  = sumMv / ADC_SAMPLES;
    uint8_t  pct = (uint8_t)((raw * 100UL) / 4095UL);
#if LDR_INVERT
    pct = 100 - pct;
#endif

    // Ghi biến toàn cục dưới mutex [K6]: Task 6 / Task 7 không bao giờ đọc được
    // bản ghi "nửa cũ nửa mới" (5 trường, không thể ghi nguyên tử).
    if (xSemaphoreTake(g_lightMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      g_light.raw      = raw;
      g_light.mv       = mv;
      g_light.lightPct = pct;
      g_light.count++;
      g_light.tick     = xTaskGetTickCount();
      xSemaphoreGive(g_lightMutex);
    } else {
      app_log("T2 khong lay duoc mutex, bo mau nay");
    }
    xEventGroupSetBits(g_events, EV_SENSOR_OK);    // [K9] báo Task 6: đã có dữ liệu thật

    // Báo tối có trễ: chỉ thông báo khi trạng thái ĐỔI -> Task 8 không bị đánh thức vô ích
    bool newDark = dark ? (pct < DARK_OFF_PCT) : (pct < DARK_ON_PCT);
    if (newDark != dark) {
      dark = newDark;
      // [K10] Task notification thay cho queue 1 phần tử: nhanh hơn, không cần tạo đối tượng.
      // Giá trị gửi kèm: bit 8 = trạng thái tối, bit 0..7 = % ánh sáng.
      xTaskNotify(g_task[ID_T8], ((uint32_t)dark << 8) | pct, eSetValueWithOverwrite);
    }

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));   // [K3] đúng 500 ms/lần
  }
}

// =====================================================================
// Task 8: chờ thông báo từ Task 2, cập nhật cờ g_dark và ghi log [K10]
// =====================================================================
void task8_alarm(void *pv) {
  app_sync_start(EV_T8);
  for (;;) {
    uint32_t v = 0;
    // Blocked tới khi Task 2 notify. Xoá toàn bộ giá trị khi thoát (ULONG_MAX).
    xTaskNotifyWait(0, ULONG_MAX, &v, portMAX_DELAY);
    bool dark = (v >> 8) & 1;
    g_dark = dark;
    app_log("T8 %s (anh sang %lu%%)", dark ? "CANH BAO: TOI" : "het toi, binh thuong",
            (unsigned long)(v & 0xFF));
  }
}
