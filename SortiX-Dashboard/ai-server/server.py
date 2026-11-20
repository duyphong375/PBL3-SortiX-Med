"""
SortiX-Med - AI Camera Server (YOLO11 + MQTT + FastAPI MJPEG Video Stream)

Chay dong thoi:
  1. Doc Camera iPhone DroidCam (URL Wi-Fi http://192.168.137.155:4747/video hoac USB index 1)
  2. Nhan dien YOLO11 va gui JSON len MQTT topic sorter/sorter_01/vision
  3. Phat luong MJPEG tai http://localhost:8000/video_feed de nhung len Web Dashboard
"""

import asyncio
from concurrent.futures import ThreadPoolExecutor
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time
import urllib.request
from pathlib import Path
from urllib.parse import urlparse, urlunparse

import cv2
import numpy as np
import paho.mqtt.client as mqtt_client
import torch
import uvicorn
from fastapi import FastAPI, Request
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
DEFAULT_IPHONE_URL = "http://192.168.137.155:4747/video"
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
JPEG_QUALITY = int(os.environ.get("AI_JPEG_QUALITY", "75"))
STREAM_MAX_WIDTH = int(os.environ.get("AI_STREAM_MAX_WIDTH", "480"))
ENABLE_ANTI_BACKLIGHT = os.environ.get("ANTI_BACKLIGHT", "0").strip() == "1"
ENABLE_ROI_LOCK = os.environ.get("AI_ROI_ENABLED", "1").strip() != "0"
ENABLE_ROI_CROP = os.environ.get("AI_ROI_CROP", "1").strip() != "0"
DEFAULT_ROI = "auto"
ROI_RAW = os.environ.get("AI_ROI", DEFAULT_ROI).strip()


def _parse_roi(raw: str) -> tuple[str, tuple[float, float, float, float] | None]:
    """Doc cau hinh ROI: 'auto', 'x1,y1,x2,y2' (0..1), hoac 'off'."""
    low = raw.lower()
    if not low or low in ("0", "off", "none", "false"):
        return "off", None
    if low == "auto":
        return "auto", None
    parts = [p.strip() for p in raw.split(",")]
    if len(parts) != 4:
        return "off", None
    try:
        x1, y1, x2, y2 = (float(v) for v in parts)
    except ValueError:
        return "off", None
    if 0.0 <= x1 < x2 <= 1.0 and 0.0 <= y1 < y2 <= 1.0:
        return "fixed", (x1, y1, x2, y2)
    return "off", None


ROI_MODE, FIXED_ROI_BOUNDS = _parse_roi(ROI_RAW) if ENABLE_ROI_LOCK else ("off", None)
active_roi_bounds: tuple[float, float, float, float] | None = FIXED_ROI_BOUNDS

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
    "cotton": "med_cotton",
    "med_cotton": "med_cotton",
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
raw_cond = threading.Condition(raw_lock)
latest_raw_frame = None
latest_raw_seq = 0
stop_event = threading.Event()
mqtt_connected = False
camera_connected = False
active_camera_source = str(CAMERA_SOURCE)
current_fps = 0.0
last_detected: list[str] = []


def _make_status_jpeg(title: str, subtitle: str) -> bytes:
    """Tao khung hinh trang thai JPEG de giu luong MJPEG luon song khi cho/ket noi lai camera."""
    canvas = np.zeros((480, 360, 3), dtype=np.uint8)
    canvas[:] = (22, 26, 36)
    cv2.putText(canvas, "SORTIX AI CAMERA", (45, 195),
                cv2.FONT_HERSHEY_SIMPLEX, 0.72, (0, 220, 255), 2)
    cv2.putText(canvas, title[:34], (30, 240),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (80, 220, 255), 2)
    cv2.putText(canvas, subtitle[:44], (20, 275),
                cv2.FONT_HERSHEY_SIMPLEX, 0.42, (180, 190, 210), 1)
    ok, buf = cv2.imencode(".jpg", canvas, [int(cv2.IMWRITE_JPEG_QUALITY), 75])
    return buf.tobytes() if ok else b""


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

CAMERA_RECONNECT_DELAY = 1.0
CAMERA_MAX_READ_FAILS = 10
CAMERA_TIMEOUT_MS = 3000
HTTP_STREAM_TIMEOUT_SEC = 5.0


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


def _push_raw_frame(frame) -> None:
    global latest_raw_frame, latest_raw_seq, camera_connected
    camera_connected = True
    with raw_cond:
        latest_raw_frame = frame
        latest_raw_seq += 1
        raw_cond.notify_all()


