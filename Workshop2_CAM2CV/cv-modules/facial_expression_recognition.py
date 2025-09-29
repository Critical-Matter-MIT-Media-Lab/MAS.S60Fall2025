"""Facial expression recognition using MediaPipe Face Mesh heuristics.

Lightweight demo that pulls frames from the ESP32 MJPEG stream, runs MediaPipe
face detection / face mesh, and infers coarse expressions (happy, surprised,
angry, sad, neutral) from landmark geometry. Completely CPU friendly.
"""

import argparse
import math
from typing import Iterable, Tuple

import cv2
import mediapipe as mp
import numpy as np
from mediapipe.framework.formats import landmark_pb2

from utils.overlays import draw_bbox, draw_hud
from utils.stream_client import FrameRateTracker, MJPEGStream
import asyncio
import json
import logging
import signal
import threading
import time
from typing import Dict, List, Optional

import websockets



FACE_DETECTION = mp.solutions.face_detection.FaceDetection(
    model_selection=0,
    min_detection_confidence=0.5,
)
FACE_MESH = mp.solutions.face_mesh.FaceMesh(
    static_image_mode=False,
    refine_landmarks=True,
    max_num_faces=1,
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5,
)

LEFT_MOUTH = 61
RIGHT_MOUTH = 291
UPPER_LIP = 13
LOWER_LIP = 14
LEFT_EYE_TOP = 159
LEFT_EYE_BOTTOM = 145
RIGHT_EYE_TOP = 386
RIGHT_EYE_BOTTOM = 374
LEFT_BROW = 70
RIGHT_BROW = 300
LEFT_EYE_CENTER = 468  # iris landmark
RIGHT_EYE_CENTER = 473
LEFT_CHEEK = 234
RIGHT_CHEEK = 454
LIP_TOP = 0
LIP_BOTTOM = 17


def _landmark_to_xy(landmark: landmark_pb2.NormalizedLandmark,
                    width: int, height: int) -> Tuple[float, float]:
    return landmark.x * width, landmark.y * height


def _distance(p1: Tuple[float, float], p2: Tuple[float, float]) -> float:
    return math.dist(p1, p2)


def _angle(p1: Tuple[float, float], p2: Tuple[float, float]) -> float:
    return math.atan2(p2[1] - p1[1], p2[0] - p1[0])


def classify_expression(landmarks: Iterable[landmark_pb2.NormalizedLandmark],
                        width: int, height: int) -> Tuple[str, float]:
    lm = list(landmarks)
    left_mouth = _landmark_to_xy(lm[LEFT_MOUTH], width, height)
    right_mouth = _landmark_to_xy(lm[RIGHT_MOUTH], width, height)
    upper_lip = _landmark_to_xy(lm[UPPER_LIP], width, height)
    lower_lip = _landmark_to_xy(lm[LOWER_LIP], width, height)
    left_eye_top = _landmark_to_xy(lm[LEFT_EYE_TOP], width, height)
    left_eye_bottom = _landmark_to_xy(lm[LEFT_EYE_BOTTOM], width, height)
    right_eye_top = _landmark_to_xy(lm[RIGHT_EYE_TOP], width, height)
    right_eye_bottom = _landmark_to_xy(lm[RIGHT_EYE_BOTTOM], width, height)
    left_brow = _landmark_to_xy(lm[LEFT_BROW], width, height)
    right_brow = _landmark_to_xy(lm[RIGHT_BROW], width, height)
    left_eye_center = _landmark_to_xy(lm[LEFT_EYE_CENTER], width, height)
    right_eye_center = _landmark_to_xy(lm[RIGHT_EYE_CENTER], width, height)
    lip_top = _landmark_to_xy(lm[LIP_TOP], width, height)
    lip_bottom = _landmark_to_xy(lm[LIP_BOTTOM], width, height)
    left_cheek = _landmark_to_xy(lm[LEFT_CHEEK], width, height)
    right_cheek = _landmark_to_xy(lm[RIGHT_CHEEK], width, height)

    face_width = _distance(left_cheek, right_cheek) + 1e-6
    mouth_width = _distance(left_mouth, right_mouth) / face_width
    mouth_height = _distance(upper_lip, lower_lip) / face_width
    lip_gap = _distance(lip_top, lip_bottom) / face_width

    left_eye_open = _distance(left_eye_top, left_eye_bottom) / face_width
    right_eye_open = _distance(right_eye_top, right_eye_bottom) / face_width
    eye_open = (left_eye_open + right_eye_open) / 2

    brow_left_gap = abs(left_brow[1] - left_eye_center[1]) / face_width
    brow_right_gap = abs(right_brow[1] - right_eye_center[1]) / face_width
    brow_gap = (brow_left_gap + brow_right_gap) / 2

    mouth_angle = _angle(left_mouth, right_mouth)

    scores = {
        "surprised": 0.0,
        "happy": 0.0,
        "angry": 0.0,
        "sad": 0.0,
        "neutral": 0.5,
    }

    if mouth_height > 0.22 and eye_open > 0.11:
        scores["surprised"] = min(1.0, (mouth_height - 0.2) * 8 + (eye_open - 0.1) * 8)

    if mouth_width > 0.42 and mouth_height > 0.10 and mouth_angle > -0.05:
        scores["happy"] = min(1.0,
                               (mouth_width - 0.4) * 4 +
                               max(0.0, mouth_angle) * 2 +
                               max(0.0, mouth_height - 0.1) * 4)

    if brow_gap < 0.08 and eye_open < 0.09:
        scores["angry"] = min(1.0,
                               (0.1 - brow_gap) * 10 +
                               (0.1 - eye_open) * 8)

    if mouth_angle < -0.08 and mouth_height < 0.12:
        scores["sad"] = min(1.0,
                             (-mouth_angle - 0.05) * 4 +
                             max(0.0, 0.12 - mouth_height) * 4)

    neutral_penalty = max(scores.values()) * 0.6
    scores["neutral"] = max(0.0, scores["neutral"] - neutral_penalty)

    label, confidence = max(scores.items(), key=lambda kv: kv[1])
    return label, float(min(1.0, confidence))


