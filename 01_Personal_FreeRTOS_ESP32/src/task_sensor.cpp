/*
 * task_sensor.cpp - Task 2 (đọc cảm biến ánh sáng BH1750) + Task 8 (báo tối)
 *
 * BH1750 (module GY-302) nằm CHUNG bus I2C với OLED của Task 6 -> mọi lần dùng bus phải giữ
 * g_i2cMutex [K6c].
 */
#include "app.h"
#include <Wire.h>

// ---------------- Driver BH1750 tối giản (theo datasheet ROHM) ----------------
// Lệnh: 0x01 = Power On, 0x10 = đo liên tục độ phân giải cao (1 lx, mỗi lần đo ~120 ms).
// Đọc 2 byte -> raw (big-endian), lux = raw / 1.2.
static bool bh1750_cmd(uint8_t cmd) {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

static bool bh1750_init() {
  return bh1750_cmd(0x01) && bh1750_cmd(0x10);
}

static bool bh1750_read(uint16_t *raw) {
  if (Wire.requestFrom((uint8_t)BH1750_ADDR, (uint8_t)2) != 2) return false;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  *raw = ((uint16_t)hi << 8) | lo;
  return true;
}

// Đọc 1 mẫu. Toàn bộ chuỗi giao dịch I2C (khởi tạo + đọc) nằm trong 1 lần giữ mutex.
static bool read_light(uint16_t *raw) {
  static bool inited = false;
  bool ok = false;
  if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(I2C_WAIT_MS)) != pdTRUE) {
    return false;                            // bus đang bận quá lâu: bỏ lượt này
  }
  if (!inited) inited = bh1750_init();       // lần đầu, hoặc sau khi mất kết nối
  if (inited) ok = bh1750_read(raw);
  if (!ok) inited = false;                   // lỗi -> lần sau khởi tạo lại (cắm lại dây là chạy)
  xSemaphoreGive(g_i2cMutex);
#ifdef SIM_NO_SENSOR
  // Chỉ dùng khi chạy giả lập QEMU (env qemu_esp32): QEMU không có BH1750 nên giao dịch I2C
  // ở trên luôn lỗi (vẫn đi qua mutex như thật). Thay bằng ánh sáng giả "sáng dần - tối dần"
  // 0..1000 lx, chu kỳ 20 s, có đoạn tối ~4 s quanh đáy để thử ngưỡng báo tối.
  uint32_t ph  = (xTaskGetTickCount() * portTICK_PERIOD_MS / 500) % 40;   // 0..39
  uint32_t tri = ph < 20 ? ph : 39 - ph;                                  // 0..19..0
  *raw = (uint16_t)(tri > 4 ? (tri - 4) * 80 : 0);                        // 0..1200 (0..1000 lx)
  ok = true;
#endif
  return ok;
}

// =====================================================================
// Task 2: đọc cảm biến ánh sáng 500 ms/lần, lưu vào biến toàn cục g_light
// =====================================================================
void task2_sensor(void *pv) {
  bool dark = false;          // trạng thái báo tối cục bộ (có hysteresis)
  bool lastOk = true;

  app_sync_start(EV_T2);
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    uint16_t raw = 0;
    bool ok = read_light(&raw);
    float lux = raw / 1.2f;
    uint8_t bar = (uint8_t)(lux >= LUX_BAR_MAX ? 100 : lux * 100 / LUX_BAR_MAX);

    // Ghi biến toàn cục dưới mutex [K6]: Task 6 / Task 7 không bao giờ đọc được
    // bản ghi "nửa cũ nửa mới" (nhiều trường, không thể ghi nguyên tử).
    if (xSemaphoreTake(g_lightMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      g_light.ok = ok;
      if (ok) {
        g_light.raw    = raw;
        g_light.lux    = lux;
        g_light.barPct = bar;
        g_light.count++;
      } else {
        g_light.errors++;
      }
      g_light.tick = xTaskGetTickCount();
      xSemaphoreGive(g_lightMutex);
    } else {
      app_log("T2 khong lay duoc mutex g_light, bo mau nay");
    }
    if (ok != lastOk) {            // chỉ log khi trạng thái cảm biến ĐỔI, không spam mỗi 500 ms
      app_log(ok ? "T2 BH1750 OK tai 0x%02X" : "T2 khong doc duoc BH1750 (0x%02X), kiem tra day",
              BH1750_ADDR);
      lastOk = ok;
    }
    if (ok) {
      xEventGroupSetBits(g_events, EV_SENSOR_OK);    // [K9] báo Task 6: đã có dữ liệu thật

      // Báo tối có trễ: chỉ thông báo khi trạng thái ĐỔI -> Task 8 không bị đánh thức vô ích
      bool newDark = dark ? (lux < DARK_OFF_LUX) : (lux < DARK_ON_LUX);
      if (newDark != dark) {
        dark = newDark;
        // [K10] Task notification thay cho queue 1 phần tử: nhanh hơn, không cần tạo đối tượng.
        // Giá trị gửi kèm: bit 16 = trạng thái tối, bit 0..15 = lux.
        uint32_t luxInt = lux > 65535 ? 65535 : (uint32_t)lux;
        xTaskNotify(g_task[ID_T8], ((uint32_t)dark << 16) | luxInt, eSetValueWithOverwrite);
      }
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
    bool dark = (v >> 16) & 1;
    g_dark = dark;
    app_log("T8 %s (anh sang %lu lx)", dark ? "CANH BAO: TOI" : "het toi, binh thuong",
            (unsigned long)(v & 0xFFFF));
  }
}
