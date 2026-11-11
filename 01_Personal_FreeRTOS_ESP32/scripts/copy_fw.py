# Sau khi build: chép firmware.bin/.elf về <project>/.pio/wokwi/ để Wokwi (wokwi.toml) tìm thấy,
# vì build_dir đã được dời ra ngoài thư mục project (xem platformio.ini).
Import("env")
import os, shutil

def _copy(source, target, env):
    out = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "wokwi")
    os.makedirs(out, exist_ok=True)
    for name in ("firmware.bin", "firmware.elf"):
        src = os.path.join(env.subst("$BUILD_DIR"), name)
        if os.path.exists(src):
            shutil.copy2(src, out)
    print("[copy_fw] firmware -> " + out)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _copy)
