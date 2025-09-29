# p5.js Visualization Starter

This front-end shows how to mirror the ESP32S3 video stream inside the browser and layer custom p5.js artwork on top. Use it to prototype interactive overlays that react to your computer vision models.

## Folder Layout

```
webcam-starter/
  web/
    index.html             Entry point served by Live Server
    styles.css             Dark UI styling
    app.js                 Boots the VisualEngine with registered visuals
    visual-engine.js       Shared runtime that fetches the MJPEG stream
    visualizations/
      template-minimal.js  Baseline overlay example
      fireworks.js         Motion-triggered particle system demo
```

## Running with VS Code Live Server

1. Install the **Live Server** extension in VS Code.
2. Open `MASS60_CV_Workshop/webcam-starter/web/index.html`.
3. Right-click inside the editor and choose **Open with Live Server**. The page opens at `http://127.0.0.1:5500/` (port may vary).
4. Update the stream URL field to match your ESP32S3 endpoint (e.g., `http://192.168.4.1/stream`).
5. If the browser blocks mixed content (HTTP video from an HTTPS page), allow it via the site settings or run Live Server over HTTP only.


### CV WebSocket bridge

The `visualizations/fireworks.js` visual consumes JSON over WebSocket (default `ws://<host>:8765/fireworks`). Override via `?ws=ws://localhost:8765/fireworks` in the page URL. Start either `cv-modules/gesture_detection.py` or `cv-modules/facial_expression_recognition.py` to drive the effect.

## Adding New Visuals

1. Duplicate `visualizations/template-minimal.js`.
2. Rename the file and register a new visual:
   ```js
   window.VISUALS.push({
     id: "my-visual",
     name: "My Visual",
     init: ({ engine, p }) => {/* setup */},
     draw: ({ engine, p, fps, elapsed }) => {/* render */},
     dispose: ({ engine, p }) => {/* teardown */},
   });
   ```
3. Add a `<script>` tag for the new file in `index.html` after `visual-engine.js`.

The `VisualEngine` exposes helper methods:

- `engine.drawCamera(p, { mirror: true })` - paints the MJPEG frame onto the canvas.
- `engine.currentFPS` - current render loop FPS.
- `engine.streamUrl` - current camera endpoint (updateable via UI).
- `engine.togglePause()` - pause/resume the draw loop (mapped to the spacebar).

## Offline Bundles

If you need to run without internet access, download `p5.min.js` ahead of time and place it in `web/lib/`. Then update `index.html` to load the script locally:

```html
<script defer src="./lib/p5.min.js"></script>
```

## Teaching Flow

1. Start with **Minimal HUD** to explain how the stream is ingested.
2. Switch to **Motion Fireworks** to showcase reactive visuals and basic frame differencing.
3. Encourage learners to pipe JSON results from the Python CV modules (e.g., via WebSocket) and render them inside a new visualization.

All captions, comments, and UI labels remain in English to stay consistent with the rest of the workshop assets.
