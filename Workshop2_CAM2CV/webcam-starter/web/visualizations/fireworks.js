
// Gesture-reactive fireworks visual for the MASS60 workshop.
(function () {
  window.VISUALS = window.VISUALS || [];

  const TWO_PI = Math.PI * 2;

  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
  const lerp = (a, b, t) => a + (b - a) * t;
  const lerpHue = (a, b, t) => {
    let delta = ((b - a + 540) % 360) - 180;
    return (a + delta * t + 360) % 360;
  };

  class Particle {
    constructor(x, y, vx, vy, hue, span, size) {
      this.x = x;
      this.y = y;
      this.vx = vx;
      this.vy = vy;
      this.hue = hue;
      this.life = 0;
      this.maxLife = span;
      this.size = size;
      this.sparkle = Math.random() * 0.6;
      this.trail = [];
    }

    update(gravity, drag) {
      this.life += 1;
      this.trail.push({ x: this.x, y: this.y, alpha: 1 });
      if (this.trail.length > 8) {
        this.trail.shift();
      }
      for (const step of this.trail) {
        step.alpha *= 0.82;
      }
      const d = this.drag != null ? this.drag : drag;
      const g = this.gravity != null ? this.gravity : gravity;
      this.vx *= d;
      this.vy = this.vy * d + g;
      this.x += this.vx;
      this.y += this.vy;
      return this.life < this.maxLife;
    }

    draw(p) {
      const fade = 1 - this.life / this.maxLife;
      const alpha = clamp(fade * 255 + Math.sin(this.life * 0.4) * this.sparkle * 160, 0, 255);
      const a = alpha / 255;
      const ctx = p.drawingContext;
      ctx.save();
      ctx.shadowBlur = 10 + this.size * 3;
      ctx.shadowColor = `hsla(${this.hue}, 100%, 65%, ${Math.min(0.85, a)})`;
      p.stroke(this.hue, 220, 255, alpha);
      p.strokeWeight(this.size);
      p.point(this.x, this.y);
      ctx.restore();

      if (this.trail.length > 1) {
        p.push();
        p.noFill();
        p.stroke(this.hue, 140, 255, alpha * 0.35);
        p.beginShape();
        for (const step of this.trail) {
          p.vertex(step.x, step.y);
        }
        p.endShape();
        p.pop();
      }
    }
  }
  class Firework {
    constructor(x, baseY, state) {
      this.x = x;
      this.y = baseY;
      this.state = state;
      this.vx = (Math.random() - 0.5) * 1.1;
      this.vy = -3.1 - state.launchPower * 3.3;
      this.twist = state.twist;
      this.hue = state.hue;
      this.life = 0;
      this.exploded = false;
    }

    update(visual) {
      this.life += 1;
      this.vx *= 0.99;
      this.vx += this.twist * 0.015;
      this.x += this.vx;
      this.vy += visual.gravity * 0.42;
      this.y += this.vy;

      visual.spawnEmber(this.x, this.y, this.hue, this.state);

      const targetY = visual.engine.height * (0.25 - this.state.launchPower * 0.2);
      if (!this.exploded && (this.vy >= 0 || this.y <= targetY)) {
        this.exploded = true;
        visual.spawnBurst(this);
      }

      return !this.exploded && this.y < visual.engine.height + 30;
    }

    draw(p) {
      p.push();
      p.stroke(this.hue, 180, 255, 190);
      p.strokeWeight(2);
      p.point(this.x, this.y);
      p.pop();
    }
  }
  class FireworksVisual {
    constructor() {
      this.engine = null;
      this.p = null;
      this.fireworks = [];
      this.particles = [];
      this.embers = [];
      this.socket = null;
      this.socketStatus = "disconnected";
      this.socketUrl = this.resolveSocketUrl();
      this.idleState = {
        hue: 210,
        launchPower: 0.35,
        sparkDensity: 0.4,
        twist: 0,
        center: { x: 0.5, y: 0.6 },
        gesture: "idle",
        style: "embers",
        confidence: 0,
      };
      this.currentState = { ...this.idleState };
      this.targetState = { ...this.idleState };
      this.lastPayload = 0;
      this.emitCooldown = 0;
      this.gravity = 0.07;
      this.drag = 0.985;
    }

    init({ engine, p }) {
      this.engine = engine;
      this.p = p;
      this.connectSocket();
    }

    dispose() {
      if (this.socket) {
        this.socket.close(1000, "visual disposed");
        this.socket = null;
      }
      this.fireworks = [];
      this.particles = [];
      this.embers = [];
    }
    resolveSocketUrl() {
      try {
        const params = new URLSearchParams(window.location.search);
        const override = params.get("ws");
        if (override) {
          return override;
        }
      } catch (err) {
        // ignore when running from file://
      }
      const host = window.location.hostname || "localhost";
      const protocol = window.location.protocol === "https:" ? "wss" : "ws";
      return `${protocol}://${host}:8765/fireworks`;
    }

    connectSocket() {
      try {
        const socket = new WebSocket(this.socketUrl);
        this.socket = socket;
        this.socketStatus = "connecting";
        socket.onopen = () => {
          this.socketStatus = "connected";
        };
        socket.onerror = () => {
          this.socketStatus = "error";
        };
        socket.onclose = () => {
          if (this.socket === socket) {
            this.socket = null;
          }
          this.socketStatus = "disconnected";
          setTimeout(() => this.connectSocket(), 1500);
        };
        socket.onmessage = (event) => {
          try {
            const payload = JSON.parse(event.data);
            this.handleSocketPayload(payload);
          } catch (err) {
            console.warn("gesture payload parse error", err);
          }
        };
      } catch (err) {
        this.socketStatus = "error";
      }
    }

    handleSocketPayload(payload) {
      if (payload.type === "hello") {
        this.socketStatus = "connected";
        return;
      }
      if (payload.type !== "gesture") {
        return;
      }
      this.lastPayload = performance.now();
      this.targetState = {
        hue: payload.hue ?? this.targetState.hue,
        launchPower: clamp(payload.launch_power ?? this.targetState.launchPower, 0, 1),
        sparkDensity: clamp(payload.spark_density ?? this.targetState.sparkDensity, 0.2, 2.0),
        twist: clamp(payload.twist ?? this.targetState.twist, -1, 1),
        center: payload.center
          ? {
              x: clamp(payload.center.x ?? 0.5, 0, 1),
              y: clamp(payload.center.y ?? 0.6, 0, 1),
            }
          : { ...this.targetState.center },
        gesture: payload.gesture ?? this.targetState.gesture,
        style: payload.style ?? this.targetState.style,
        confidence: payload.confidence ?? this.targetState.confidence ?? 0,
      };
      this.emitCooldown = 0;
    }
    updateState() {
      const now = performance.now();
      if (now - this.lastPayload > 3500) {
        this.targetState = { ...this.idleState };
      }

      this.currentState.hue = lerpHue(this.currentState.hue, this.targetState.hue, 0.12);
      this.currentState.launchPower = lerp(this.currentState.launchPower, this.targetState.launchPower, 0.16);
      this.currentState.sparkDensity = lerp(this.currentState.sparkDensity, this.targetState.sparkDensity, 0.18);
      this.currentState.twist = lerp(this.currentState.twist, this.targetState.twist, 0.12);
      this.currentState.center = {
        x: lerp(this.currentState.center.x, this.targetState.center.x, 0.18),
        y: lerp(this.currentState.center.y, this.targetState.center.y, 0.18),
      };
      this.currentState.gesture = this.targetState.gesture;
      this.currentState.style = this.targetState.style;
      this.currentState.confidence = this.targetState.confidence;
    }

    launchIfNeeded() {
      if (this.emitCooldown > 0) {
        this.emitCooldown -= 1;
        return;
      }

      const launches = Math.max(1, Math.round(1 + this.currentState.sparkDensity * 1.5));
      const preferredX = clamp(this.currentState.center.x, 0.15, 0.85) * this.engine.width;
      for (let i = 0; i < launches; i += 1) {
        const jitter = (Math.random() - 0.5) * (120 - this.currentState.confidence * 80);
        const firework = new Firework(preferredX + jitter, this.engine.height + 12, {
          hue: this.currentState.hue,
          launchPower: clamp(this.currentState.launchPower, 0.15, 0.95),
          sparkDensity: this.currentState.sparkDensity,
          twist: this.currentState.twist,
          style: this.currentState.style,
        });
        this.fireworks.push(firework);
      }

      const cooldownBase = 18;
      this.emitCooldown = cooldownBase + Math.round((2 - this.currentState.sparkDensity) * 14);
    }
    spawnEmber(x, y, hue, state) {
      if (Math.random() > 0.55) {
        return;
      }
      const ember = new Particle(
        x,
        y,
        (Math.random() - 0.5) * 0.4,
        0.6 + Math.random() * 0.4,
        hue,
        24,
        1.2,
      );
      this.embers.push(ember);
    }

    getPatternFromStyle(style) {
      switch (style) {
        case "meteor":
          return Math.random() < 0.5 ? "crossette" : "palm";
        case "galaxy":
          return "chrysanthemum";
        case "stardust":
          return "ring";
        case "aurora":
          return "willow";
        case "nebula":
          return "pistil";
        case "comet":
          return "palm";
        default:
          return "chrysanthemum";
      }
    }


    spawnBurst(firework) {
      const palette = this.buildPalette(firework.state);
      const style = firework.state.style;
      const pattern = this.getPatternFromStyle(style);
      const density = clamp(firework.state.sparkDensity, 0.2, 2.0);
      const swirl = firework.state.twist * 1.0;

      let countBase = 120 + density * 90;
      if (pattern === "palm") countBase = 26 + density * 10;
      if (pattern === "ring") countBase = 90 + density * 60;
      if (pattern === "willow") countBase = 140 + density * 80;
      const count = Math.floor(countBase);

      for (let i = 0; i < count; i += 1) {
        const t = i / count;
        let angle = t * TWO_PI;
        let speed = 3.0 + density * 1.9;
        let life = 70 + Math.random() * 40;
        let size = 2 + Math.random() * 2;

        if (pattern === "chrysanthemum") {
          angle += swirl * 0.9;
        } else if (pattern === "ring") {
          angle += swirl * 0.6;
          speed = 3.2 + density * 1.3;
          life += 10;
        } else if (pattern === "willow") {
          speed = 2.6 + density * 1.0;
          life = 90 + Math.random() * 60;
        } else if (pattern === "palm") {
          if (i % Math.max(1, Math.floor(count / 8)) !== 0) continue;
          speed = 4.2 + density * 1.6;
          size = 3 + Math.random() * 2;
          life = 80 + Math.random() * 40;
        } else if (pattern === "crossette") {
          speed = 3.4 + density * 1.6;
        }

        const hue = palette[i % palette.length];
        const vx = Math.cos(angle) * speed;
        const vy = Math.sin(angle) * speed;
        const particle = new Particle(
          firework.x,
          firework.y,
          vx,
          vy,
          hue,
          life,
          size,
        );
        if (pattern === "willow") {
          particle.drag = 0.992;
          particle.gravity = this.gravity * 0.9;
        }
        if (pattern === "ring") {
          particle.drag = 0.987;
        }
        if (pattern === "crossette") {
          particle.pattern = "crossette";
          particle.splitAt = 0.45 + Math.random() * 0.12;
          particle.childrenSpawned = false;
          particle.size = 2.2 + Math.random() * 0.6;
        }
        this.particles.push(particle);
      }

      // Inner core (pistil) for extra depth
      if (pattern === "pistil" || pattern === "chrysanthemum") {
        const inner = 28 + Math.floor(density * 10);
        for (let i = 0; i < inner; i += 1) {
          const a = (i / inner) * TWO_PI + swirl * 0.4;
          const sp = 1.4 + Math.random() * 0.8;
          const hue = (firework.state.hue + 20) % 360;
          const vx = Math.cos(a) * sp;
          const vy = Math.sin(a) * sp;
          const p = new Particle(firework.x, firework.y, vx, vy, hue, 36 + Math.random() * 12, 1.8);
          p.drag = 0.985;
          this.particles.push(p);
        }
      }
    }

    buildPalette(state) {
      const base = state.hue % 360;
      if (state.style === "meteor") {
        return [base, (base + 12) % 360, (base + 24) % 360];
      }
      if (state.style === "galaxy") {
        return [base, (base + 90) % 360, (base + 270) % 360];
      }
      if (state.style === "aurora") {
        return [base, (base + 40) % 360, (base + 80) % 360];
      }
      return [base, (base + 45) % 360, (base + 315) % 360];
    }
    drawBackground(p) {
      p.push();
      p.noStroke();
      const gradient = p.drawingContext.createLinearGradient(0, 0, 0, this.engine.height);
      const hue = this.currentState.hue;
      gradient.addColorStop(0, `hsla(${hue}, 80%, 12%, 0.95)`);
      gradient.addColorStop(1, `hsla(${(hue + 30) % 360}, 90%, 5%, 0.98)`);
      p.drawingContext.fillStyle = gradient;
      p.rect(0, 0, this.engine.width, this.engine.height);
      p.pop();
    }

    update() {
      this.updateState();
      this.launchIfNeeded();

      this.fireworks = this.fireworks.filter((fw) => fw.update(this));
      this.embers = this.embers.filter((ember) => ember.update(this.gravity * 0.6, 0.94));

      const next = [];
      for (const particle of this.particles) {
        const alive = particle.update(this.gravity, this.drag);
        if (alive) {
          if (particle.pattern === "crossette" && !particle.childrenSpawned && particle.life > particle.maxLife * (particle.splitAt || 0.5)) {
            particle.childrenSpawned = true;
            const baseSpeed = 2.2 + Math.random() * 0.8;
            const angles = [0, Math.PI / 2, Math.PI, (3 * Math.PI) / 2];
            for (const a of angles) {
              const vx = Math.cos(a) * baseSpeed;
              const vy = Math.sin(a) * baseSpeed;
              const child = new Particle(particle.x, particle.y, vx, vy, particle.hue, 30 + Math.random() * 16, 1.6);
              child.drag = 0.985;
              next.push(child);
            }
          }
          next.push(particle);
        }
      }
      this.particles = next;
    }

    drawHud(p) {
      p.push();
      p.colorMode(p.RGB);
      p.noStroke();
      p.fill(10, 18, 36, 200);
      p.rect(16, this.engine.height - 84, 280, 68, 12);
      p.fill(126, 197, 255);
      p.textSize(15);
      p.textAlign(p.LEFT, p.TOP);
      p.text(`Gesture: ${this.currentState.gesture} (${Math.round(this.currentState.confidence * 100)}%)`, 28, this.engine.height - 76);
      p.text(
        `Hue ${this.currentState.hue.toFixed(0)} • Launch ${(this.currentState.launchPower * 100).toFixed(0)}%`,
        28,
        this.engine.height - 54,
      );
      p.text(`WS: ${this.socketStatus}` , 28, this.engine.height - 32);
      p.pop();
    }

    draw(ctx) {
      const { p, engine } = ctx;
      if (!this.engine) {
        this.engine = engine;
      }
      p.colorMode(p.HSB, 360, 255, 255, 255);
      this.drawBackground(p);
      engine.drawCamera(p, { mirror: true, opacity: 140 });

      this.update();

      // Additive blending for glow-rich particles
      p.push();
      p.blendMode(p.ADD);
      for (const ember of this.embers) {
        ember.draw(p);
      }
      for (const particle of this.particles) {
        particle.draw(p);
      }
      p.pop();

      // Normal blend for rocket heads
      for (const firework of this.fireworks) {
        firework.draw(p);
      }

      this.drawHud(p);
    }
  }
  const visual = new FireworksVisual();

  window.VISUALS.push({
    id: "fireworks",
    name: "Gesture Fireworks",
    init: (ctx) => visual.init(ctx),
    draw: (ctx) => visual.draw(ctx),
    dispose: () => visual.dispose(),
  });
})();