LOGGER = logging.getLogger("expression_bridge")


def clamp(value: float, mn: float, mx: float) -> float:
    return max(mn, min(value, mx))


class ExpressionMapper:
    def __init__(self) -> None:
        self.style_map = {
            "happy": "nebula",
            "surprised": "galaxy",
            "angry": "meteor",
            "sad": "stardust",
            "neutral": "embers",
            "unknown": "embers",
        }
        self.base_hue = {
            "happy": 50.0,
            "surprised": 200.0,
            "angry": 0.0,
            "sad": 220.0,
            "neutral": 180.0,
            "unknown": 180.0,
        }

    def map(self, label: str, center: Tuple[float, float], confidence: float) -> Tuple[float, float, float, float, str]:
        label = label if label in self.style_map else "neutral"
        hue = (self.base_hue[label] + center[0] * 120.0) % 360.0
        launch_power = clamp(1.0 - center[1], 0.15, 0.95)
        if label == "happy":
            density = 1.4
            twist = 0.15
        elif label == "surprised":
            density = 1.6
            twist = 0.25
        elif label == "angry":
            density = 1.2
            twist = -0.2
        elif label == "sad":
            density = 0.6
            twist = -0.1
        else:  # neutral
            density = 0.8
            twist = 0.0
        # Slightly modulate density by confidence
        density = clamp(density * (0.7 + 0.6 * confidence), 0.2, 1.9)
        style = self.style_map[label]
        return hue, launch_power, density, twist, style


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Facial expression bridge for the fireworks visual")
    parser.add_argument("--source", default="http://192.168.4.1/stream", help="MJPEG stream URL")
    parser.add_argument("--display", action="store_true", help="Show annotated video window")
    parser.add_argument("--save", type=str, help="Optional MP4 output path")
    parser.add_argument("--min-score", type=float, default=0.2, help="Minimum confidence to display label")
    parser.add_argument("--ws-host", default="0.0.0.0", help="WebSocket bind host")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket bind port")
    parser.add_argument("--ws-path", default="/fireworks", help="WebSocket path clients must use")
    parser.add_argument("--ws-rate", type=float, default=15.0, help="Max events per second to broadcast")
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"], help="Logger level")
    return parser


def enqueue_payload(loop: asyncio.AbstractEventLoop, queue: asyncio.Queue, payload: Dict[str, object]) -> None:
    async def _put() -> None:
        if queue.full():
            try:
                queue.get_nowait()
            except asyncio.QueueEmpty:
                pass
        await queue.put(payload)

    asyncio.run_coroutine_threadsafe(_put(), loop)


class WebSocketHub:
    def __init__(self) -> None:
        self._clients: List[websockets.WebSocketServerProtocol] = []
        self._lock = asyncio.Lock()

    async def register(self, ws: websockets.WebSocketServerProtocol) -> None:
        async with self._lock:
            self._clients.append(ws)
        LOGGER.info("Client connected (%d total)", len(self._clients))
        hello = json.dumps({"type": "hello", "message": "expression-bridge-ready"})
        try:
            await ws.send(hello)
            await ws.wait_closed()
        finally:
            async with self._lock:
                if ws in self._clients:
                    self._clients.remove(ws)
            LOGGER.info("Client disconnected (%d total)", len(self._clients))

    async def broadcast(self, message: str) -> None:
        if not self._clients:
            return
        async with self._lock:
            targets = [client for client in self._clients if not client.closed]
        if not targets:
            return
        await asyncio.gather(*[self._safe_send(c, message) for c in targets], return_exceptions=True)

    @staticmethod
    async def _safe_send(client: websockets.WebSocketServerProtocol, message: str) -> None:
        try:
            await client.send(message)
        except Exception:
            pass


