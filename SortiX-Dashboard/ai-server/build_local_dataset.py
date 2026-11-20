"""
SortiX-Med - Xay dung lai dataset YOLO11 day du tu auto_labeled_for_review/ va raw_captures/background/.

Chuan lop khop voi data.yaml & auto_labeled_for_review/classes.txt:
  0: cotton
  1: scissor
  2: syringe
"""

from __future__ import annotations

import random
import shutil
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
AUTO_LABELED_DIR = BASE_DIR / "auto_labeled_for_review"
BACKGROUND_DIR = BASE_DIR / "raw_captures" / "background"
TARGET_DATASET_DIR = BASE_DIR / "dataset"

SINGLE_CLASSES: dict[str, int] = {
    "cotton": 0,
    "scissor": 1,
    "syringe": 2,
}

SPLITS = ("train", "valid", "test")
TRAIN_RATIO = 0.80
VALID_RATIO = 0.12
SEED = 42


def _parse_box(line: str) -> tuple[int, float, float, float, float] | None:
    parts = line.strip().split()
    if len(parts) != 5:
        return None
    try:
        cid = int(parts[0])
        xc, yc, w, h = (float(v) for v in parts[1:])
    except ValueError:
        return None
    if not (0.0 <= xc <= 1.0 and 0.0 <= yc <= 1.0 and 0.0 < w <= 1.0 and 0.0 < h <= 1.0):
        return None
    return cid, xc, yc, w, h


def _is_valid_box(box: tuple[int, float, float, float, float]) -> bool:
    """Loai bo false positive o mep ban/khung anh (vd: width=1.0 hoac goc tren ban)."""
    _, xc, yc, w, h = box
    if w >= 0.85 or h >= 0.85:
        return False
    if yc < 0.16 and (xc < 0.20 or xc > 0.80):
        return False
    return True


def _format_box(box: tuple[int, float, float, float, float]) -> str:
    cid, xc, yc, w, h = box
    return f"{cid} {xc:.6f} {yc:.6f} {w:.6f} {h:.6f}"


def _collect_samples() -> dict[str, list[tuple[Path, str]]]:
    """Thu thap va lam sach anh + nhan theo tung nhom nguon."""
    grouped: dict[str, list[tuple[Path, str]]] = {
        "cotton": [],
        "scissor": [],
        "syringe": [],
        "mixed": [],
        "background": [],
    }

    for folder, expected_cid in SINGLE_CLASSES.items():
        src_dir = AUTO_LABELED_DIR / folder
        if not src_dir.exists():
            continue
        for jpg_path in sorted(src_dir.glob("*.jpg")):
            txt_path = jpg_path.with_suffix(".txt")
            if not txt_path.exists():
                continue
            valid_boxes = []
            for raw_line in txt_path.read_text(encoding="utf-8").splitlines():
                parsed = _parse_box(raw_line)
                if parsed and parsed[0] == expected_cid and _is_valid_box(parsed):
                    valid_boxes.append(parsed)
            if not valid_boxes:
                continue
            # Moi anh don lop chi chua 1 vat chinh -> chon box co dien tich lon nhat de tranh trung box
            best_box = max(valid_boxes, key=lambda b: b[3] * b[4])
            grouped[folder].append((jpg_path, _format_box(best_box) + "\n"))

    mixed_dir = AUTO_LABELED_DIR / "mixed"
    if mixed_dir.exists():
        for jpg_path in sorted(mixed_dir.glob("*.jpg")):
            txt_path = jpg_path.with_suffix(".txt")
            if not txt_path.exists():
                continue
            valid_boxes = []
            for raw_line in txt_path.read_text(encoding="utf-8").splitlines():
                parsed = _parse_box(raw_line)
                if parsed and parsed[0] in (0, 1, 2) and _is_valid_box(parsed):
                    valid_boxes.append(parsed)
            cids = {b[0] for b in valid_boxes}
            # Chi giu anh mixed du ca 3 lop de tranh thieu nhan gay hoc sai nen
            if cids == {0, 1, 2}:
                label_text = "\n".join(_format_box(b) for b in valid_boxes) + "\n"
                grouped["mixed"].append((jpg_path, label_text))

    if BACKGROUND_DIR.exists():
        for jpg_path in sorted(BACKGROUND_DIR.glob("*.jpg")):
            grouped["background"].append((jpg_path, ""))

    return grouped


def _split_group(
    items: list[tuple[Path, str]], rng: random.Random
) -> dict[str, list[tuple[Path, str]]]:
    shuffled = list(items)
    rng.shuffle(shuffled)
    n = len(shuffled)
    n_train = max(1, int(round(n * TRAIN_RATIO))) if n >= 3 else n
    n_valid = max(1, int(round(n * VALID_RATIO))) if n - n_train >= 2 else max(0, n - n_train)
    return {
        "train": shuffled[:n_train],
        "valid": shuffled[n_train : n_train + n_valid],
        "test": shuffled[n_train + n_valid :],
    }


def build_dataset() -> None:
    grouped = _collect_samples()
    rng = random.Random(SEED)

    if TARGET_DATASET_DIR.exists():
        shutil.rmtree(TARGET_DATASET_DIR)

    for split in SPLITS:
        (TARGET_DATASET_DIR / split / "images").mkdir(parents=True, exist_ok=True)
        (TARGET_DATASET_DIR / split / "labels").mkdir(parents=True, exist_ok=True)

    split_totals = {split: 0 for split in SPLITS}
    print("[*] Tong hop du lieu sach tu raw_captures + auto_labeled_for_review:")

    for group_name, items in grouped.items():
        parts = _split_group(items, rng)
        print(
            f"  - {group_name:10s}: {len(items):3d} anh "
            f"(train={len(parts['train'])}, valid={len(parts['valid'])}, test={len(parts['test'])})"
        )
        for split in SPLITS:
            img_dst_dir = TARGET_DATASET_DIR / split / "images"
            lbl_dst_dir = TARGET_DATASET_DIR / split / "labels"
            for jpg_path, label_text in parts[split]:
                shutil.copy2(jpg_path, img_dst_dir / jpg_path.name)
                (lbl_dst_dir / f"{jpg_path.stem}.txt").write_text(label_text, encoding="utf-8")
                split_totals[split] += 1

    dataset_yaml = TARGET_DATASET_DIR / "data.yaml"
    dataset_yaml.write_text(
        "train: ../train/images\n"
        "val: ../valid/images\n"
        "test: ../test/images\n\n"
        "nc: 3\n"
        "names: ['cotton', 'scissor', 'syringe']\n",
        encoding="utf-8",
    )

    total_all = sum(split_totals.values())
    print(f"[OK] Da tao dataset tai {TARGET_DATASET_DIR}: tong {total_all} anh")
    print(
        f"     train={split_totals['train']} | valid={split_totals['valid']} | test={split_totals['test']}"
    )


if __name__ == "__main__":
    build_dataset()
