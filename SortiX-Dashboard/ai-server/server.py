"""
SortiX-Med - AI Camera Server (YOLO11 + MQTT + FastAPI MJPEG Video Stream)

Chay dong thoi:
  1. Doc Camera iPhone DroidCam (URL Wi-Fi http://192.168.137.231:4747/video hoac USB index 1)
  2. Nhan dien YOLO11 va gui JSON len MQTT topic sorter/sorter_01/vision
  3. Phat luong MJPEG tai http://localhost:8000/video_feed de nhung len Web Dashboard
"""

import json
import os
import sys
import threading
import time
from pathlib import Path

import cv2
import paho.mqtt.client as mqtt_client
import torch
import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, StreamingResponse
from ultralytics import YOLO

try:
    from dotenv import load_dotenv
    _env_local = Path(__file__).resolve().parent.parent / ".env.local"
    if _env_local.exists():
        load_dotenv(_env_local, override=False)
except ImportError:
    pass

# ==========================================
# 1. CAU HINH
# ==========================================

BASE_DIR = Path(__file__).resolve().parent
DEFAULT_IPHONE_URL = "http://192.168.137.231:4747/video"
CAMERA_SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("CAMERA_SOURCE", DEFAULT_IPHONE_URL)
CAMERA_BACKEND = cv2.CAP_MSMF
# Xoay khung hinh mac dinh 90 do cho iPhone DroidCam (co the doi qua bien CAMERA_ROTATE: 0, 90, 180, 270)
CAMERA_ROTATE = int(os.environ.get("CAMERA_ROTATE", "90"))
STREAM_PORT = int(os.environ.get("AI_STREAM_PORT", "8000"))

IMAGE_SIZE = 640
CONFIDENCE = 0.6
PUBLISH_INTERVAL = 2.5
ABSENCE_RESET_SEC = 0.6
VISION_STABLE_FRAMES = int(os.environ.get("VISION_STABLE_FRAMES", "2"))
JPEG_QUALITY = 85
ENABLE_ANTI_BACKLIGHT = os.environ.get("ANTI_BACKLIGHT", "1").strip() != "0"

MQTT_BROKER = os.environ.get("MQTT_BROKER_URL", "mqtts://broker.emqx.io:8883")
MQTT_USERNAME = os.environ.get("MQTT_USERNAME", "")
MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "")
SORTIX_CONTROL_SECRET = (
    os.environ.get("SORTIX_CONTROL_SECRET")
    or os.environ.get("MQTT_CONTROL_SECRET")
    or "sortix_sec_control_2026"
)

TOPIC_VISION = "sorter/sorter_01/vision"
TOPIC_STATUS = "sorter/sorter_01/status"

CLASS_TO_BRAND_ID = {
    "syringe": "med_syringe",
    "med_syringe": "med_syringe",
    "scissor": "med_scissors",
    "scissors": "med_scissors",
    "med_scissors": "med_scissors",
    "cotton": "med_vial",  # Tam map ve med_vial (Khay 3) de khop UI Dashboard khi chay test
    "med_cotton": "med_vial",
    "vial": "med_vial",
    "med_vial": "med_vial",
    "forceps": "med_forceps",
    "med_forceps": "med_forceps",
}


def _parse_broker(url: str) -> tuple[str, int]:
    cleaned = url.strip()
    for prefix in ("mqtts://", "mqtt://"):
        if cleaned.startswith(prefix):
            cleaned = cleaned[len(prefix):]
    if ":" in cleaned:
        host, port_str = cleaned.rsplit(":", 1)
        return host, int(port_str)
    return cleaned, 1883


def map_class_to_brand_id(class_name: str) -> str:
    normalized = str(class_name or "").strip().lower()
    return CLASS_TO_BRAND_ID.get(normalized, normalized)