async def websocket_main(args: argparse.Namespace, queue: asyncio.Queue, stop_event: threading.Event) -> None:
    hub = WebSocketHub()

    async def handler(ws: websockets.WebSocketServerProtocol, path: str) -> None:
        if args.ws_path and path != args.ws_path:
            await ws.close(code=1008, reason="Invalid path")
            return
        await hub.register(ws)

    server = await websockets.serve(
        handler,
        args.ws_host,
        args.ws_port,
        ping_interval=20,
        ping_timeout=20,
    )

    LOGGER.info("WebSocket server running on ws://%s:%d%s", args.ws_host, args.ws_port, args.ws_path)

    try:
        while not stop_event.is_set():
            payload = await queue.get()
            if payload.get("type") == "shutdown":
                break
            await hub.broadcast(json.dumps(payload))
    finally:
        server.close()
        await server.wait_closed()
        LOGGER.info("WebSocket server stopped")


def configure_logging(level: str) -> None:
    logging.basicConfig(level=getattr(logging, level.upper()), format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")


def run_detection(
    args: argparse.Namespace,
    loop: asyncio.AbstractEventLoop,
    queue: asyncio.Queue,
    stop_event: threading.Event,
) -> None:
    LOGGER.info("Starting MJPEG reader: %s", args.source)
    fps_tracker = FrameRateTracker()
    mapper = ExpressionMapper()
    writer = None
    window_name = "Expression Bridge"

    last_emit = 0.0

    try:
        with MJPEGStream(args.source) as stream:
            for frame in stream.frames():
                if stop_event.is_set():
                    break

                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                detection_result = FACE_DETECTION.process(rgb)
                detections = detection_result.detections or []

                center = (0.5, 0.6)
                if detections:
                    detection = detections[0]
                    location = detection.location_data.relative_bounding_box
                    h, w, _ = frame.shape
                    x1 = max(int(location.xmin * w), 0)
                    y1 = max(int(location.ymin * h), 0)
                    x2 = min(int((location.xmin + location.width) * w), w - 1)
                    y2 = min(int((location.ymin + location.height) * h), h - 1)
                    draw_bbox(frame, (x1, y1, x2, y2), "face", color=(80, 180, 255))
                    cx = (x1 + x2) / 2 / max(w, 1)
                    cy = (y1 + y2) / 2 / max(h, 1)
                    center = (clamp(cx, 0.0, 1.0), clamp(cy, 0.0, 1.0))

                mesh_result = FACE_MESH.process(rgb)
                label = "neutral"
                confidence = 0.0
                if mesh_result.multi_face_landmarks:
                    landmarks = mesh_result.multi_face_landmarks[0].landmark
                    label, confidence = classify_expression(landmarks, frame.shape[1], frame.shape[0])

                hue, launch_power, density, twist, style = mapper.map(label, center, confidence)

                fps = fps_tracker.update()
                draw_hud(
                    frame,
                    [
                        f"Expr: {label} ({confidence:.2f})  Hue: {hue:.0f}  Launch: {launch_power:.2f}",
                        f"Density: {density:.2f}  Twist: {twist:.2f}  Center: {center[0]:.2f},{center[1]:.2f}",
                        f"WS: ws://{args.ws_host}:{args.ws_port}{args.ws_path}",
                    ],
                    origin=(16, 120),
                    color=(120, 220, 255),
                    scale=0.7,
                )
                draw_hud(frame, [f"Source: {args.source}", f"FPS: {fps:.1f}", f"Faces: {1 if detections else 0}"])

                if args.save and writer is None:
                    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                    height, width = frame.shape[:2]
                    writer = cv2.VideoWriter(args.save, fourcc, max(fps, 15) or 15, (width, height))
                if writer is not None:
                    writer.write(frame)

                if args.display:
                    cv2.imshow(window_name, frame)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        stop_event.set()
                        break

                now = time.time()
                if now - last_emit >= 1.0 / max(args.ws_rate, 1):
                    payload = {
                        "type": "gesture",
                        "gesture": label,
                        "confidence": round(confidence, 3),
                        "hue": round(hue, 2),
                        "launch_power": round(launch_power, 3),
                        "spark_density": round(density, 3),
                        "twist": round(twist, 3),
                        "center": {"x": round(center[0], 3), "y": round(center[1], 3)},
                        "style": style,
                        "timestamp": time.time(),
                    }
                    enqueue_payload(loop, queue, payload)
                    last_emit = now
    except Exception:
        LOGGER.exception("Expression detection loop crashed")
    finally:
        stop_event.set()
        if writer is not None:
            writer.release()
        if args.display:
            cv2.destroyAllWindows()
        enqueue_payload(loop, queue, {"type": "shutdown"})
        LOGGER.info("Expression detection stopped")


def main() -> None:
    parser = build_arg_parser()
    args = parser.parse_args()
    configure_logging(args.log_level)

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    queue: asyncio.Queue = asyncio.Queue(maxsize=4)
    stop_event = threading.Event()

    detection_thread = threading.Thread(
        target=run_detection,
        args=(args, loop, queue, stop_event),
        name="expression-detection",
        daemon=True,
    )
    detection_thread.start()

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop_event.set)
        except NotImplementedError:
            pass

    try:
        loop.run_until_complete(websocket_main(args, queue, stop_event))
    finally:
        stop_event.set()
        detection_thread.join(timeout=2.0)
        loop.stop()
        loop.close()


if __name__ == "__main__":
    main()
