"""
SortiX-Med - Cong cu danh nhan tu dong (Auto-Label) chuan YOLO cho anh chup tren bang tai xanh.

Quy trinh xu ly cho tung anh:
  1. Xac dinh dai bang tai xanh [belt_top, belt_bot] theo mau HSV, loai bo hoan toan
     vung cam bien vat can & bo mach dien o phan duoi khung hinh.
  2. Voi thu muc don lop (cotton, scissor, syringe): uu tien tach khoi vat the
     tuong phan tren nen bang tai xanh (giup om tron ca mui keo va dau kim tiem),
     neu khong du ro se fallback sang model YOLO best.pt chay tren anh da che bo mach.
  3. Voi thu muc 'mixed': chay YOLO best.pt tren anh da che bo mach.

Ket qua xuat ra `auto_labeled_bo_sung_for_review/`:
  - `<class>/`: cap anh `.jpg` + file nhan YOLO `.txt` (0=cotton, 1=scissor, 2=syringe)
  - `_preview/<class>/`: anh da ve khung + duong bien bang tai de kiem tra nhanh
  - `need_manual_label/<class>/`: anh chua bat duoc (can gan tay)
"""

import argparse
import shutil
from pathlib import Path

import cv2
import numpy as np
from ultralytics import YOLO

BASE_DIR = Path(__file__).resolve().parent
DEFAULT_SOURCE = BASE_DIR / "raw_captures_bo_sung"
DEFAULT_OUTPUT = BASE_DIR / "auto_labeled_bo_sung_for_review"
DEFAULT_WEIGHTS = BASE_DIR / "weights" / "best.pt"
IMAGE_EXTS = {".jpg", ".jpeg", ".png"}


def clamp01(val: float) -> float:
    """Gioi han gia tri toa do chuan hoa trong doan [0.0, 1.0]."""
    return max(0.0, min(1.0, val))


def xyxy_to_yolo(x1: float, y1: float, x2: float, y2: float, w: int, h: int) -> tuple[float, float, float, float]:
    """Chuyen bounding box pixel (x1, y1, x2, y2) sang chuan YOLO (cx, cy, bw, bh)."""
    nx1, ny1 = clamp01(x1 / w), clamp01(y1 / h)
    nx2, ny2 = clamp01(x2 / w), clamp01(y2 / h)
    bw = max(0.0, nx2 - nx1)
    bh = max(0.0, ny2 - ny1)
    return nx1 + bw / 2.0, ny1 + bh / 2.0, bw, bh


def detect_green_belt_bounds(im: np.ndarray) -> tuple[int, int]:
    """Tim ranh gioi tren/duoi cua dai bang tai xanh, dung ngay phia tren thanh ray trang."""
    h, _ = im.shape[:2]
    hsv = cv2.cvtColor(im, cv2.COLOR_BGR2HSV)
    # Mau xanh la dam cua mat bang tai (loai den LED xanh choi va mach dien)
    green = cv2.inRange(hsv, (40, 45, 25), (88, 225, 195))
    row_ratio = green[: int(h * 0.72), :].mean(axis=1) / 255.0
    tops = np.where(row_ratio > 0.35)[0]
    bots = np.where(row_ratio > 0.45)[0]
    bt = int(tops[0]) if (len(tops) and tops[0] < int(h * 0.15)) else 0
    bb = int(bots[-1]) if len(bots) else int(h * 0.56)
    if bb <= bt + 40:
        bb = int(h * 0.58)
    return bt, bb


def is_bottom_sensor_artifact(x1: float, y1: float, x2: float, y2: float, belt_bot: int, img_w: int, img_h: int) -> bool:
    """Loai bo box nho nam sat mep duoi bang tai (hai den LED cam bien vat can nho len)."""
    area_ratio = ((x2 - x1) * (y2 - y1)) / max(1.0, float(img_w * img_h))
    cy = (y1 + y2) / 2.0
    return y2 >= belt_bot - 12 and cy > belt_bot - 55 and area_ratio < 0.04


