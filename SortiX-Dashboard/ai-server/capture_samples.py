"""
SortiX-Med - Cong cu chup anh mau thuc te sieu nhanh tu Camera iPhone (DroidCam)

Phim tat trong cua so video:
  - Phim 1: Chon thu muc 'syringe'    (Kim tiem)
  - Phim 2: Chon thu muc 'scissor'    (Keo)
  - Phim 3: Chon thu muc 'cotton'     (Bong gon)
  - Phim 4: Chon thu muc 'vial'       (Ong nghiem / Lo thuoc)
  - Phim 5: Chon thu muc 'mixed'      (Nhieu mon dat chung)
  - Phim 0: Chon thu muc 'background' (Nen bang tai trong)
  - Phim Space: Chup 1 anh thu cong
  - Phim A: Bat / Tat che do TU DONG CHUP moi 0.35 giay (vua xoay vat vua chup)
  - Phim Q: Thoat
"""

import os
import sys
import time
from pathlib import Path

import cv2
import numpy as np

BASE_DIR = Path(__file__).resolve().parent
OUTPUT_ROOT = BASE_DIR / "raw_captures"

CAMERA_SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("CAMERA_SOURCE", "1")
CAMERA_ROTATE = int(os.environ.get("CAMERA_ROTATE", "90"))
AUTO_INTERVAL = 0.35
RECONNECT_FAIL_THRESHOLD = 15
RECONNECT_INTERVAL = 1.5

CLASSES = {
    ord("1"): "syringe",
    ord("2"): "scissor",
    ord("3"): "cotton",
    ord("4"): "vial",
    ord("5"): "mixed",
    ord("0"): "background",
}


def rotate_frame(frame):
    if CAMERA_ROTATE == 90:
        return cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
    if CAMERA_ROTATE == 180:
        return cv2.rotate(frame, cv2.ROTATE_180)
    if CAMERA_ROTATE == 270:
        return cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
    return frame


def count_images(folder: Path) -> int:
    if not folder.exists():
        return 0
    return sum(1 for p in folder.iterdir() if p.suffix.lower() in (".jpg", ".jpeg", ".png"))


def open_camera(source: str) -> cv2.VideoCapture | None:
    """Mo ket noi camera qua index USB/DroidCam hoac URL Wi-Fi."""
    if str(source).isdigit():
        cap = cv2.VideoCapture(int(source), cv2.CAP_MSMF)
    else:
        cap = cv2.VideoCapture(str(source))
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    if cap.isOpened():
        return cap
    cap.release()
    return None


def make_reconnecting_frame(source: str) -> np.ndarray:
    """Tao khung hinh cho khi dang mat ket noi va tu dong ket noi lai camera."""
    canvas = np.zeros((640, 480, 3), dtype=np.uint8)
    cv2.putText(canvas, "MAT KET NOI CAMERA!", (55, 270),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 80, 255), 2)
    cv2.putText(canvas, "Dang tu dong ket noi lai...", (70, 315),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 220, 255), 2)
    cv2.putText(canvas, f"Nguon: {str(source)[:42]}", (25, 360),
                cv2.FONT_HERSHEY_SIMPLEX, 0.45, (180, 180, 180), 1)
    cv2.putText(canvas, "Nhan Q de thoat", (155, 610),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (200, 255, 200), 1)
    return canvas


def main() -> None:
    for name in CLASSES.values():
        (OUTPUT_ROOT / name).mkdir(parents=True, exist_ok=True)

    cap = open_camera(CAMERA_SOURCE)
    if cap is None:
        print(f"[!] Chua mo duoc camera ({CAMERA_SOURCE}), se tu dong thu ket noi lai...")

    counts = {name: count_images(OUTPUT_ROOT / name) for name in CLASSES.values()}
    current_class = "syringe"
    auto_capture = False
    last_auto_time = 0.0
    flash_until = 0.0
    fail_count = 0
    last_reconnect_attempt = 0.0

    print("\n=== SORTIX - CHUP ANH MAU THUC TE ===")
    print("Thu muc luu anh:", OUTPUT_ROOT)
    print("Phim 1: syringe | 2: scissor | 3: cotton | 4: vial | 5: mixed | 0: background")
    print("Phim SPACE: chup 1 tam | Phim A: bat/tat tu dong chup | Phim Q: thoat\n")

    while True:
        now = time.time()
        ret, frame = (cap.read() if cap is not None else (False, None))

        if not ret or frame is None:
            fail_count += 1
            if cap is not None and fail_count >= RECONNECT_FAIL_THRESHOLD:
                print("[!] Mat tin hieu camera, dang thu ket noi lai...")
                cap.release()
                cap = None
                auto_capture = False

            if cap is None and (now - last_reconnect_attempt >= RECONNECT_INTERVAL):
                last_reconnect_attempt = now
                cap = open_camera(CAMERA_SOURCE)
                if cap is not None:
                    fail_count = 0
                    print("[+] Da ket noi lai camera thanh cong!")

            cv2.imshow("SORTIX - CAPTURE SAMPLES", make_reconnecting_frame(CAMERA_SOURCE))
            key = cv2.waitKey(30) & 0xFF
            if key in (ord("q"), ord("Q")):
                break
            continue

        fail_count = 0
        frame = rotate_frame(frame)
        save_dir = OUTPUT_ROOT / current_class

        if auto_capture and (now - last_auto_time >= AUTO_INTERVAL):
            filename = save_dir / f"{current_class}_{int(now * 1000)}.jpg"
            if cv2.imwrite(str(filename), frame, [int(cv2.IMWRITE_JPEG_QUALITY), 95]):
                counts[current_class] += 1
                flash_until = now + 0.12
            last_auto_time = now

        preview = frame.copy()
        total = counts[current_class]
        status = "AUTO: ON (0.35s)" if auto_capture else "AUTO: OFF (bam A de bat)"
        color = (0, 255, 0) if auto_capture else (0, 220, 255)

        cv2.putText(preview, f"CLASS: {current_class.upper()} ({total} anh)", (15, 32),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.72, (0, 255, 180), 2)
        cv2.putText(preview, status, (15, 62),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
        cv2.putText(preview, "1:syringe 2:scissor 3:cotton 4:vial 5:mixed 0:bg", (12, preview.shape[0] - 32),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.42, (255, 255, 255), 1)
        cv2.putText(preview, "SPACE: Chup 1 tam | A: Tu dong | Q: Thoat", (12, preview.shape[0] - 12),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.42, (200, 255, 200), 1)
        if now < flash_until:
            cv2.rectangle(preview, (4, 4), (preview.shape[1] - 4, preview.shape[0] - 4), (0, 255, 0), 4)

        cv2.imshow("SORTIX - CAPTURE SAMPLES", preview)
        key = cv2.waitKey(1) & 0xFF

        if key in (ord("q"), ord("Q")):
            break
        elif key in CLASSES:
            current_class = CLASSES[key]
            auto_capture = False
            print(f"[*] Chuyen sang class: {current_class} ({counts[current_class]} anh)")
        elif key in (ord("a"), ord("A")):
            auto_capture = not auto_capture
            last_auto_time = 0.0
            print(f"[*] Auto capture: {'BAT' if auto_capture else 'TAT'}")
        elif key == 32:  # Space
            filename = save_dir / f"{current_class}_{int(time.time() * 1000)}.jpg"
            if cv2.imwrite(str(filename), frame, [int(cv2.IMWRITE_JPEG_QUALITY), 95]):
                counts[current_class] += 1
                flash_until = time.time() + 0.12
                print(f"[+] Da luu ({counts[current_class]}): {filename.name}")

    if cap is not None:
        cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
