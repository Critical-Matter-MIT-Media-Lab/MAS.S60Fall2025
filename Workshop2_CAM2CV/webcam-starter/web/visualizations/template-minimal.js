// template-minimal.js
// Baseline visual that mirrors the camera feed, overlays stream status, and draws
// a simple bounding frame. Serves as a starting point for new visuals.
(function () {
  window.VISUALS = window.VISUALS || [];

  window.VISUALS.push({
    id: "minimal",
    name: "Minimal HUD",
    description: "Draws the camera feed with a simple status overlay.",
    init: ({ engine }) => {
      console.log("[visual] Minimal HUD ready", engine.streamUrl);
    },
    draw: ({ engine, p, fps, streamActive }) => {
      engine.drawCamera(p, { mirror: true });

      p.push();
      p.noStroke();
      p.fill(10, 15, 35, 180);
      p.rect(0, 0, engine.width, 54, 0, 0, 16, 16);
      p.fill(255);
      p.textSize(18);
      p.textFont("Segoe UI, sans-serif");
      p.textAlign(p.LEFT, p.CENTER);
      p.text(`Stream: ${streamActive ? "online" : "offline"}`, 18, 20);
      p.text(`FPS: ${fps.toFixed(1)}`, 18, 40);
      p.pop();

      p.push();
      p.noFill();
      p.stroke(168, 85, 247);
      p.strokeWeight(2);
      p.rect(24, 72, engine.width - 48, engine.height - 120, 18);
      p.pop();
    },
  });
})();
