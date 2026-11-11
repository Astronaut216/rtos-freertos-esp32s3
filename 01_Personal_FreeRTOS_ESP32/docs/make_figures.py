"""Vẽ hình cho report: python docs/make_figures.py (chạy trong thư mục project)."""
import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "figures")
os.makedirs(OUT, exist_ok=True)

BLUE, ORANGE, AQUA, YELLOW, MAG, GREEN, VIOLET, RED = (
    "#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948")
INK, INK2, GRID, SURF = "#0b0b0b", "#52514e", "#d9d8d4", "#fcfcfb"
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10, "text.color": INK,
                     "axes.edgecolor": INK2, "axes.labelcolor": INK2,
                     "xtick.color": INK2, "ytick.color": INK2, "figure.facecolor": "white"})


def box(ax, x, y, w, h, text, fc, ec=None, fs=9, tc=INK, bold=False):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
                                fc=fc, ec=ec or fc, lw=1.4))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, color=tc,
            fontweight="bold" if bold else "normal")


def arrow(ax, p, q, color=INK2, text=None, rad=0.0, fs=8, ls="-"):
    ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=12, color=color, lw=1.3,
                                 connectionstyle=f"arc3,rad={rad}", linestyle=ls))
    if text:
        ax.text((p[0] + q[0]) / 2, (p[1] + q[1]) / 2 + 0.12, text, ha="center", va="bottom",
                fontsize=fs, color=INK2, bbox=dict(fc="white", ec="none", pad=0.5))


# ---------------- Hình 1: phần cứng ----------------
def fig_hardware():
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.set_xlim(0, 10); ax.set_ylim(0, 6); ax.axis("off")
    box(ax, 3.6, 1.3, 2.8, 3.4, "ESP32-S3\nDevKitC-1\n\n2 lõi Xtensa LX7\n240 MHz\nFreeRTOS (ESP-IDF 5.5)",
        "#eaf2fc", BLUE, fs=10, bold=False)
    left = [("LED1 (đỏ) + 220Ω", "GPIO12", 4.9), ("LED2 (xanh) + 220Ω", "GPIO13", 4.0),
            ("LED3 (vàng) + 220Ω", "GPIO14", 3.1)]
    for name, pin, y in left:
        box(ax, 0.2, y - 0.3, 2.3, 0.6, name, "#fff4e5", YELLOW, fs=9)
        arrow(ax, (3.6, y), (2.5, y), ORANGE, pin)
    for name, pin, y in [("BTN1 → GND", "GPIO16 (pull-up)", 2.2), ("BTN2 → GND", "GPIO17 (pull-up)", 1.3)]:
        box(ax, 0.2, y - 0.3, 2.3, 0.6, name, "#e9f7f1", AQUA, fs=9)
        arrow(ax, (2.5, y), (3.6, y), AQUA, pin)
    box(ax, 7.5, 3.9, 2.3, 0.8, "Module quang trở\n(AO)", "#f1eefc", VIOLET, fs=9)
    arrow(ax, (7.5, 4.3), (6.4, 4.3), VIOLET)
    ax.text(6.95, 3.95, "GPIO2 / ADC1", ha="center", fontsize=8, color=INK2)
    box(ax, 7.5, 1.6, 2.3, 1.0, "OLED SSD1306\n128×64, I2C 0x3C", "#f1eefc", VIOLET, fs=9)
    arrow(ax, (6.4, 2.25), (7.5, 2.25), VIOLET, "SDA GPIO41")
    arrow(ax, (6.4, 1.8), (7.5, 1.8), VIOLET, "SCL GPIO42")
    box(ax, 3.4, 0.1, 3.2, 0.6, "USB-UART → PC (Serial 115200)", "#f3f3f1", INK2, fs=9)
    ax.plot([5, 5], [0.7, 1.3], color=INK2, lw=1.3)
    ax.set_title("Hình 1. Sơ đồ kết nối phần cứng", fontsize=12, color=INK, pad=4)
    fig.savefig(os.path.join(OUT, "f1_hardware.png"), dpi=170, bbox_inches="tight"); plt.close(fig)


