"""Utility helpers for reading the ESP32 MJPEG stream during the MASS60 workshop."""
import contextlib
import time
from typing import Generator, Iterable, Optional

import cv2
import numpy as np
import requests

UserAgent = "MASS60-CV-Workshop/1.0"


class MJPEGStream:
    """Naive MJPEG iterator for ESP32-style multipart streams."""

    def __init__(self, url: str, timeout: float = 10.0, chunk_size: int = 1024) -> None:
        self.url = url
        self.timeout = timeout
        self.chunk_size = chunk_size
        self._response: Optional[requests.Response] = None

    def __enter__(self) -> "MJPEGStream":
        self.open()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()

    def open(self) -> None:
        if self._response is not None:
            return
        headers = {"User-Agent": UserAgent, "Accept": "multipart/x-mixed-replace"}
        self._response = requests.get(
            self.url,
            stream=True,
            timeout=self.timeout,
            headers=headers,
        )
        self._response.raise_for_status()

    def close(self) -> None:
        if self._response is not None:
            self._response.close()
            self._response = None

    def frames(self) -> Generator[np.ndarray, None, None]:
        if self._response is None:
            self.open()
        assert self._response is not None

        buffer = bytearray()
        jpeg_start = b"\xff\xd8"
        jpeg_end = b"\xff\xd9"

        for chunk in self._response.iter_content(chunk_size=self.chunk_size):
            buffer.extend(chunk)
            start = buffer.find(jpeg_start)
            end = buffer.find(jpeg_end)
            if start != -1 and end != -1 and end > start:
                jpg = buffer[start : end + 2]
                del buffer[: end + 2]
                frame = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
                if frame is None:
                    continue
                yield frame


class FrameRateTracker:
    def __init__(self, averaging: int = 30) -> None:
        self.timestamps: list[float] = []
        self.averaging = averaging

    def update(self) -> float:
        now = time.perf_counter()
        self.timestamps.append(now)
        if len(self.timestamps) > self.averaging:
            self.timestamps.pop(0)
        if len(self.timestamps) < 2:
            return 0.0
        elapsed = self.timestamps[-1] - self.timestamps[0]
        if elapsed <= 0:
            return 0.0
        fps = (len(self.timestamps) - 1) / elapsed
        return fps


def with_mjpeg(url: str) -> contextlib.AbstractContextManager["MJPEGStream"]:
    stream = MJPEGStream(url)
    return contextlib.closing(stream)


def clamp(value: float, min_value: float, max_value: float) -> float:
    return max(min_value, min(value, max_value))


def hex_to_bgr(color: str) -> tuple[int, int, int]:
    color = color.lstrip("#")
    return tuple(int(color[i : i + 2], 16) for i in (4, 2, 0))
