"""Drawing helpers that keep HUD overlays consistent across MASS60 CV demos."""
from typing import Iterable, Tuple

import cv2
import numpy as np

DEFAULT_FONT = cv2.FONT_HERSHEY_SIMPLEX


def draw_hud(
    frame: np.ndarray,
    lines: Iterable[str],
    origin: Tuple[int, int] = (16, 32),
    color: Tuple[int, int, int] = (255, 255, 255),
    scale: float = 0.6,
    thickness: int = 1,
    line_height: int = 22,
) -> None:
    x, y = origin
    for line in lines:
        cv2.putText(frame, line, (x, y), DEFAULT_FONT, scale, color, thickness, cv2.LINE_AA)
        y += line_height


def draw_center_caption(
    frame: np.ndarray,
    text: str,
    y_offset: int = 40,
    color: Tuple[int, int, int] = (20, 220, 255),
    scale: float = 0.8,
    thickness: int = 2,
) -> None:
    h, w = frame.shape[:2]
    text_size = cv2.getTextSize(text, DEFAULT_FONT, scale, thickness)[0]
    x = (w - text_size[0]) // 2
    y = y_offset + text_size[1]
    cv2.putText(frame, text, (x, y), DEFAULT_FONT, scale, color, thickness, cv2.LINE_AA)


def draw_bbox(
    frame: np.ndarray,
    bbox: Tuple[int, int, int, int],
    label: str,
    color: Tuple[int, int, int] = (0, 255, 0),
    thickness: int = 2,
) -> None:
    x1, y1, x2, y2 = bbox
    cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)
    label_size, _ = cv2.getTextSize(label, DEFAULT_FONT, 0.5, 1)
    label_x = max(x1, 0)
    label_y = max(y1 - 8, label_size[1] + 4)
    cv2.rectangle(
        frame,
        (label_x, label_y - label_size[1] - 6),
        (label_x + label_size[0] + 4, label_y),
        color,
        -1,
    )
    cv2.putText(
        frame,
        label,
        (label_x + 2, label_y - 2),
        DEFAULT_FONT,
        0.5,
        (0, 0, 0),
        1,
        cv2.LINE_AA,
    )