# ---------------- Hình 2: kiến trúc phần mềm ----------------
def fig_software():
    fig, ax = plt.subplots(figsize=(11, 6.4))
    ax.set_xlim(0, 12); ax.set_ylim(0, 7.4); ax.axis("off")
    ax.add_patch(FancyBboxPatch((0.1, 0.2), 4.2, 6.6, boxstyle="round,pad=0.02", fc="#f7f7f5", ec=GRID))
    ax.text(2.2, 6.95, "Core 1 – I/O (đáp ứng nhanh)", ha="center", fontsize=10, color=INK2)
    ax.add_patch(FancyBboxPatch((7.6, 0.2), 4.3, 6.6, boxstyle="round,pad=0.02", fc="#f7f7f5", ec=GRID))
    ax.text(9.75, 6.95, "Core 0 – nền", ha="center", fontsize=10, color=INK2)

    T = {}
    def task(key, x, y, text, c):
        box(ax, x, y, 3.6, 0.85, text, "white", c, fs=8.5); T[key] = (x, y)
    task("t1", 0.4, 5.6, "T1 led1 · prio 2\nvTaskDelayUntil 100 ms", BLUE)
    task("t3", 0.4, 4.3, "T3 btn1 · prio 3\nquét 10 ms → đảo LED2", BLUE)
    task("t4", 0.4, 3.0, "T4 btn2 · prio 3\nquét 10 ms → give sem", BLUE)
    task("t5", 0.4, 1.7, "T5 led3 · prio 4\ntake sem → LED3 3 s", BLUE)
    task("t2", 7.9, 5.6, "T2 sensor · prio 2\nADC 500 ms → g_light", ORANGE)
    task("t8", 7.9, 4.3, "T8 alarm · prio 3\nchờ notification", ORANGE)
    task("t6", 7.9, 3.0, "T6 oled · prio 1\nvẽ OLED 500 ms", ORANGE)
    task("t7", 7.9, 1.7, "T7 console · prio 1\nin log + nhận lệnh", ORANGE)
    box(ax, 7.9, 0.45, 3.6, 0.85, "Timer daemon\nsoftware timer 10 s", "white", INK2, fs=8.5)

    # đối tượng kernel ở giữa
    def obj(x, y, text, c):
        box(ax, x, y, 2.6, 0.75, text, c, c, fs=8.5, tc="white", bold=True)
    obj(4.7, 5.65, "Mutex\ng_lightMutex", RED)
    obj(4.7, 4.35, "Event group\nđiểm hẹn 8 task", YELLOW)
    obj(4.7, 3.05, "Binary sem\ng_btn2Sem", GREEN)
    obj(4.7, 1.75, "Spinlock\ng_led3Lock", MAG)
    obj(4.7, 0.45, "Queue log (16)\n← mọi task app_log()", VIOLET)

    arrow(ax, (7.9, 6.02), (7.3, 6.02), RED, "ghi")
    arrow(ax, (7.3, 5.75), (7.9, 3.6), RED)
    ax.text(7.62, 4.95, "đọc", fontsize=8, color=INK2, ha="center")
    arrow(ax, (4.0, 3.42), (4.7, 3.42), GREEN, "give")
    arrow(ax, (4.7, 3.1), (4.0, 2.4), GREEN)
    ax.text(4.25, 2.6, "take", fontsize=8, color=INK2)
    arrow(ax, (4.0, 2.1), (4.7, 2.1), MAG, "ghi")
    arrow(ax, (7.3, 2.3), (7.9, 3.05), MAG)
    ax.text(7.5, 2.75, "đọc", fontsize=8, color=INK2)
    arrow(ax, (7.3, 0.85), (7.9, 1.85), VIOLET)
    ax.text(7.45, 1.45, "nhận", fontsize=8, color=INK2)
    arrow(ax, (9.7, 5.6), (9.7, 5.15), ORANGE)
    ax.text(9.85, 5.33, "xTaskNotify", fontsize=8, color=INK2)
    arrow(ax, (9.7, 1.3), (9.7, 1.7), INK2)
    ax.set_title("Hình 2. Kiến trúc phần mềm: 8 task, 2 lõi và đối tượng đồng bộ", fontsize=12, pad=4)
    fig.savefig(os.path.join(OUT, "f2_software.png"), dpi=170, bbox_inches="tight"); plt.close(fig)


