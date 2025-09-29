// visual-engine.js
// Provides a reusable p5.js engine that mirrors the ESP32 MJPEG feed, manages
// visual selection, exposes helper methods (drawCamera, togglePause, saveFrame),
// and handles keyboard shortcuts for the MASS60 workshop visualisations.
(function () {
  window.VISUALS = window.VISUALS || [];

  class VisualEngine {
    constructor(options) {
      this.mount = options.mount;
      this.statusLabel = options.statusLabel;
      this.visualLabel = options.visualLabel;
      this.visualButtonsContainer = options.visualButtonsContainer;
      this.streamUrlInput = options.streamUrlInput;
      this.applyButton = options.applyButton;
      this.width = options.width || 640;
      this.height = options.height || 480;
      this.visuals = options.visuals || [];
      this.streamUrl = options.streamUrl || "http://192.168.4.1/stream";
      this.activeVisualIndex = 0;
      this.currentFPS = 0;
      this.paused = false;
      this._lastDraw = 0;
      this.activeVisual = null;
      this.cameraElement = null;
      this.canvas = null;
      this.p5 = null;

      this.initUI();
      this.createSketch();
    }

    initUI() {
      if (this.streamUrlInput) {
        this.streamUrlInput.value = this.streamUrl;
      }
      if (this.applyButton) {
        this.applyButton.addEventListener("click", () => {
          const url = this.streamUrlInput.value.trim();
          if (url) {
            this.updateStream(url);
          }
        });
      }
      this.renderVisualButtons();
      document.addEventListener("keydown", (event) => this.handleKey(event));
    }

    createSketch() {
      this.p5 = new p5((p) => {
        p.setup = () => {
          this.canvas = p.createCanvas(this.width, this.height);
          this.canvas.parent(this.mount);
          p.frameRate(60);

          this.cameraElement = p.createImg(this.streamUrl, "ESP32S3 stream");
          this.cameraElement.hide();
          this.cameraElement.elt.crossOrigin = "anonymous";
          this.cameraElement.elt.addEventListener("load", () => this.updateStatus("Streaming"));
          this.cameraElement.elt.addEventListener("error", () => this.updateStatus("Stream error"));

          this.setVisual(0);
        };

        p.draw = () => {
          if (this.paused) {
            return;
          }
          const now = performance.now();
          if (this._lastDraw) {
            const delta = now - this._lastDraw;
            this.currentFPS = delta > 0 ? 1000 / delta : 0;
          }
          this._lastDraw = now;

          this.drawFrame(p);
        };
      }, this.mount);
    }

    drawFrame(p) {
      p.push();
      p.noStroke();
      p.fill(3, 7, 18, 180);
      p.rect(0, 0, this.width, this.height);
      p.pop();

      const img = this.cameraElement?.elt;
      const visual = this.activeVisual;
      if (visual && typeof visual.draw === "function") {
        visual.draw({
          p,
          engine: this,
          fps: this.currentFPS,
          elapsed: p.millis() / 1000,
          streamActive: !!(img && img.complete && img.naturalWidth > 0),
        });
      } else {
        p.push();
        p.fill(200);
        p.textAlign(p.CENTER, p.CENTER);
        p.textSize(16);
        p.text("No visual registered", this.width / 2, this.height / 2);
        p.pop();
      }
    }

    drawCamera(p, options = {}) {
      const { mirror = false, opacity = 255 } = options;
      const img = this.cameraElement?.elt;
      if (!img || !img.complete || img.naturalWidth === 0) {
        return;
      }
      const ctx = p.drawingContext;
      ctx.save();
      const alpha = Math.min(Math.max(opacity / 255, 0), 1);
      ctx.globalAlpha = alpha;
      if (mirror) {
        ctx.translate(this.width, 0);
        ctx.scale(-1, 1);
      }
      ctx.drawImage(img, 0, 0, this.width, this.height);
      ctx.restore();
    }

    renderVisualButtons() {
      if (!this.visualButtonsContainer) {
        return;
      }
      this.visualButtonsContainer.innerHTML = "";
      this.visuals.forEach((visual, index) => {
        const button = document.createElement("button");
        button.textContent = `${index + 1}. ${visual.name}`;
        if (index === this.activeVisualIndex) {
          button.classList.add("active");
        }
        button.addEventListener("click", () => this.setVisual(index));
        this.visualButtonsContainer.appendChild(button);
      });
    }

    setVisual(index) {
      if (!this.visuals.length) {
        this.updateStatus("No visuals registered");
        return;
      }
      const clampedIndex = Math.max(0, Math.min(index, this.visuals.length - 1));
      if (this.activeVisual && typeof this.activeVisual.dispose === "function") {
        this.activeVisual.dispose({ engine: this, p: this.p5 });
      }
      this.activeVisualIndex = clampedIndex;
      this.activeVisual = this.visuals[clampedIndex];
      if (typeof this.activeVisual.init === "function") {
        this.activeVisual.init({ engine: this, p: this.p5 });
      }
      this.updateVisualLabel();
      this.highlightActiveButton();
    }

    updateVisualLabel() {
      if (!this.visualLabel) {
        return;
      }
      const visual = this.visuals[this.activeVisualIndex];
      this.visualLabel.textContent = visual ? visual.name : "None";
    }

    highlightActiveButton() {
      if (!this.visualButtonsContainer) {
        return;
      }
      [...this.visualButtonsContainer.children].forEach((button, idx) => {
        button.classList.toggle("active", idx === this.activeVisualIndex);
      });
    }

    updateStream(url) {
      this.streamUrl = url;
      if (this.cameraElement?.elt) {
        const token = Date.now();
        this.cameraElement.elt.src = `${url}?_=${token}`;
      }
      this.updateStatus("Connecting...");
    }

    updateStatus(message) {
      if (this.statusLabel) {
        this.statusLabel.textContent = message;
      }
    }

    handleKey(event) {
      if (event.key >= "1" && event.key <= "9") {
        const target = Number(event.key) - 1;
        if (target < this.visuals.length) {
          this.setVisual(target);
          event.preventDefault();
          return;
        }
      }

      if (event.code === "Space") {
        this.togglePause();
        event.preventDefault();
        return;
      }

      if (event.key.toLowerCase() === "s") {
        this.saveFrame();
      }
    }

    togglePause() {
      this.paused = !this.paused;
      this.updateStatus(this.paused ? "Paused" : "Streaming");
    }

    saveFrame() {
      if (!this.p5) {
        return;
      }
      const filename = `mass60-visual-${Date.now()}`;
      this.p5.saveCanvas(filename, "png");
    }
  }

  window.VisualEngine = VisualEngine;
})();
