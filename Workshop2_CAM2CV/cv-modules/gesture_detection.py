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

import cv2
import mediapipe as mp
import numpy as np
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
            "fist": "meteor",
            "peace": "galaxy",
            "pinch": "stardust",
            "thumbs_up": "aurora",
            "point": "comet",
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

        gesture = "mixed"
        confidence = 0.6

        if finger_states["index"] and finger_states["thumb"] and pinch_strength > 0.6:
            gesture = "pinch"
            confidence = 0.92
        elif finger_count >= 4 and pinch_strength < 0.5:
            gesture = "open_palm"
            confidence = clamp(0.7 + tip_spread * 0.4, 0.0, 1.0)
        elif finger_count == 0 and pinch_strength < 0.3:
            gesture = "fist"
            confidence = 0.95
        elif (
            finger_states["index"]
            and finger_states["middle"]
            and not finger_states["ring"]
            and not finger_states["pinky"]
        ):
            gesture = "peace"
            confidence = 0.85
        elif self._is_thumb_up(lm, finger_states):
            gesture = "thumbs_up"
            confidence = 0.8
        elif finger_count == 1 and finger_states["index"]:
            gesture = "point"
            confidence = 0.75

        hue = self._resolve_hue(gesture, center)
        launch_power = clamp(1.0 - center[1], 0.1, 0.98)
        spark_density = clamp(0.3 + tip_spread * 0.9, 0.1, 1.6)
        if gesture == "fist":
            spark_density = clamp(1.4 + pinch_strength * 0.3, 1.0, 1.9)
        twist = self._resolve_twist(lm)

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
        thumb_mcp = landmarks[INDEX_MCP]
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
    parser.add_argument("--source", default="http://192.168.4.1/stream", help="MJPEG stream URL")
    parser.add_argument("--display", action="store_true", help="Show annotated OpenCV window")
    parser.add_argument("--save", type=str, help="Optional MP4 path for recording annotated frames")
    parser.add_argument("--ws-host", default="0.0.0.0", help="WebSocket bind host")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket bind port")
    parser.add_argument("--ws-path", default="/fireworks", help="WebSocket path clients must use")
    parser.add_argument("--ws-rate", type=float, default=20.0, help="Max events per second to broadcast")
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


def run_detection(
    args: argparse.Namespace,
    loop: asyncio.AbstractEventLoop,
    queue: asyncio.Queue,
    stop_event: threading.Event,
) -> None:
    LOGGER.info("Starting MJPEG reader: %s", args.source)
    fps_tracker = FrameRateTracker()
    classifier = GestureClassifier()
    mp_hands = mp.solutions.hands.Hands(
        max_num_hands=1,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5,
        model_complexity=1,
    )

    last_emit = 0.0
    last_presence = 0.0
    last_logged_gesture: Optional[str] = None
    writer = None
    window_name = "Gesture Bridge"

    try:
        with MJPEGStream(args.source) as stream:
            for frame in stream.frames():
                if stop_event.is_set():
                    break

                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                result = mp_hands.process(rgb)

                gesture_result: GestureResult
                now = time.time()

                if result.multi_hand_landmarks:
                    handedness = "unknown"
                    if result.multi_handedness:
                        handedness = result.multi_handedness[0].classification[0].label
                    gesture_result = classifier.classify(result.multi_hand_landmarks[0], handedness)
                    last_presence = now
                else:
                    if now - last_presence > 0.75:
                        gesture_result = classifier.idle()
                    else:
                        # Avoid hammering idle payloads when the hand just left the frame
                        gesture_result = classifier.idle()
                        gesture_result.confidence = 0.05

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
        mp_hands.close()
        enqueue_payload(loop, queue, {"type": "shutdown"})
        LOGGER.info("Gesture detection stopped")


class WebSocketHub:
    def __init__(self) -> None:
        self._clients: List[websockets.WebSocketServerProtocol] = []
        self._lock = asyncio.Lock()

    async def register(self, ws: websockets.WebSocketServerProtocol) -> None:
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
            targets = [client for client in self._clients if not client.closed]
        if not targets:
            return
        coroutines = []
        for client in targets:
            coroutines.append(self._safe_send(client, message))
        await asyncio.gather(*coroutines, return_exceptions=True)

    @staticmethod
    async def _safe_send(client: websockets.WebSocketServerProtocol, message: str) -> None:
        try:
            await client.send(message)
        except Exception as exc:  # noqa: BLE001
            LOGGER.debug("Failed to send to client: %s", exc)


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