def _probe_tcp_port(ip: str, port: int, timeout: float = 0.35) -> bool:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        return sock.connect_ex((ip, port)) == 0
    except OSError:
        return False
    finally:
        sock.close()


def _discover_droidcam_http_url(base_url: str) -> str | None:
    """Tu dong do lai IP iPhone DroidCam qua iPhone.mshome.net va bang ARP hotspot."""
    parsed = urlparse(base_url)
    port = parsed.port or 4747
    candidates: list[str] = []

    if parsed.hostname:
        candidates.append(parsed.hostname)
    try:
        candidates.append(socket.gethostbyname("iPhone.mshome.net"))
    except OSError:
        pass

    try:
        arp_out = subprocess.check_output(["arp", "-a"], text=True, timeout=2.0, errors="ignore")
        for ip in re.findall(r"\b(\d{1,3}(?:\.\d{1,3}){3})\b", arp_out):
            if ip.endswith(".1") or ip.endswith(".255") or ip.startswith(("224.", "239.", "255.")):
                continue
            candidates.append(ip)
    except Exception:
        pass

    unique_ips = list(dict.fromkeys(candidates))
    if not unique_ips:
        return None

    with ThreadPoolExecutor(max_workers=min(16, len(unique_ips))) as pool:
        results = list(pool.map(lambda h: _probe_tcp_port(h, port), unique_ips))

    for host, ok in zip(unique_ips, results):
        if ok:
            netloc = f"{host}:{port}"
            return urlunparse(parsed._replace(netloc=netloc))
    return None


def _extract_boundary(content_type: str) -> bytes | None:
    for part in content_type.split(";"):
        part = part.strip()
        if part.lower().startswith("boundary="):
            raw = part.split("=", 1)[1].strip().strip('"').lstrip("-")
            if raw:
                return f"--{raw}".encode("ascii", errors="ignore")
    return None


def _extract_latest_jpeg(buf: bytearray, boundary: bytes | None) -> bytes | None:
    """Lay frame JPEG hoan chinh moi nhat trong buffer (uu tien tach theo multipart boundary)."""
    if boundary is not None:
        # Tim 2 boundary lien tiep cuoi cung de lay tron ven part moi nhat (tranh cat nham EXIF)
        last_b = buf.rfind(boundary)
        if last_b != -1:
            prev_b = buf.rfind(boundary, 0, last_b)
            if prev_b != -1:
                part = bytes(buf[prev_b + len(boundary) : last_b])
                del buf[:last_b]
                hdr_end = part.find(b"\r\n\r\n")
                payload = part[hdr_end + 4 :] if hdr_end != -1 else part
                soi = payload.find(b"\xff\xd8")
                eoi = payload.rfind(b"\xff\xd9")
                if soi != -1 and eoi > soi:
                    return payload[soi : eoi + 2]
        return None

    # Fallback cho stream khong co boundary: tim SOI dau tien va EOI cuoi cung
    end_idx = buf.rfind(b"\xff\xd9")
    if end_idx != -1:
        start_idx = buf.find(b"\xff\xd8", 0, end_idx)
        if start_idx != -1:
            jpg = bytes(buf[start_idx : end_idx + 2])
            del buf[: end_idx + 2]
            return jpg
        del buf[: end_idx + 2]
    return None