def resolve_model_path() -> Path:
    run_best = BASE_DIR / "runs" / "sortix_med_3class" / "weights" / "best.pt"
    deploy_best = BASE_DIR / "weights" / "best.pt"
    if run_best.exists() and (
        not deploy_best.exists() or run_best.stat().st_mtime >= deploy_best.stat().st_mtime
    ):
        return run_best
    if deploy_best.exists():
        return deploy_best
    models = [p for p in BASE_DIR.rglob("best.pt") if ".venv" not in p.parts]
    if models:
        return models[0]
    raise FileNotFoundError("Khong tim thay file best.pt trong ai-server!")


# ==========================================
# 2. TRANG THAI CHIA SE GIUA THREAD
# ==========================================

frame_lock = threading.Lock()
frame_cond = threading.Condition(frame_lock)
latest_jpeg: bytes | None = None
latest_jpeg_seq = 0
raw_lock = threading.Lock()
latest_raw_frame = None
latest_raw_seq = 0
stop_event = threading.Event()
mqtt_connected = False
current_fps = 0.0
last_detected: list[str] = []


def _rotate_frame(frame):
    if CAMERA_ROTATE == 90:
        return cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
    if CAMERA_ROTATE == 180:
        return cv2.rotate(frame, cv2.ROTATE_180)
    if CAMERA_ROTATE == 270:
        return cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
    return frame


_CLAHE = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))


def enhance_for_backlight(frame):
    """Can bang sang cuc bo (LAB CLAHE) de giam nguoc sang / bong toi va choi den."""
    if not ENABLE_ANTI_BACKLIGHT:
        return frame
    lab = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
    l_chan, a_chan, b_chan = cv2.split(lab)
    l_enhanced = _CLAHE.apply(l_chan)
    merged = cv2.merge((l_enhanced, a_chan, b_chan))
    return cv2.cvtColor(merged, cv2.COLOR_LAB2BGR)


# ==========================================
# 3. MQTT CLIENT
# ==========================================

MQTT_HOST, MQTT_PORT = _parse_broker(MQTT_BROKER)
_client_kwargs: dict = {
    "client_id": f"sortix_ai_stream_{int(time.time())}",
    "protocol": mqtt_client.MQTTv311,
}
if hasattr(mqtt_client, "CallbackAPIVersion"):
    _client_kwargs["callback_api_version"] = mqtt_client.CallbackAPIVersion.VERSION1

mqtt_cli = mqtt_client.Client(**_client_kwargs)
if MQTT_BROKER.strip().lower().startswith("mqtts://"):
    mqtt_cli.tls_set()
if MQTT_USERNAME:
    mqtt_cli.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD or None)


def _on_connect(client, userdata, flags, rc):
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        print(f"[MQTT] Ket noi thanh cong toi {MQTT_HOST}:{MQTT_PORT}")
        client.publish(
            TOPIC_STATUS,
            json.dumps({
                "source": "ai_server",
                "status": "online",
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            }),
            qos=0,
            retain=True,
        )
    else:
        print(f"[MQTT] Ket noi that bai (rc={rc})")


def _on_disconnect(client, userdata, rc):
    global mqtt_connected
    mqtt_connected = False


mqtt_cli.on_connect = _on_connect
mqtt_cli.on_disconnect = _on_disconnect


def publish_vision(detections: list[dict]) -> None:
    if not mqtt_connected or not detections:
        return
    top = max(detections, key=lambda item: float(item.get("confidence", 0.0)))
    raw_class = str(top.get("class_name", "")).strip()
    brand_id = map_class_to_brand_id(raw_class)
    now_iso = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    payload = {
        "product_id": f"cam_{raw_class.lower() or 'item'}_{int(time.time() * 1000)}",
        "brand_id": brand_id,
        "class_name": raw_class,
        "confidence": top["confidence"],
        "catalog_version": "catalog_01",
        "timestamp": now_iso,
        "auth_token": SORTIX_CONTROL_SECRET,
        "source": "ai_server",
        "count": len(detections),
        "detections": detections,
        "top": top,
    }
    mqtt_cli.publish(TOPIC_VISION, json.dumps(payload), qos=0)


# ==========================================
# 4. VONG LAP CAMERA + YOLO11 (BACKGROUND)
# ==========================================

