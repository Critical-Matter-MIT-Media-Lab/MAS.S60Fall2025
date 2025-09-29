(function () {
  window.VISUALS = window.VISUALS || [];

  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  const lerp = (a, b, t) => a + (b - a) * t;

  // Stable color per gesture (HSB hue)
  const GESTURE_HUE = {
    open_palm: 190,
    stop: 0,
    five: 200,
    four: 40,
    three: 300,
    three2: 320,
    fist: 20,
    peace: 280,
    victory: 290,
    two_up_inverted: 260,
    pinch: 210,
    ok: 120,
    thumbs_up: 90,
    thumbs_down: 15,
    point: 45,
    pistol: 55,
    rock: 330,
    shaka: 160,
    call: 140,
    mixed: 210,
    idle: 210,
  };

  class Bubble {
    constructor(x, y, r, hue, alpha) {
      this.x = x;
      this.y = y;
      this.r = r;
      this.hue = hue;
      this.alpha = alpha;
      // Base upward drift, small lateral drift
      this.vxBase = (Math.random() - 0.5) * 0.15;
      this.vyBase = -0.35 - Math.random() * 0.35;
      // Seeds for smooth noise-driven motion
      this.seedX = Math.random() * 1000;
      this.seedY = Math.random() * 1000;
    }
    update(p, w, h, t) {
      // Smooth Perlin-noise motion for continuous trajectories
      const nx = p.noise(this.seedX, t * 0.15);
      const ny = p.noise(this.seedY, t * 0.18);
      const vx = this.vxBase + (nx - 0.5) * 0.35;
      const vy = this.vyBase + (ny - 0.5) * 0.25;
      this.x += vx;
      this.y += vy;
      // Wrap horizontally
      if (this.x < -this.r) this.x = w + this.r;
      if (this.x > w + this.r) this.x = -this.r;
      // When going above, gently re-enter from bottom without x jump
      if (this.y < -this.r - 4) {
        this.y = h + this.r + 12 + Math.random() * 20;
      }
    }
    draw(p) {
      p.push();
      p.noFill();
      p.stroke(this.hue, 90, 250, this.alpha);
      p.strokeWeight(2);
      p.circle(this.x, this.y, this.r * 2);
      // glossy highlight
      p.noStroke();
      p.fill(this.hue, 60, 255, this.alpha * 0.25);
      p.ellipse(this.x - this.r * 0.35, this.y - this.r * 0.35, this.r * 0.7, this.r * 0.55);
      p.pop();
    }
  }

  class BubblesVisual {
    constructor() {
      this.engine = null;
      this.p = null;
      this.socket = null;
      this.socketUrl = this.resolveSocketUrl();
      this.socketStatus = "disconnected";
      this.lastPayload = 0;

      this.currentGesture = "idle";
      this.currentHue = GESTURE_HUE[this.currentGesture] || 210;
      this.targetHue = this.currentHue;

      this.bubbles = [];
      this.time = 0; // global time for smooth noise

      // Halo effect when color changes
      this.halo = 0.0; // 0..1 intensity
      this.haloCenter = { x: 0, y: 0 };
    }

    init({ engine, p }) {
      this.engine = engine;
      this.p = p;
      p.colorMode(p.HSB, 360, 255, 255, 255);
      this.haloCenter = { x: engine.width * 0.5, y: engine.height * 0.5 };
      this.populateBubbles();
      this.connectSocket();
    }
    dispose() {
      if (this.socket) {
        this.socket.close(1000, "visual disposed");
        this.socket = null;
      }
      this.bubbles = [];
    }

    resolveSocketUrl() {
      try {
        const params = new URLSearchParams(window.location.search);
        const override = params.get("ws");
        if (override) return override;
      } catch (_) {}
      const host = window.location.hostname || "localhost";
      const protocol = window.location.protocol === "https:" ? "wss" : "ws";
      return `${protocol}://${host}:8765/fireworks`;
    }

    connectSocket() {
      try {
        const socket = new WebSocket(this.socketUrl);
        this.socket = socket;
        this.socketStatus = "connecting";
        socket.onopen = () => (this.socketStatus = "connected");
        socket.onerror = () => (this.socketStatus = "error");
        socket.onclose = () => {
          if (this.socket === socket) this.socket = null;
          this.socketStatus = "disconnected";
          setTimeout(() => this.connectSocket(), 1500);
        };
        socket.onmessage = (event) => {
          try {
            const payload = JSON.parse(event.data);
            this.handleSocketPayload(payload);
          } catch (_) {}
        };
      } catch (_) {
        this.socketStatus = "error";
      }
    }

    handleSocketPayload(payload) {
      if (payload.type !== "gesture") return;
      this.lastPayload = performance.now();
      const g = String(payload.gesture || "idle").toLowerCase();
      this.currentGesture = g;

      // If idle, do NOT change color; keep current/target hue as-is
      if (g === "idle") {
        return;
      }

      // Update halo origin to gesture center if provided
      if (payload.center && this.engine) {
        const cx = clamp(payload.center.x ?? 0.5, 0, 1) * this.engine.width;
        const cy = clamp(payload.center.y ?? 0.5, 0, 1) * this.engine.height;
        this.haloCenter = { x: cx, y: cy };
      }

      const mappedHue = GESTURE_HUE[g];
      const nextHue = mappedHue != null ? mappedHue : (payload.hue ?? this.targetHue);

      // Trigger halo only when hue meaningfully changes
      const hueDelta = Math.abs(((nextHue - this.targetHue + 540) % 360) - 180);
      if (hueDelta > 3) {
        this.halo = 1.0;
      }
      this.targetHue = nextHue;
    }

    populateBubbles() {
      const { width: w, height: h } = this.engine;
      const baseHue = this.currentHue;
      this.bubbles = [];
      const count = 12;
      for (let i = 0; i < count; i += 1) {
        const x = Math.random() * w;
        const y = Math.random() * h;
        const r = 18 + Math.random() * 36;
        const hue = (baseHue + (i * 17) % 60) % 360;
        const alpha = 120 + Math.random() * 60; // semi-transparent
        this.bubbles.push(new Bubble(x, y, r, hue, alpha));
      }
    }

    update() {
      // Global time advances smoothly for noise motion
      this.time += 0.008;
      // Fade to target hue
      this.currentHue = lerp(this.currentHue, this.targetHue, 0.12);
      // Update bubble hues and positions smoothly
      for (let i = 0; i < this.bubbles.length; i += 1) {
        const b = this.bubbles[i];
        const offset = (i * 9) % 40;
        b.hue = (this.currentHue + offset) % 360;
        b.update(this.p, this.engine.width, this.engine.height, this.time);
      }
      // Halo decay
      this.halo = Math.max(0, this.halo * 0.92 - 0.002);
      // Do not repopulate randomly; keep set persistent for continuity
    }

    drawBackground(p) {
      p.push();
      p.noStroke();
      p.fill(0, 0, 0, 200);
      p.rect(0, 0, this.engine.width, this.engine.height);
      p.pop();
    }

    draw(ctx) {
      const { p, engine } = ctx;
      if (!this.engine) this.engine = engine;
      p.colorMode(p.HSB, 360, 255, 255, 255);

      this.drawBackground(p);
      // Optional camera preview under bubbles
      engine.drawCamera(p, { mirror: true, opacity: 80 });

      this.update();

      // Halo glow on color change
      if (this.halo > 0.001) {
        p.push();
        p.blendMode(p.SCREEN);
        const ctx2 = p.drawingContext;
        const cx = this.haloCenter.x || this.engine.width * 0.5;
        const cy = this.haloCenter.y || this.engine.height * 0.5;
        const maxR = Math.hypot(this.engine.width, this.engine.height) * 0.6;
        const hue = Math.round(this.currentHue);
        const grad = ctx2.createRadialGradient(cx, cy, 8, cx, cy, maxR);
        grad.addColorStop(0, `hsla(${hue}, 100%, 60%, ${0.22 * this.halo})`);
        grad.addColorStop(1, `hsla(${hue}, 100%, 60%, 0)`);
        const prev = ctx2.fillStyle;
        ctx2.fillStyle = grad;
        ctx2.fillRect(0, 0, this.engine.width, this.engine.height);
        ctx2.fillStyle = prev;
        p.pop();
      }

      p.push();
      p.blendMode(p.SCREEN);
      for (const b of this.bubbles) b.draw(p);
      p.pop();

      // small status
      p.push();
      p.colorMode(p.RGB);
      p.fill(255, 255, 255, 90);
      p.textSize(12);
      p.textAlign(p.RIGHT, p.BOTTOM);
      p.text(`${this.currentGesture} • WS ${this.socketStatus}`, this.engine.width - 8, this.engine.height - 8);
      p.pop();
    }
  }

  const bubbles = new BubblesVisual();
  window.VISUALS.push({
    id: "bubbles",
    name: "Gesture Bubbles",
    init: (ctx) => bubbles.init(ctx),
    draw: (ctx) => bubbles.draw(ctx),
    dispose: () => bubbles.dispose(),
  });
})();