def extract_belt_foreground_box(im: np.ndarray, bt: int, bb: int) -> tuple[int, int, int, int] | None:
    """Tach vat the chinh nam tren mat bang tai xanh bang mat na khong-xanh trong HSV."""
    h, w = im.shape[:2]
    y1_roi = max(0, bt)
    y2_roi = max(y1_roi + 20, bb - 8)
    roi = im[y1_roi:y2_roi, :]
    hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
    green = cv2.inRange(hsv, (35, 32, 18), (95, 248, 220))
    fg = cv2.bitwise_not(green)

    # Xoa chu watermark manh 'using droidcam.app' nhung giu lai mui keo/dau kim tiem
    fg = cv2.morphologyEx(fg, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (4, 4)))
    fg = cv2.morphologyEx(fg, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (15, 15)))

    cnts, _ = cv2.findContours(fg, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    valid: list[tuple[float, int, int, int, int]] = []
    for c in cnts:
        area = cv2.contourArea(c)
        if area < h * w * 0.008:
            continue
        x, y, bw, bh = cv2.boundingRect(c)
        if bw < 22 or bh < 22:
            continue
        gx1, gy1, gx2, gy2 = x, y + y1_roi, x + bw, y + bh + y1_roi
        if is_bottom_sensor_artifact(gx1, gy1, gx2, gy2, bb, w, h):
            continue
        valid.append((area, gx1, gy1, gx2, gy2))

    if not valid:
        return None

    valid.sort(key=lambda t: t[0], reverse=True)
    _, x1, y1, x2, y2 = valid[0]
    pad = 8
    return (max(0, x1 - pad), max(bt, y1 - pad), min(w, x2 + pad), min(bb, y2 + pad))


def predict_yolo_on_belt(model: YOLO, im: np.ndarray, bt: int, bb: int, target_cls_id: int | None, conf: float) -> list[tuple[int, float, float, float, float, float, str]]:
    """Chay YOLO best.pt tren anh da che vung bo mach phia duoi va loc artifact cam bien."""
    h, w = im.shape[:2]
    masked = np.full_like(im, (114, 114, 114))
    masked[bt:bb, :] = im[bt:bb, :]

    def _run_and_filter(img_in: np.ndarray, pad_x: int = 0, pad_y: int = 0, tag: str = "yolo"):
        res = model.predict(img_in, conf=conf, augment=True, verbose=False)[0]
        if res.boxes is None or len(res.boxes) == 0:
            return []
        out = []
        for box in res.boxes:
            cls_id = int(box.cls[0])
            c_val = float(box.conf[0])
            if target_cls_id is not None and cls_id != target_cls_id:
                continue
            x1, y1, x2, y2 = (float(v) for v in box.xyxy[0])
            x1, x2 = max(0.0, x1 - pad_x), min(float(w), x2 - pad_x)
            y1, y2 = max(float(bt), y1 - pad_y), min(float(bb), y2 - pad_y)
            if x2 - x1 < 15 or y2 - y1 < 15:
                continue
            if is_bottom_sensor_artifact(x1, y1, x2, y2, bb, w, h):
                continue
            cx, cy, bw, bh = xyxy_to_yolo(x1, y1, x2, y2, w, h)
            out.append((cls_id, cx, cy, bw, bh, c_val, tag))
        return out

    dets = _run_and_filter(masked, tag="yolo")
    if dets:
        return dets

    py, px = int(h * 0.35), int(w * 0.35)
    padded = cv2.copyMakeBorder(masked, py, py, px, px, cv2.BORDER_CONSTANT, value=(114, 114, 114))
    return _run_and_filter(padded, pad_x=px, pad_y=py, tag="pad")


def save_preview_image(im: np.ndarray, preview_path: Path, bt: int, bb: int, detections: list[tuple[int, float, float, float, float, float, str]], names: dict[int, str]) -> None:
    """Ve bounding box, nhan va duong gioi han bang tai de nguoi dung review nhanh."""
    vis = im.copy()
    h, w = vis.shape[:2]
    cv2.line(vis, (0, bt), (w, bt), (255, 255, 0), 1)
    cv2.line(vis, (0, bb), (w, bb), (255, 255, 0), 1)

    for cls_id, cx, cy, bw, bh, conf, tag in detections:
        x1 = int((cx - bw / 2.0) * w)
        y1 = int((cy - bh / 2.0) * h)
        x2 = int((cx + bw / 2.0) * w)
        y2 = int((cy + bh / 2.0) * h)
        cv2.rectangle(vis, (x1, y1), (x2, y2), (0, 230, 80), 2)
        label = f"{names.get(cls_id, cls_id)} {conf:.2f} [{tag}]"
        cv2.putText(vis, label, (max(4, x1), max(22, y1 - 8)), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 230, 80), 2)

    preview_path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(preview_path), vis, [int(cv2.IMWRITE_JPEG_QUALITY), 90])