CAMERA_RECONNECT_DELAY = 2.0
CAMERA_MAX_READ_FAILS = 30
CAMERA_TIMEOUT_MS = 5000


def _open_capture() -> cv2.VideoCapture | None:
    """Mo ket noi toi camera iPhone (URL HTTP/RTSP hoac USB index)."""
    if str(CAMERA_SOURCE).isdigit():
        cap = cv2.VideoCapture(int(CAMERA_SOURCE), CAMERA_BACKEND)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    else:
        cap = cv2.VideoCapture(str(CAMERA_SOURCE))

    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    if hasattr(cv2, "CAP_PROP_OPEN_TIMEOUT_MSEC"):
        cap.set(cv2.CAP_PROP_OPEN_TIMEOUT_MSEC, CAMERA_TIMEOUT_MS)
    if hasattr(cv2, "CAP_PROP_READ_TIMEOUT_MSEC"):
        cap.set(cv2.CAP_PROP_READ_TIMEOUT_MSEC, CAMERA_TIMEOUT_MS)

    if not cap.isOpened():
        cap.release()
        return None
    return cap


def camera_reader_worker() -> None:
    """Doc lien tuc de xa buffer OpenCV, tu dong ket noi lai khi mat luong camera."""
    global latest_raw_frame, latest_raw_seq
    cap: cv2.VideoCapture | None = None
    fail_count = 0

    try:
        while not stop_event.is_set():
            if cap is None:
                cap = _open_capture()
                if cap is None:
                    print(
                        f"[CAM] Khong mo duoc camera ({CAMERA_SOURCE}), "
                        f"thu lai sau {CAMERA_RECONNECT_DELAY:.0f}s..."
                    )
                    stop_event.wait(CAMERA_RECONNECT_DELAY)
                    continue
                print(f"[CAM] Da ket noi camera: {CAMERA_SOURCE}")
                fail_count = 0

            ret, frame = cap.read()
            if not ret or frame is None:
                fail_count += 1
                if fail_count >= CAMERA_MAX_READ_FAILS:
                    print("[CAM] Mat ket noi luong camera, dang thu ket noi lai...")
                    cap.release()
                    cap = None
                    stop_event.wait(CAMERA_RECONNECT_DELAY)
                else:
                    time.sleep(0.02)
                continue

            fail_count = 0
            with raw_lock:
                latest_raw_frame = frame
                latest_raw_seq += 1
    finally:
        if cap is not None:
            cap.release()