def _read_http_mjpeg_once(url: str) -> tuple[bool, bool]:
    """Doc luong HTTP MJPEG (DroidCam) truc tiep, bo buffer an cua FFmpeg va timeout nhanh."""
    global camera_connected
    got_frame = False
    req = urllib.request.Request(url, headers={"User-Agent": "SortiX-AI/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=HTTP_STREAM_TIMEOUT_SEC) as resp:
            ctype = (resp.headers.get("Content-Type") or "").lower()
            if "multipart" not in ctype and "image" not in ctype:
                return False, False

            # Tat Nagle (TCP_NODELAY) de giam do tre goi tin JPEG nho qua Wi-Fi
            try:
                raw_sock = getattr(getattr(resp, "fp", None), "raw", None)
                sock = getattr(raw_sock, "_sock", None)
                if sock is not None:
                    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            except Exception:
                pass

            boundary = _extract_boundary(resp.headers.get("Content-Type") or "")
            print(f"[CAM] Da ket noi HTTP MJPEG: {url}")
            buf = bytearray()
            last_frame_at = time.time()

            while not stop_event.is_set():
                # Dung read1() de nhan byte ngay khi TCP co du lieu, khong cho day buffer
                chunk = resp.read1(32768) if hasattr(resp, "read1") else resp.read(4096)
                if not chunk:
                    break
                buf.extend(chunk)

                jpg_bytes = _extract_latest_jpeg(buf, boundary)
                if jpg_bytes:
                    frame = cv2.imdecode(
                        np.frombuffer(jpg_bytes, dtype=np.uint8),
                        cv2.IMREAD_COLOR,
                    )
                    if frame is not None:
                        _push_raw_frame(frame)
                        got_frame = True
                        last_frame_at = time.time()

                if len(buf) > 2 * 1024 * 1024:
                    buf.clear()
                if time.time() - last_frame_at > HTTP_STREAM_TIMEOUT_SEC:
                    break
            return True, got_frame
    except Exception as exc:
        print(f"[CAM] Loi luong HTTP MJPEG ({url}): {exc}")
        return True, got_frame
    finally:
        camera_connected = False


def camera_reader_worker() -> None:
    """Doc lien tuc de xa buffer va tu dong ket noi lai nhanh khi mat luong camera."""
    global camera_connected, active_camera_source
    src = str(CAMERA_SOURCE).strip()
    active_camera_source = src
    is_http = src.lower().startswith(("http://", "https://"))

    if is_http:
        consecutive_fails = 0
        fallback_to_opencv = False
        while not stop_event.is_set():
            handled_as_mjpeg, got_frame = _read_http_mjpeg_once(active_camera_source)
            if not handled_as_mjpeg:
                # Neu URL khong phai MJPEG thi roi xuong OpenCV fallback ben duoi
                fallback_to_opencv = True
                break

            if got_frame:
                consecutive_fails = 0
            else:
                consecutive_fails += 1
                if consecutive_fails >= 2:
                    discovered = _discover_droidcam_http_url(src)
                    if discovered and discovered != active_camera_source:
                        print(f"[CAM] Tu dong chuyen sang IP DroidCam moi: {discovered}")
                        active_camera_source = discovered
                        consecutive_fails = 0
                        continue

            if not stop_event.is_set():
                stop_event.wait(CAMERA_RECONNECT_DELAY)
        if not fallback_to_opencv:
            return

    cap: cv2.VideoCapture | None = None
    fail_count = 0

    try:
        while not stop_event.is_set():
            if cap is None:
                camera_connected = False
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
                    camera_connected = False
                    cap.release()
                    cap = None
                    stop_event.wait(CAMERA_RECONNECT_DELAY)
                else:
                    time.sleep(0.02)
                continue

            fail_count = 0
            _push_raw_frame(frame)
    finally:
        camera_connected = False
        if cap is not None:
            cap.release()


def _publish_jpeg(jpeg_bytes: bytes) -> None:
    global latest_jpeg, latest_jpeg_seq
    if not jpeg_bytes:
        return
    with frame_cond:
        latest_jpeg = jpeg_bytes
        latest_jpeg_seq += 1
        frame_cond.notify_all()


def _detect_green_belt_roi(frame: np.ndarray) -> tuple[float, float, float, float] | None:
    """Tu dong tim dai bang tai xanh theo HSV (hoat dong o ca 2 chieu dat camera)."""
    small = frame[::2, ::2]
    h = small.shape[0]
    if h < 40:
        return None
    hsv = cv2.cvtColor(small, cv2.COLOR_BGR2HSV)
    green = cv2.inRange(hsv, (35, 35, 18), (95, 245, 215))
    row_cov = (green > 0).mean(axis=1)
    k = max(3, int(h * 0.02) | 1)
    smooth = np.convolve(row_cov, np.ones(k) / k, mode="same")
    mask = smooth >= 0.36

    best_s, best_e = 0, -1
    cur_s = None
    for y, ok in enumerate(mask):
        if ok and cur_s is None:
            cur_s = y
        elif not ok and cur_s is not None:
            if (y - 1 - cur_s) > (best_e - best_s):
                best_s, best_e = cur_s, y - 1
            cur_s = None
    if cur_s is not None and (len(mask) - 1 - cur_s) > (best_e - best_s):
        best_s, best_e = cur_s, len(mask) - 1

    if best_e - best_s < h * 0.22:
        return None
    y1 = max(0.0, round(best_s / h - 0.005, 3))
    y2 = min(1.0, round((best_e + 1) / h + 0.005, 3))
    return (0.0, y1, 1.0, y2)


def _apply_roi_mask(
    frame: np.ndarray, roi: tuple[float, float, float, float] | None
) -> tuple[np.ndarray, tuple[int, int, int, int] | None]:
    """Che vung ngoai ROI bang xam (114,114,114) de giu nguyen ty le train ma loai bo hoan toan bo mach/LED."""
    if roi is None:
        return frame, None
    h, w = frame.shape[:2]
    rx1 = max(0, min(w - 1, int(round(roi[0] * w))))
    ry1 = max(0, min(h - 1, int(round(roi[1] * h))))
    rx2 = max(rx1 + 1, min(w, int(round(roi[2] * w))))
    ry2 = max(ry1 + 1, min(h, int(round(roi[3] * h))))
    masked = np.full_like(frame, (114, 114, 114))
    masked[ry1:ry2, rx1:rx2] = frame[ry1:ry2, rx1:rx2]
    return masked, (rx1, ry1, rx2, ry2)


def _render_roi_overlay(
    frame: np.ndarray,
    roi_px: tuple[int, int, int, int],
    detections: list[dict],
) -> np.ndarray:
    """Hien thi luong video theo ROI: mac dinh cat gon chi lay dai bang tai (AI_ROI_CROP=1)."""
    h, w = frame.shape[:2]
    rx1, ry1, rx2, ry2 = roi_px
    vis = frame.copy()

    for det in detections:
        bbox = det.get("bbox") or []
        if len(bbox) != 4:
            continue
        bx1, by1, bx2, by2 = (int(round(v)) for v in bbox)
        cv2.rectangle(vis, (bx1, by1), (bx2, by2), (0, 235, 90), 2)
        label = f"{det['class_name']} {det['confidence']:.2f}"
        label_y = max(ry1 + 22, by1 - 8) if ENABLE_ROI_CROP else max(22, by1 - 8)
        cv2.putText(
            vis,
            label,
            (max(rx1 + 6, bx1), label_y),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.65,
            (0, 235, 90),
            2,
        )

    if ENABLE_ROI_CROP:
        return vis[ry1:ry2, rx1:rx2]

    if ry1 > 0:
        vis[:ry1, :] = cv2.convertScaleAbs(vis[:ry1, :], alpha=0.35, beta=0)
    if ry2 < h:
        vis[ry2:, :] = cv2.convertScaleAbs(vis[ry2:, :], alpha=0.35, beta=0)
    if rx1 > 0:
        vis[ry1:ry2, :rx1] = cv2.convertScaleAbs(vis[ry1:ry2, :rx1], alpha=0.35, beta=0)
    if rx2 < w:
        vis[ry1:ry2, rx2:] = cv2.convertScaleAbs(vis[ry1:ry2, rx2:], alpha=0.35, beta=0)

    cv2.rectangle(vis, (rx1, ry1), (rx2 - 1, ry2 - 1), (255, 220, 0), 2)
    cv2.putText(
        vis,
        "ROI AIM",
        (max(8, rx2 - 92), max(22, ry2 - 10)),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.55,
        (255, 220, 0),
        2,
    )
    return vis


def camera_worker() -> None:
    global current_fps, last_detected, active_roi_bounds

    _publish_jpeg(_make_status_jpeg("DANG KHOI DONG AI...", "Loading YOLO11 model"))

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
    print(
        f"[ROI] Che do: {ROI_MODE}"
        + (f" ({FIXED_ROI_BOUNDS})" if FIXED_ROI_BOUNDS is not None else "")
    )
    reader = threading.Thread(target=camera_reader_worker, daemon=True)
    reader.start()

    print(f"[STREAM] San sang phat tai: http://localhost:{STREAM_PORT}/video_feed")
    prev_time = time.time()
    last_publish_time = 0.0
    last_published_brand: str | None = None
    last_seen_any_time = 0.0
    last_processed_seq = 0
    last_status_frame_at = 0.0
    last_roi_scan_at = 0.0
    candidate_brand: str | None = None
    candidate_streak = 0

    while not stop_event.is_set():
        with raw_cond:
            if latest_raw_frame is None or latest_raw_seq == last_processed_seq:
                raw_cond.wait(timeout=0.5)
            if latest_raw_frame is None or latest_raw_seq == last_processed_seq:
                frame = None
            else:
                frame = latest_raw_frame.copy()
                last_processed_seq = latest_raw_seq

        if frame is None:
            now_idle = time.time()
            if not camera_connected and (now_idle - last_status_frame_at >= 1.0):
                current_fps = 0.0
                _publish_jpeg(_make_status_jpeg("DANG KET NOI CAMERA...", active_camera_source))
                last_status_frame_at = now_idle
            continue

        frame = _rotate_frame(frame)
        ai_frame = enhance_for_backlight(frame)

        now_roi = time.time()
        if ROI_MODE == "auto" and (active_roi_bounds is None or now_roi - last_roi_scan_at >= 0.35):
            detected_roi = _detect_green_belt_roi(frame)
            last_roi_scan_at = now_roi
            if detected_roi is not None:
                if active_roi_bounds is None or (
                    abs(detected_roi[1] - active_roi_bounds[1]) > 0.12
                    or abs(detected_roi[3] - active_roi_bounds[3]) > 0.12
                ):
                    active_roi_bounds = detected_roi
                else:
                    active_roi_bounds = tuple(
                        round(old * 0.85 + new * 0.15, 3)
                        for old, new in zip(active_roi_bounds, detected_roi)
                    )

        ai_input, roi_px = _apply_roi_mask(ai_frame, active_roi_bounds)

        results = model.predict(
            source=ai_input,
            imgsz=IMAGE_SIZE,
            conf=CONFIDENCE,
            device=device,
            verbose=False,
        )

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
            if roi_px is not None and len(bbox) == 4:
                cx = (bbox[0] + bbox[2]) * 0.5
                cy = (bbox[1] + bbox[3]) * 0.5
                if not (roi_px[0] <= cx <= roi_px[2] and roi_px[1] <= cy <= roi_px[3]):
                    continue
            detected_labels.append(f"{class_name}: {confidence:.2f}")
            detection_dicts.append({
                "class_id": class_id,
                "class_name": class_name,
                "confidence": round(confidence, 4),
                "bbox": bbox,
            })

        annotated = (
            _render_roi_overlay(ai_frame, roi_px, detection_dicts)
            if roi_px is not None
            else results[0].plot()
        )

        now = time.time()
        inst_fps = 1.0 / max(now - prev_time, 0.0001)
        prev_time = now
        current_fps = inst_fps if current_fps <= 0 else (current_fps * 0.8 + inst_fps * 0.2)
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

        if STREAM_MAX_WIDTH > 0 and annotated.shape[1] > STREAM_MAX_WIDTH:
            new_h = max(1, int(annotated.shape[0] * (STREAM_MAX_WIDTH / annotated.shape[1])))
            annotated = cv2.resize(annotated, (STREAM_MAX_WIDTH, new_h), interpolation=cv2.INTER_AREA)

        cv2.putText(
            annotated,
            f"SORTIX AI | FPS: {current_fps:.1f} | MQTT: {'OK' if mqtt_connected else 'OFF'}",
            (12, 26),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.55,
            (0, 255, 180) if mqtt_connected else (0, 180, 255),
            2,
        )

        ok, buffer = cv2.imencode(".jpg", annotated, [int(cv2.IMWRITE_JPEG_QUALITY), JPEG_QUALITY])
        if ok:
            _publish_jpeg(buffer.tobytes())
            last_status_frame_at = now

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


def _wait_for_frame(last_sent_seq: int, timeout: float = 0.4) -> tuple[bytes | None, int]:
    with frame_cond:
        if latest_jpeg is None or latest_jpeg_seq == last_sent_seq:
            frame_cond.wait(timeout=timeout)
        return latest_jpeg, latest_jpeg_seq


async def mjpeg_generator(request: Request):
    last_sent_seq = -1
    last_yield_at = 0.0
    try:
        while not stop_event.is_set():
            if await request.is_disconnected():
                break

            frame_bytes, current_seq = await asyncio.to_thread(_wait_for_frame, last_sent_seq, 0.4)
            now = time.time()
            # Gui keepalive neu qua 1.0s chua co frame moi de trinh duyet khong timeout/dong ket noi
            if frame_bytes is None or (current_seq == last_sent_seq and now - last_yield_at < 1.0):
                continue

            last_sent_seq = current_seq
            last_yield_at = now
            yield (
                b"--frame\r\n"
                b"Content-Type: image/jpeg\r\n"
                b"Content-Length: " + str(len(frame_bytes)).encode("ascii") + b"\r\n\r\n"
                + frame_bytes
                + b"\r\n"
            )
    except (asyncio.CancelledError, ConnectionResetError, BrokenPipeError):
        pass


@app.get("/video_feed")
async def video_feed(request: Request):
    return StreamingResponse(
        mjpeg_generator(request),
        media_type="multipart/x-mixed-replace; boundary=frame",
        headers={
            "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
            "Pragma": "no-cache",
            "Expires": "0",
            "X-Accel-Buffering": "no",
        },
    )


@app.get("/health")
def health():
    return JSONResponse({
        "status": "ok",
        "camera_source": active_camera_source,
        "camera_connected": camera_connected,
        "mqtt_connected": mqtt_connected,
        "roi_mode": ROI_MODE,
        "roi_enabled": active_roi_bounds is not None,
        "roi": list(active_roi_bounds) if active_roi_bounds is not None else None,
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