def auto_label_directory(source_dir: Path, output_dir: Path, weights_path: Path, conf: float) -> None:
    """Chay danh nhan tu dong cho tat ca thu muc con trong source_dir."""
    if not source_dir.exists():
        raise FileNotFoundError(f"Khong tim thay thu muc nguon: {source_dir}")
    if not weights_path.exists():
        raise FileNotFoundError(f"Khong tim thay file weights: {weights_path}")

    model = YOLO(str(weights_path))
    names: dict[int, str] = {int(k): str(v) for k, v in model.names.items()}
    name_to_id = {v.lower(): k for k, v in names.items()}

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    ordered_names = [names[i] for i in sorted(names)]
    (output_dir / "classes.txt").write_text("\n".join(ordered_names) + "\n", encoding="utf-8")

    summary: dict[str, dict[str, int]] = {}

    for sub in sorted(source_dir.iterdir()):
        if not sub.is_dir():
            continue
        folder_name = sub.name.lower()
        if folder_name == "background":
            continue

        imgs = sorted(p for p in sub.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTS)
        if not imgs:
            continue

        target_cls_id = name_to_id.get(folder_name)
        if folder_name != "mixed" and target_cls_id is None:
            print(f"[!] Bo qua thu muc '{sub.name}' vi khong co trong model names {ordered_names}")
            continue

        stats = {"total": len(imgs), "fg": 0, "yolo": 0, "manual": 0}

        for img_path in imgs:
            im = cv2.imread(str(img_path))
            if im is None:
                stats["manual"] += 1
                continue
            h, w = im.shape[:2]
            bt, bb = detect_green_belt_bounds(im)

            dets: list[tuple[int, float, float, float, float, float, str]] = []
            if target_cls_id is not None:
                fg_box = extract_belt_foreground_box(im, bt, bb)
                if fg_box is not None:
                    cx, cy, bw, bh = xyxy_to_yolo(*fg_box, w, h)
                    dets = [(target_cls_id, cx, cy, bw, bh, 1.0, "fg")]
                    stats["fg"] += 1

            if not dets:
                dets = predict_yolo_on_belt(model, im, bt, bb, target_cls_id, conf=conf)
                if dets:
                    stats["yolo"] += 1

            if dets:
                cls_out_dir = output_dir / sub.name
                cls_out_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(img_path, cls_out_dir / img_path.name)

                label_lines = [f"{c} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}" for c, cx, cy, bw, bh, _, _ in dets]
                (cls_out_dir / f"{img_path.stem}.txt").write_text("\n".join(label_lines) + "\n", encoding="utf-8")

                save_preview_image(im, output_dir / "_preview" / sub.name / img_path.name, bt, bb, dets, names)
            else:
                stats["manual"] += 1
                manual_dir = output_dir / "need_manual_label" / sub.name
                manual_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(img_path, manual_dir / img_path.name)

        summary[sub.name] = stats

    print("\n=== KET QUA DANH NHAN TU DONG (DA LOC CAM BIEN & BO MACH) ===")
    print("Thu muc xuat:", output_dir)
    for cls_name, st in summary.items():
        auto_ok = st["fg"] + st["yolo"]
        print(f"  - {cls_name}: {auto_ok}/{st['total']} tu dong (BeltFG={st['fg']}, YOLO={st['yolo']}) | Can gan tay: {st['manual']}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SortiX-Med YOLO auto-labeling tool")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="Thu muc anh dau vao")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Thu muc xuat ket qua review")
    parser.add_argument("--weights", type=Path, default=DEFAULT_WEIGHTS, help="Duong dan file weights .pt")
    parser.add_argument("--conf", type=float, default=0.20, help="Nguong confidence YOLO fallback")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    auto_label_directory(args.source, args.output, args.weights, args.conf)