def camera_worker() -> None:
    global latest_jpeg, latest_jpeg_seq, current_fps, last_detected

    model_path = resolve_model_path()
    device = 0 if torch.cuda.is_available() else "cpu"
    print(f"[AI] Dang tai model: {model_path} (device={device})")
    model = YOLO(str(model_path))

    try:
        mqtt_cli.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        mqtt_cli.loop_start()
    except Exception as exc:
        print(f"[MQTT] Canh bao: Khong ket noi duoc MQTT ({exc})")

    print(f"[CAM] Dang ket noi nguon camera iPhone: {CAMERA_SOURCE}")
    reader = threading.Thread(target=camera_reader_worker, daemon=True)
    reader.start()

    print(f"[STREAM] San sang phat tai: http://localhost:{STREAM_PORT}/video_feed")
    prev_time = time.time()
    last_publish_time = 0.0
    last_published_brand: str | None = None
    last_seen_any_time = 0.0
    last_processed_seq = 0
    candidate_brand: str | None = None
    candidate_streak = 0

    while not stop_event.is_set():
        with raw_lock:
            if latest_raw_frame is None or latest_raw_seq == last_processed_seq:
                frame = None
            else:
                frame = latest_raw_frame.copy()
                last_processed_seq = latest_raw_seq

        if frame is None:
            time.sleep(0.005)
            continue

        frame = _rotate_frame(frame)
        ai_frame = enhance_for_backlight(frame)

        results = model.predict(
            source=ai_frame,
            imgsz=IMAGE_SIZE,
            conf=CONFIDENCE,
            device=device,
            verbose=False,
        )

        annotated = results[0].plot()
        detection_dicts: list[dict] = []
        detected_labels: list[str] = []

        for box in results[0].boxes:
            class_id = int(box.cls[0])
            confidence = float(box.conf[0])
            class_name = model.names[class_id]
            try:
                bbox = [round(float(v), 1) for v in box.xyxy[0]]
            except Exception:
                bbox = []
            detected_labels.append(f"{class_name}: {confidence:.2f}")
            detection_dicts.append({
                "class_id": class_id,
                "class_name": class_name,
                "confidence": round(confidence, 4),
                "bbox": bbox,
            })

        now = time.time()
        current_fps = 1.0 / max(now - prev_time, 0.0001)
        prev_time = now
        last_detected = detected_labels

        if detection_dicts:
            top_item = max(detection_dicts, key=lambda item: float(item.get("confidence", 0.0)))
            top_brand = map_class_to_brand_id(str(top_item.get("class_name", "")))
            if top_brand == candidate_brand:
                candidate_streak += 1
            else:
                candidate_brand = top_brand
                candidate_streak = 1

            is_stable = candidate_streak >= max(1, VISION_STABLE_FRAMES)
            is_new_after_gap = (now - last_seen_any_time) >= ABSENCE_RESET_SEC
            is_brand_changed = top_brand != last_published_brand
            is_cooldown_elapsed = (now - last_publish_time) >= PUBLISH_INTERVAL

            if is_stable and (is_new_after_gap or is_brand_changed or is_cooldown_elapsed):
                publish_vision(detection_dicts)
                last_publish_time = now
                last_published_brand = top_brand

            last_seen_any_time = now
        else:
            candidate_brand = None
            candidate_streak = 0
            if (now - last_seen_any_time) >= ABSENCE_RESET_SEC:
                last_published_brand = None

        cv2.putText(
            annotated,
            f"SORTIX AI | FPS: {current_fps:.1f} | MQTT: {'OK' if mqtt_connected else 'OFF'}",
            (15, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (0, 255, 180) if mqtt_connected else (0, 180, 255),
            2,
        )

        ok, buffer = cv2.imencode(".jpg", annotated, [int(cv2.IMWRITE_JPEG_QUALITY), JPEG_QUALITY])
        if ok:
            with frame_cond:
                latest_jpeg = buffer.tobytes()
                latest_jpeg_seq += 1
                frame_cond.notify_all()

    mqtt_cli.loop_stop()
    mqtt_cli.disconnect()


# ==========================================
# 5. FASTAPI MJPEG SERVER
# ==========================================

app = FastAPI(title="SortiX AI Camera Stream")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


def mjpeg_generator():
    last_sent_seq = -1
    while not stop_event.is_set():
        with frame_cond:
            if latest_jpeg is None or latest_jpeg_seq == last_sent_seq:
                frame_cond.wait(timeout=0.5)
            if latest_jpeg is None or latest_jpeg_seq == last_sent_seq:
                continue
            frame_bytes = latest_jpeg
            last_sent_seq = latest_jpeg_seq
        yield (
            b"--frame\r\n"
            b"Content-Type: image/jpeg\r\n\r\n" + frame_bytes + b"\r\n"
        )


@app.get("/video_feed")
def video_feed():
    return StreamingResponse(
        mjpeg_generator(),
        media_type="multipart/x-mixed-replace; boundary=frame",
    )


@app.get("/health")
def health():
    return JSONResponse({
        "status": "ok",
        "camera_source": CAMERA_SOURCE,
        "mqtt_connected": mqtt_connected,
        "fps": round(current_fps, 1),
        "last_detected": last_detected,
    })


if __name__ == "__main__":
    worker = threading.Thread(target=camera_worker, daemon=True)
    worker.start()
    try:
        uvicorn.run(app, host="0.0.0.0", port=STREAM_PORT, log_level="warning")
    finally:
        stop_event.set()
