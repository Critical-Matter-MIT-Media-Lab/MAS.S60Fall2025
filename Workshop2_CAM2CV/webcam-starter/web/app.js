// app.js
// Boots the MASS60 visual engine once the DOM is ready and wires up UI elements.
document.addEventListener("DOMContentLoaded", () => {
  const mount = document.getElementById("canvas-wrapper");
  const statusLabel = document.getElementById("stream-status");
  const visualLabel = document.getElementById("visual-label");
  const visualButtons = document.getElementById("visual-buttons");
  const streamUrlInput = document.getElementById("stream-url");
  const applyButton = document.getElementById("apply-stream");

  if (!mount) {
    throw new Error("Canvas wrapper not found");
  }

  const engine = new VisualEngine({
    mount,
    statusLabel,
    visualLabel,
    visualButtonsContainer: visualButtons,
    streamUrlInput,
    applyButton,
    visuals: window.VISUALS,
    streamUrl: streamUrlInput ? streamUrlInput.value : undefined,
    width: 640,
    height: 480,
  });

  window.mass60Engine = engine;
});