# ---------------- Hình 3: trạng thái task ----------------
def fig_states():
    fig, ax = plt.subplots(figsize=(8, 4.2))
    ax.set_xlim(0, 10); ax.set_ylim(0, 5); ax.axis("off")
    S = {"Ready": (1, 3.2), "Running": (6.2, 3.2), "Blocked": (3.6, 0.5), "Suspended": (6.8, 0.5)}
    col = {"Ready": BLUE, "Running": GREEN, "Blocked": ORANGE, "Suspended": INK2}
    for k, (x, y) in S.items():
        box(ax, x, y, 2.4, 0.9, k, col[k], col[k], fs=11, tc="white", bold=True)
    arrow(ax, (3.4, 3.85), (6.2, 3.85), INK2, "scheduler chọn (prio cao nhất)")
    arrow(ax, (6.2, 3.45), (3.4, 3.45), INK2, "bị task prio cao hơn chiếm")
    arrow(ax, (6.6, 3.2), (5.4, 1.4), INK2, "vTaskDelay / Take / Wait", rad=0.1)
    arrow(ax, (3.9, 1.4), (2.4, 3.2), INK2, "hết giờ / give / notify", rad=0.1)
    arrow(ax, (7.8, 3.2), (8.0, 1.4), INK2, "vTaskSuspend")
    arrow(ax, (6.8, 0.8), (3.4, 3.3), INK2, rad=-0.35, ls="--")
    ax.text(4.9, 1.75, "vTaskResume", fontsize=8, color=INK2, rotation=0,
            bbox=dict(fc="white", ec="none", pad=0.5))
    ax.set_title("Hình 3. Các trạng thái task trong FreeRTOS", fontsize=12, pad=4)
    fig.savefig(os.path.join(OUT, "f3_states.png"), dpi=170, bbox_inches="tight"); plt.close(fig)


# ---------------- Hình 4: dòng thời gian từ log QEMU ----------------
def fig_timeline():
    log = open(os.path.join(HERE, "qemu_log.txt"), encoding="utf-8").read().splitlines()
    on, off, dark = [], [], []
    for l in log:
        m = re.match(r"\[\s*(\d+)\] (.*)", l)
        if not m: continue
        t, s = int(m.group(1)) / 1000, m.group(2)
        if "LED3 ON" in s: on.append(t)
        if "LED3 OFF" in s: off.append(t)
        if "CANH BAO" in s: dark.append((t, 1))
        if "het toi" in s: dark.append((t, 0))
    gives = []
    seen = 0
    for l in log:   # lệnh give không có timestamp: gán = lần LED3 ON gần nhất (minh hoạ)
        if l.startswith("give sem"):
            gives.append("OK" in l)
    fig, ax = plt.subplots(figsize=(9, 3))
    for a, b in zip(on, off):
        ax.broken_barh([(a, b - a)], (1.6, 0.6), color=YELLOW, edgecolor="white", linewidth=2)
        ax.text((a + b) / 2, 1.9, f"{(b - a) * 1000:.0f} ms", ha="center", va="center", fontsize=8)
    st, t0 = 0, 0
    for t, v in dark + [(20, None)]:
        if st: ax.broken_barh([(t0, t - t0)], (0.6, 0.6), color=INK2)
        if v is not None: st, t0 = v, t
    ax.set_yticks([0.9, 1.9]); ax.set_yticklabels(["g_dark (T8)", "LED3 (T5)"])
    ax.set_xlim(0, 20); ax.set_ylim(0.3, 2.5); ax.set_xlabel("thời gian (s)")
    ax.grid(axis="x", color=GRID, lw=0.6); ax.set_axisbelow(True)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    ax.set_title("Hình 4. Dòng thời gian đo từ log (3 lệnh give liên tiếp → LED3 sáng 2 lần × 3 s)",
                 fontsize=11, pad=6)
    fig.savefig(os.path.join(OUT, "f4_timeline.png"), dpi=170, bbox_inches="tight"); plt.close(fig)


# ---------------- Hình 5: stack còn dư ----------------
def fig_stack():
    names = ["T1_led1", "T2_sensor", "T3_btn1", "T4_btn2", "T5_led3", "T6_oled", "T7_console", "T8_alarm"]
    size = [3072, 3072, 3072, 3072, 3072, 4096, 4096, 3072]
    free = [2488, 2484, 2496, 2492, 1684, 2680, 2492, 1672]
    used = [s - f for s, f in zip(size, free)]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    y = range(len(names))[::-1]
    ax.barh(y, used, color=BLUE, height=0.55, label="đã dùng (đỉnh)")
    ax.barh(y, free, left=used, color="#c9dcf3", height=0.55, label="còn dư (high-water mark)")
    for yy, u, s in zip(y, used, size):
        ax.text(s + 40, yy, f"{u}/{s} B", va="center", fontsize=8, color=INK2)
    ax.set_yticks(list(y)); ax.set_yticklabels(names)
    ax.set_xlim(0, 4800); ax.set_xlabel("byte")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.18), ncol=2, fontsize=8, frameon=False)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    ax.set_title("Hình 5. Stack từng task đo bằng uxTaskGetStackHighWaterMark (lệnh stat)", fontsize=11)
    fig.savefig(os.path.join(OUT, "f5_stack.png"), dpi=170, bbox_inches="tight"); plt.close(fig)


if __name__ == "__main__":
    fig_hardware(); fig_software(); fig_states(); fig_timeline(); fig_stack()
    print("OK ->", OUT)
