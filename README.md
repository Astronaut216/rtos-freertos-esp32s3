# Đồ án RTOS – HK1 2026-2027, đợt 2 (GVHD: Huỳnh Hoàng Hà)

**Sinh viên:** Đặng Đình Mạnh – MSSV 24119055

| Project | Tỷ trọng | Thư mục |
|---|---|---|
| Cá nhân: FreeRTOS trên ESP32 | 70% | [`01_Personal_FreeRTOS_ESP32/`](01_Personal_FreeRTOS_ESP32/) |
| Nhóm: kiến trúc HW cho model AI nhẹ trên kit SoC (NPU DM-SA) | 30% | [`02_Group_HW_AI_Accelerator/`](02_Group_HW_AI_Accelerator/) |
| Báo cáo + slide tiến độ | – | [`03_Reports/`](03_Reports/) |

**Thành viên nhóm (project 30%):** Lê Ngọc Hoàn (24119038), Đặng Đình Mạnh (24119055). Thư mục `02_Group_HW_AI_Accelerator/` là bài chung của nhóm, giống nhau giữa các thành viên.

```
RTOS_Project/
├── 01_Personal_FreeRTOS_ESP32/   PlatformIO: 8 task + 1 software timer, 17 kỹ thuật FreeRTOS, cảm biến BH1750
│   ├── src/                      main.cpp, task_io.cpp, task_sensor.cpp, task_display.cpp, task_control.cpp
│   ├── include/                  config.h (chân, chu kỳ, priority, stack), app.h (đối tượng dùng chung)
│   ├── docs/                     hình cho report, log chạy QEMU, script vẽ hình
│   ├── prebuilt/                 firmware ESP32-S3 nạp sẵn ở địa chỉ 0x0
│   └── diagram.json, wokwi.toml  mô phỏng Wokwi
├── 02_Group_HW_AI_Accelerator/   (nhóm) literature → model Python QAT → golden C → spec → RTL → PetaLinux
└── 03_Reports/
    ├── Report_Project_CaNhan_FreeRTOS_24119055.docx / .pdf
    ├── Slide_Tuan2_ProjectCaNhan.pptx   chạy task 1–3
    ├── Slide_Tuan3_ProjectCaNhan.pptx   chạy task 4–6 + đề xuất kỹ thuật
    ├── Slide_Tuan4_ProjectCaNhan.pptx   kỹ thuật mới + kết quả đo
    └── Slide_Project_NPU_DMSA.pptx      slide project nhóm
```

## Trạng thái kiểm chứng (project cá nhân)

| Hạng mục | Trạng thái | Cách kiểm tra |
|---|---|---|
| Build ESP32-S3 / ESP32 | OK, không cảnh báo (`-Wall`). RAM 6,8 %, Flash 12,0 % (S3) | `pio run -e esp32s3` |
| Chạy giả lập QEMU (ESP32) | OK: 8 task qua điểm hẹn, LED3 sáng đúng 3000 ms, semaphore gộp lần nhấn, báo tối 20/40 lx, queue set (lệnh + timer), suspend/resume/prio, log bỏ = 0 | `docs/qemu_log.txt` |
| Chạy trên kit thật | Chờ chạy: điền cột "Kết quả" Bảng 6 trong report | xem `01_.../README.md` |
