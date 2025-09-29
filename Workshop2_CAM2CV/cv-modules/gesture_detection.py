"""Real-time gesture bridge that feeds the MASS60 fireworks visual via WebSocket.

The script pulls frames from the ESP32 MJPEG endpoint, runs MediaPipe Hands to
classify coarse gestures, and broadcasts high-level parameters (hue, launch
power, spark density, twist) to the p5.js visual. It also offers an optional
OpenCV preview window to debug the recogniser during workshops.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import math
import signal
import threading
import time
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Tuple

import sys

import cv2
import mediapipe as mp
import websockets
from mediapipe.framework.formats import landmark_pb2

from utils.overlays import draw_hud
from utils.stream_client import FrameRateTracker, MJPEGStream

LOGGER = logging.getLogger("gesture_bridge")

FINGER_TIPS = (4, 8, 12, 16, 20)
FINGER_PIPS = (3, 6, 10, 14, 18)
INDEX_TIP = 8
MIDDLE_TIP = 12
RING_TIP = 16
PINKY_TIP = 20
THUMB_TIP = 4
WRIST = 0
THUMB_MCP = 2
MIDDLE_MCP = 9
INDEX_MCP = 5
PINKY_MCP = 17
PALM_BASE = (WRIST, INDEX_MCP, MIDDLE_MCP, PINKY_MCP)


@dataclass
class GestureResult:
    gesture: str
    confidence: float
    hue: float
    launch_power: float
    spark_density: float
    twist: float
    center: Tuple[float, float]
    handedness: str
    spread: float
    pinch_strength: float
    style: str

    def to_payload(self) -> Dict[str, object]:
        return {
            "type": "gesture",
            "gesture": self.gesture,
            "confidence": round(self.confidence, 3),
            "hue": round(self.hue, 2),
            "launch_power": round(self.launch_power, 3),
            "spark_density": round(self.spark_density, 3),
            "twist": round(self.twist, 3),
            "handedness": self.handedness,
            "center": {
                "x": round(self.center[0], 3),
                "y": round(self.center[1], 3),
            },
            "spread": round(self.spread, 3),
            "pinch_strength": round(self.pinch_strength, 3),
            "style": self.style,
            "timestamp": time.time(),
        }



class GestureSmoother:
    """Temporal smoother for gesture labels and numeric params.

    - Debounces quick label flips using a sliding window vote
    - Applies EMA low-pass filtering to numeric parameters
    """

    def __init__(self, window: int = 8, min_switch_count: int = 4, min_confidence: float = 0.65, ema: float = 0.25) -> None:
        from collections import deque
        self.window = max(3, int(window))
        self.min_switch = max(2, int(min_switch_count))
        self.min_conf = float(min_confidence)
        self.alpha = float(ema)
        self.history = deque(maxlen=self.window)
        self.current_label: Optional[str] = None
        self.smoothed: Optional[GestureResult] = None

    def _ema(self, prev: float, new: float) -> float:
        a = self.alpha
        return prev * (1 - a) + new * a

    def update(self, new: GestureResult) -> GestureResult:
        # Track vote history for labels (only count confident frames)
        if new.confidence >= self.min_conf or new.gesture == "idle":
            self.history.append(new.gesture)

        # Decide next label
        next_label = new.gesture
        if self.current_label is None:
            self.current_label = next_label
        else:
            if next_label != self.current_label:
                # Vote within window
                counts = {}
                for g in self.history:
                    counts[g] = counts.get(g, 0) + 1
                candidate = max(counts.items(), key=lambda kv: kv[1])[0] if counts else next_label
                if candidate == self.current_label:
                    next_label = self.current_label
                else:
                    # Only switch when candidate is sufficiently represented
                    if counts.get(candidate, 0) >= self.min_switch and new.confidence >= self.min_conf:
                        next_label = candidate
                    else:
                        next_label = self.current_label

        self.current_label = next_label

        if self.smoothed is None:
            # Seed smoothed with first observation but enforce chosen label
            self.smoothed = GestureResult(
                gesture=next_label,
                confidence=new.confidence,
                hue=new.hue,
                launch_power=new.launch_power,
                spark_density=new.spark_density,
                twist=new.twist,
                center=new.center,
                handedness=new.handedness,
                spread=new.spread,
                pinch_strength=new.pinch_strength,
                style=new.style,
            )
            return self.smoothed

        # Apply EMA to numeric params; copy label/style from decision
        s = self.smoothed
        self.smoothed = GestureResult(
            gesture=next_label,
            confidence=self._ema(s.confidence, new.confidence),
            hue=self._ema(s.hue, new.hue),
            launch_power=self._ema(s.launch_power, new.launch_power),
            spark_density=self._ema(s.spark_density, new.spark_density),
            twist=self._ema(s.twist, new.twist),
            center=(self._ema(s.center[0], new.center[0]), self._ema(s.center[1], new.center[1])),
            handedness=new.handedness,
            spread=self._ema(s.spread, new.spread),
            pinch_strength=self._ema(s.pinch_strength, new.pinch_strength),
            style=new.style,
        )
        return self.smoothed


def clamp(value: float, min_value: float, max_value: float) -> float:
    return max(min_value, min(value, max_value))


def distance(a: landmark_pb2.NormalizedLandmark, b: landmark_pb2.NormalizedLandmark) -> float:
    return math.dist((a.x, a.y), (b.x, b.y))


def average(points: Iterable[Tuple[float, float]]) -> Tuple[float, float]:
    xs, ys = zip(*points)
    return sum(xs) / len(xs), sum(ys) / len(ys)


class GestureClassifier:
    def __init__(self) -> None:
        self._style_map: Dict[str, str] = {
            "open_palm": "nebula",
            "stop": "nebula",
            "five": "nebula",
            "four": "aurora",
            "three": "galaxy",
            "three2": "galaxy",
            "fist": "meteor",
            "peace": "galaxy",
            "victory": "galaxy",
            "two_up_inverted": "galaxy",
            "pinch": "stardust",
            "ok": "stardust",
            "thumbs_up": "aurora",
            "thumbs_down": "meteor",
            "point": "comet",
            "pistol": "comet",
            "rock": "meteor",
            "shaka": "aurora",
            "call": "aurora",
            "mixed": "spark",
            "idle": "embers",
        }

    def idle(self) -> GestureResult:
        return GestureResult(
            gesture="idle",
            confidence=0.2,
            hue=210.0,
            launch_power=0.35,
            spark_density=0.2,
            twist=0.0,
            center=(0.5, 0.6),
            handedness="unknown",
            spread=0.1,
            pinch_strength=0.0,
            style=self._style_map["idle"],
        )

    def classify(
        self,
        landmarks: landmark_pb2.NormalizedLandmarkList,
        handedness: str,
    ) -> GestureResult:
        lm: List[landmark_pb2.NormalizedLandmark] = list(landmarks.landmark)
        wrist = lm[WRIST]
        middle_mcp = lm[MIDDLE_MCP]
        index_mcp = lm[INDEX_MCP]
        pinky_mcp = lm[PINKY_MCP]

        palm_reference = distance(wrist, middle_mcp) + distance(index_mcp, pinky_mcp)
        palm_reference = max(palm_reference, 1e-3)

        finger_states = self._finger_states(lm, handedness)
        finger_count = sum(1 for state in finger_states.values() if state)

        thumb_tip = lm[THUMB_TIP]
        index_tip = lm[INDEX_TIP]
        pinch_gap = distance(thumb_tip, index_tip)
        pinch_strength = clamp(1.0 - pinch_gap / (0.35 * palm_reference), 0.0, 1.0)

        tip_points = [lm[idx] for idx in (INDEX_TIP, MIDDLE_TIP, RING_TIP, PINKY_TIP)]
        tip_spread = sum(distance(p, middle_mcp) for p in tip_points) / (len(tip_points) * palm_reference)
        tip_spread = clamp(tip_spread, 0.0, 2.0)

        centroid = average([(lm[idx].x, lm[idx].y) for idx in PALM_BASE])
        center = (clamp(centroid[0], 0.0, 1.0), clamp(centroid[1], 0.0, 1.0))
        # Compute twist early because it is used in gesture rules below
        twist = self._resolve_twist(lm)

        gesture = "mixed"
        confidence = 0.6

        # Helper booleans
        thumb = finger_states["thumb"]
        index = finger_states["index"]
        middle = finger_states["middle"]
        ring = finger_states["ring"]
        pinky = finger_states["pinky"]

        # Quick canonical gestures (order: specific > general)
        # OK vs Pinch: small gap between thumb and index, independent of index extension
        if pinch_strength > 0.72:
            if (int(middle) + int(ring) + int(pinky)) >= 2:
                gesture = "ok"; confidence = 0.93
            else:
                gesture = "pinch"; confidence = 0.92
        # Four (no thumb), then Five (all five)
        elif index and middle and ring and pinky and not thumb:
            gesture = "four"; confidence = clamp(0.72 + tip_spread * 0.4, 0.0, 1.0)
        elif index and middle and ring and pinky and thumb:
            gesture = "five"; confidence = clamp(0.72 + tip_spread * 0.4, 0.0, 1.0)
        # General open palm
        elif finger_count >= 4 and pinch_strength < 0.5:
            gesture = "open_palm"; confidence = clamp(0.7 + tip_spread * 0.4, 0.0, 1.0)
        # Closed fist
        elif finger_count == 0 and pinch_strength < 0.3:
            gesture = "fist"; confidence = 0.95
        # V-sign variants
        elif index and middle and not ring and not pinky and abs(twist) > 0.25:
            gesture = "victory"; confidence = 0.86
        elif index and middle and not ring and not pinky:
            gesture = "peace"; confidence = 0.85
        # Rock: index + pinky extended, middle+ring closed; allow fallback by spread
        elif (index and not middle and not ring and pinky) or (
            (not middle and not ring) and
            distance(index_tip, middle_mcp) > 0.35 * palm_reference and
            distance(lm[PINKY_TIP], middle_mcp) > 0.32 * palm_reference
        ):
            gesture = "rock"; confidence = 0.86
        # Call/Shaka family
        elif thumb and not index and not middle and not ring and pinky and abs(twist) > 0.25:
            gesture = "call"; confidence = 0.86
        elif thumb and not index and not middle and not ring and pinky:
            gesture = "shaka"; confidence = 0.86
        # Pistol / Point family
        elif thumb and index and not middle and not ring and not pinky:
            gesture = "pistol"; confidence = 0.84
        elif index and middle and ring and not pinky and not thumb:
            gesture = "three"; confidence = 0.82
        elif index and middle and not ring and not pinky and thumb:
            gesture = "two_up_inverted"; confidence = 0.82
        elif middle and ring and pinky and not index and not thumb:
            gesture = "three2"; confidence = 0.8
        elif finger_count == 1 and index:
            gesture = "point"; confidence = 0.78
        elif self._is_thumb_up(lm, finger_states):
            gesture = "thumbs_up"; confidence = 0.82
        else:
            # Thumbs down heuristic: thumb extended and below wrist vertically
            wrist = lm[WRIST]
            thumb_tip = lm[THUMB_TIP]
            if thumb and (thumb_tip.y > wrist.y + 0.05) and not index and not middle and not ring and not pinky:
                gesture = "thumbs_down"; confidence = 0.82


        hue = self._resolve_hue(gesture, center)
        launch_power = clamp(1.0 - center[1], 0.1, 0.98)
        spark_density = clamp(0.3 + tip_spread * 0.9, 0.1, 1.6)
        if gesture == "fist":
            spark_density = clamp(1.4 + pinch_strength * 0.3, 1.0, 1.9)

        return GestureResult(
            gesture=gesture,
            confidence=confidence,
            hue=hue,
            launch_power=launch_power,
            spark_density=spark_density,
            twist=twist,
            center=center,
            handedness=handedness.lower(),
            spread=tip_spread,
            pinch_strength=pinch_strength,
            style=self._style_map.get(gesture, "spark"),
        )

    def _finger_states(
        self,
        landmarks: List[landmark_pb2.NormalizedLandmark],
        handedness: str,
    ) -> Dict[str, bool]:
        states = {}
        thumb_tip = landmarks[THUMB_TIP]
        thumb_ip = landmarks[FINGER_PIPS[0]]

        if handedness.lower() == "left":
            states["thumb"] = thumb_tip.x > thumb_ip.x + 0.01
        else:
            states["thumb"] = thumb_tip.x < thumb_ip.x - 0.01

        names = ["index", "middle", "ring", "pinky"]
        for idx, name in enumerate(names, start=1):
            tip = landmarks[FINGER_TIPS[idx]]
            pip = landmarks[FINGER_PIPS[idx]]
            states[name] = tip.y < pip.y - 0.02

        return states

    @staticmethod
    def _is_thumb_up(
        landmarks: List[landmark_pb2.NormalizedLandmark],
        states: Dict[str, bool],
    ) -> bool:
        thumb_tip = landmarks[THUMB_TIP]
        thumb_mcp = landmarks[THUMB_MCP]
        wrist = landmarks[WRIST]
        return (
            states["thumb"]
            and not states["index"]
            and not states["middle"]
            and not states["ring"]
            and not states["pinky"]
            and thumb_tip.y < thumb_mcp.y
            and thumb_tip.y < wrist.y
        )

    @staticmethod
    def _resolve_hue(gesture: str, center: Tuple[float, float]) -> float:
        base = center[0] * 360.0
        if gesture == "fist":
            return 20.0 + center[1] * 10.0
        if gesture == "peace":
            return 300.0 + (1.0 - center[1]) * 40.0
        if gesture == "thumbs_up":
            return 90.0 + center[1] * 60.0
        if gesture == "pinch":
            return 210.0 + center[0] * 120.0
        if gesture == "point":
            return 45.0 + center[0] * 80.0
        return base

    @staticmethod
    def _resolve_twist(landmarks: List[landmark_pb2.NormalizedLandmark]) -> float:
        wrist = landmarks[WRIST]
        index_tip = landmarks[INDEX_TIP]
        ring_tip = landmarks[RING_TIP]
        lateral = (index_tip.x - ring_tip.x) * 0.6
        forward = (wrist.y - index_tip.y) * 0.4
        return clamp(lateral + forward, -0.8, 0.8)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Gesture bridge for the fireworks visual")
    parser.add_argument("--source", default="http://192.168.4.1/stream", help="MJPEG stream URL (ignored if --webcam)")
    parser.add_argument("--webcam", action="store_true", help="Use local webcam instead of ESP32 stream")
    parser.add_argument("--camera-index", type=int, default=0, help="Webcam index to use when --webcam is set")
    parser.add_argument("--display", action="store_true", help="Show annotated OpenCV window")
    parser.add_argument("--save", type=str, help="Optional MP4 path for recording annotated frames")
    parser.add_argument("--ws-host", default="0.0.0.0", help="WebSocket bind host")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket bind port")
    parser.add_argument("--ws-path", default="/fireworks", help="WebSocket path clients must use")
    parser.add_argument("--ws-rate", type=float, default=20.0, help="Max events per second to broadcast")
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"], help="Logger level")
    # Backend: rules (default) or tasks (MediaPipe Tasks)
    parser.add_argument("--gesture-backend", default="rules", choices=["rules", "tasks"], help="Gesture backend: rules or tasks")
    parser.add_argument("--gesture-model", default=None, help="Path to gesture_recognizer.task (required when --gesture-backend tasks)")
    return parser


def enqueue_payload(loop: asyncio.AbstractEventLoop, queue: asyncio.Queue, payload: Dict[str, object]) -> None:
    def _put() -> None:
        try:
            if queue.full():
                try:
                    queue.get_nowait()
                except asyncio.QueueEmpty:
                    pass
            queue.put_nowait(payload)
        except Exception:
            pass

    try:
        loop.call_soon_threadsafe(_put)
    except Exception:
        pass


def run_detection(
    args: argparse.Namespace,
    loop: asyncio.AbstractEventLoop,
    queue: asyncio.Queue,
    stop_event: threading.Event,
) -> None:
    if getattr(args, "webcam", False):
        LOGGER.info("Starting webcam capture (index %d)", args.camera_index)
    else:
        LOGGER.info("Starting MJPEG reader: %s", args.source)
    fps_tracker = FrameRateTracker()
    classifier = GestureClassifier()

    # Backend selection: rules (MediaPipe Hands + heuristics) or tasks (MediaPipe Tasks GestureRecognizer)
    backend = getattr(args, "gesture_backend", "rules")
    recognizer = None
    mp_hands = None
    if backend == "tasks":
        try:
            from mediapipe.tasks import python as mp_python
            from mediapipe.tasks.python import vision as mp_vision
            from mediapipe.tasks.python.core.base_options import BaseOptions
        except Exception as e:
            LOGGER.error("Failed to import MediaPipe Tasks modules: %s", e)
            backend = "rules"

    if backend == "tasks":
        model_path = args.gesture_model
        if not model_path:
            import os
            # Default to resources/models/gesture_recognizer.task relative to repo
            default_path = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "resources", "models", "gesture_recognizer.task"))
            model_path = default_path
            LOGGER.info("Using default gesture model path: %s", model_path)
        try:
            base = BaseOptions(model_asset_path=model_path)
            options = mp_vision.GestureRecognizerOptions(base_options=base)
            recognizer = mp_vision.GestureRecognizer.create_from_options(options)
            LOGGER.info("Gesture backend: tasks (%s)", model_path)
        except Exception as e:
            LOGGER.error("Failed to load gesture_recognizer.task at %s: %s", model_path, e)
            backend = "rules"

    if backend == "rules":
        mp_hands = mp.solutions.hands.Hands(
            max_num_hands=1,
            min_detection_confidence=0.7,
            min_tracking_confidence=0.7,
            model_complexity=1,
        )
        LOGGER.info("Gesture backend: rules (MediaPipe Hands + heuristics)")

    smoother = GestureSmoother(window=8, min_switch_count=4, min_confidence=0.65, ema=0.25)

    last_emit = 0.0
    last_presence = 0.0
    last_log = 0.0
    last_logged_gesture: Optional[str] = None
    writer = None
    window_name = "Gesture Bridge"

    try:
        # Choose source: webcam or ESP32 MJPEG
        if getattr(args, "webcam", False):
            from utils.stream_client import WebcamStream
            source_ctx = WebcamStream(index=args.camera_index)
        else:
            source_ctx = MJPEGStream(args.source)
        with source_ctx as stream:
            for frame in stream.frames():
                if stop_event.is_set():
                    break

                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

                gesture_result: GestureResult
                now = time.time()

                if backend == "tasks" and recognizer is not None:
                    # Use MediaPipe Tasks GestureRecognizer
                    try:
                        from mediapipe.tasks.python.vision.core.vision_task_running_mode import VisionTaskRunningMode  # type: ignore
                    except Exception:
                        VisionTaskRunningMode = None  # type: ignore
                    # Create mp.Image from numpy RGB
                    from mediapipe import Image
                    mp_image = Image(image_format=Image.ImageFormat.SRGB, data=rgb)
                    try:
                        result = recognizer.recognize(mp_image)
                    except Exception as e:
                        LOGGER.debug("Gesture recognizer failed: %s", e)
                        result = None
                    label = "idle"; score = 0.0
                    if result and getattr(result, "gestures", None):
                        candidates = result.gestures[0]
                        if candidates:
                            top = max(candidates, key=lambda c: getattr(c, "score", 0.0))
                            label = getattr(top, "category_name", "idle")
                            score = float(getattr(top, "score", 0.0))
                    # Build a minimal GestureResult compatible with payload
                    # Use stable defaults for fireworks fields (front-end bubbles ignore them)
                    gesture_result = smoother.update(
                        GestureResult(
                            gesture=label or "idle",
                            confidence=score,
                            hue=210.0,
                            launch_power=0.35,
                            spark_density=0.4,
                            twist=0.0,
                            center=(0.5, 0.6),
                            handedness="unknown",
                            spread=0.0,
                            pinch_strength=0.0,
                            style="embers",
                        )
                    )
                    if label != "idle":
                        last_presence = now
                else:
                    # Use rule-based (MediaPipe Hands)
                    result = mp_hands.process(rgb) if mp_hands is not None else None
                    if result and result.multi_hand_landmarks:
                        handedness = "unknown"
                        if result.multi_handedness:
                            handedness = result.multi_handedness[0].classification[0].label
                        raw_result = classifier.classify(result.multi_hand_landmarks[0], handedness)
                        gesture_result = smoother.update(raw_result)
                        last_presence = now
                    else:
                        if now - last_presence > 0.75:
                            raw_result = classifier.idle()
                        else:
                            # Avoid hammering idle payloads when the hand just left the frame
                            raw_result = classifier.idle()
                            raw_result.confidence = 0.05
                        gesture_result = smoother.update(raw_result)

                fps = fps_tracker.update()
                draw_hud(
                    frame,
                    [
                        f"Gesture: {gesture_result.gesture} ({gesture_result.confidence:.2f})",
                        f"Hue: {gesture_result.hue:.0f} | Launch: {gesture_result.launch_power:.2f}",
                        f"Density: {gesture_result.spark_density:.2f} | Twist: {gesture_result.twist:.2f}",
                        f"Center: {gesture_result.center[0]:.2f}, {gesture_result.center[1]:.2f}",
                    ],
                    origin=(16, 120),
                    color=(120, 220, 255),
                    scale=0.7,
                )
                draw_hud(
                    frame,
                    [
                        f"Source: {args.source}",
                        f"FPS: {fps:.1f}",
                        f"WS: ws://{args.ws_host}:{args.ws_port}{args.ws_path}",
                    ],
                )

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

                if now - last_log >= 2.0:
                    LOGGER.info("Gesture %s (%.2f)", gesture_result.gesture, gesture_result.confidence)
                    last_log = now

                if now - last_emit >= 1.0 / max(args.ws_rate, 1):
                    payload = gesture_result.to_payload()
                    enqueue_payload(loop, queue, payload)
                    last_emit = now
                    if payload["gesture"] != last_logged_gesture:
                        LOGGER.info(
                            "Gesture %s -> hue %.0f launch %.2f density %.2f",
                            payload["gesture"],
                            payload["hue"],
                            payload["launch_power"],
                            payload["spark_density"],
                        )
                        last_logged_gesture = payload["gesture"]
    except Exception:
        LOGGER.exception("Gesture detection loop crashed")
    finally:
        stop_event.set()
        if writer is not None:
            writer.release()
        if args.display:
            cv2.destroyAllWindows()
        if mp_hands is not None:
            mp_hands.close()
        enqueue_payload(loop, queue, {"type": "shutdown"})
        LOGGER.info("Gesture detection stopped")


class WebSocketHub:
    def __init__(self) -> None:
        self._clients: List[object] = []
        self._lock = asyncio.Lock()

    async def register(self, ws: object) -> None:
        async with self._lock:
            self._clients.append(ws)
        LOGGER.info("Client connected (%d total)", len(self._clients))
        hello = json.dumps({"type": "hello", "message": "gesture-bridge-ready"})
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
            targets = list(self._clients)
        if not targets:
            return
        coroutines = []
        for client in targets:
            coroutines.append(self._safe_send(client, message))
        await asyncio.gather(*coroutines, return_exceptions=True)

    @staticmethod
    async def _safe_send(client: object, message: str) -> None:
        try:
            await client.send(message)
        except Exception as exc:  # noqa: BLE001
            LOGGER.debug("Failed to send to client: %s", exc)


async def websocket_main(args: argparse.Namespace, queue: asyncio.Queue, stop_event: threading.Event) -> None:
    hub = WebSocketHub()

    async def handler(ws: object, path: str | None = None) -> None:
        # Support websockets>=12 where handler receives only (ws) and path is available as ws.path
        effective_path = path if path is not None else getattr(ws, "path", None)
        if args.ws_path and effective_path is not None and effective_path != args.ws_path:
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
            message = json.dumps(payload)
            await hub.broadcast(message)
    finally:
        server.close()
        await server.wait_closed()
        LOGGER.info("WebSocket server stopped")


def configure_logging(level: str) -> None:
    logging.basicConfig(
        level=getattr(logging, level.upper()),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )


def main() -> None:
    parser = build_arg_parser()
    args = parser.parse_args()
    configure_logging(args.log_level)

    if args.display and sys.platform.startswith("win"):
        LOGGER.warning("Disabling --display on Windows to avoid OpenCV GUI freeze; use the browser visual instead.")
        args.display = False

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    queue: asyncio.Queue = asyncio.Queue(maxsize=4)
    stop_event = threading.Event()

    detection_thread = threading.Thread(
        target=run_detection,
        args=(args, loop, queue, stop_event),
        name="gesture-detection",
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
