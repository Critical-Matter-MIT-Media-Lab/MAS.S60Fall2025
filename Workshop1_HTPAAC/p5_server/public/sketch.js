// WebSocket connection
let socket;
let serverConnected = false;

// Network configuration - Modified for 5 sensors
const NUM_NODES = 5;
let nodes = [];
let particles = [];
let connections = [];
let backgroundParticles = [];

// Sensor data
let sensorData = Array(NUM_NODES)
  .fill(null)
  .map((_, i) => ({
    id: i,
    connected: false,
    value: 0,
    lastUpdate: null,
    color: null,
  }));

// Visualization settings
const NODE_SIZE = 10;
const PARTICLE_EMIT_RATE = 0.2;
const MAX_PARTICLES = 600;
const ATTRACTION_FORCE = 0.025;
const REPULSION_FORCE = 0.6;
const MAX_SPEED = 8;

// Define unique colors for 5 sensors
const NODE_COLORS = [
  [255, 20, 147], // Deep Pink
  [0, 255, 255], // Cyan
  [255, 165, 0], // Orange
  [50, 255, 50], // Lime Green
  [147, 112, 219], // Medium Purple
];

// Node class
class Node {
  constructor(id, x, y) {
    this.id = id;
    this.x = x;
    this.y = y;
    this.connected = false;
    this.dataValue = 0;
    this.phase = random(TWO_PI);
    this.orbitRadius = 0;
    this.targetOrbitRadius = 0;
    this.rotation = 0;
    this.energy = 0;
    this.color = NODE_COLORS[id];
    this.pulseIntensity = 0;
    this.targetPulseIntensity = 0;
  }

  updateData(value, connected) {
    this.connected = connected;
    this.dataValue = value;

    // Adjust pulse intensity based on data value
    if (connected && value > 0) {
      this.targetPulseIntensity = map(value, 0, 1023, 0.3, 1.0); // Assume sensor value range 0-1023
    } else {
      this.targetPulseIntensity = 0;
    }
  }

  update() {
    this.phase += 0.02 + this.pulseIntensity * 0.03;
    this.rotation += 0.01 + this.pulseIntensity * 0.02;

    // Smooth transition of pulse intensity
    this.pulseIntensity = lerp(
      this.pulseIntensity,
      this.targetPulseIntensity,
      0.1
    );

    if (this.connected) {
      this.targetOrbitRadius =
        30 + sin(this.phase) * (15 + this.pulseIntensity * 20);
      this.energy = min(this.energy + 0.08, 1);
    } else {
      this.targetOrbitRadius = 0;
      this.energy = max(this.energy - 0.05, 0);
    }

    this.orbitRadius = lerp(this.orbitRadius, this.targetOrbitRadius, 0.15);
  }

  draw() {
    push();
    translate(this.x, this.y);
    rotate(this.rotation);

    // Abstract orbital rings
    if (this.energy > 0) {
      noFill();
      for (let i = 0; i < 4; i++) {
        let radius = this.orbitRadius * (1 + i * 0.6);
        let alpha =
          this.energy * (120 + this.pulseIntensity * 60) * (1 - i * 0.25);
        stroke(this.color[0], this.color[1], this.color[2], alpha);
        strokeWeight(0.8 + this.pulseIntensity * 0.5);

        let arcStart = this.phase + (i * PI) / 4;
        let arcEnd = arcStart + PI * (1.8 - i * 0.2);
        arc(0, 0, radius, radius, arcStart, arcEnd);
      }
    }

    // Central point with glow
    if (this.energy > 0) {
      // Glow layers
      for (let i = 6; i > 0; i--) {
        let alpha = this.energy * (25 + this.pulseIntensity * 15) * (1 / i);
        fill(this.color[0], this.color[1], this.color[2], alpha);
        noStroke();
        circle(0, 0, NODE_SIZE * i * (this.energy + this.pulseIntensity * 0.5));
      }
    }

    // Core
    let coreAlpha = 180 + this.energy * 75 + this.pulseIntensity * 50;
    fill(this.color[0], this.color[1], this.color[2], coreAlpha);
    noStroke();
    circle(0, 0, NODE_SIZE * (1 + this.pulseIntensity * 0.3));

    // Data value display (small dots)
    if (this.connected && this.dataValue > 0) {
      let dataIntensity = map(this.dataValue, 0, 1023, 0, 1);
      for (let i = 0; i < 6; i++) {
        let angle = this.phase * 3 + (TWO_PI / 6) * i;
        let dist = this.orbitRadius * (0.8 + dataIntensity * 0.4);
        let px = cos(angle) * dist;
        let py = sin(angle) * dist;

        fill(255, 255, 255, 120 + dataIntensity * 135);
        noStroke();
        circle(px, py, 2 + dataIntensity * 3);
      }
    }

    pop();
  }
}

// Data-driven particle class
class DataParticle {
  constructor(startNode, endNode, intensity = 1) {
    this.startNode = startNode;
    this.endNode = endNode;
    this.x = startNode.x + random(-15, 15);
    this.y = startNode.y + random(-15, 15);

    // Initial velocity based on data intensity
    let angle = atan2(endNode.y - startNode.y, endNode.x - startNode.x);
    let speed = random(2, 5) * intensity;
    this.vx = cos(angle + random(-0.6, 0.6)) * speed;
    this.vy = sin(angle + random(-0.6, 0.6)) * speed;

    this.size = random(1.5, 5) * intensity;
    this.mass = this.size;
    this.trail = [];
    this.maxTrailLength = Math.floor(15 + intensity * 10);
    this.lifespan = 1;
    this.maxLifetime = random(120, 250);
    this.lifetime = 0;
    this.intensity = intensity;

    // Blend colors between start and end nodes
    this.color = lerpColor(
      color(startNode.color[0], startNode.color[1], startNode.color[2]),
      color(endNode.color[0], endNode.color[1], endNode.color[2]),
      random(0.2, 0.8)
    );

    this.sourceNode = startNode.id;
  }

  applyForce(fx, fy) {
    this.vx += fx / this.mass;
    this.vy += fy / this.mass;
  }

  update() {
    this.lifetime++;

    // Calculate data activity
    let connectedCount = nodes.filter((n) => n.connected).length;
    let avgDataIntensity =
      nodes.reduce((sum, n) => sum + n.pulseIntensity, 0) / NUM_NODES;
    let chaosFactor =
      (connectedCount / NUM_NODES) *
      avgDataIntensity *
      (particles.length / MAX_PARTICLES);

    // Apply inter-particle forces
    particles.forEach((other) => {
      if (other !== this) {
        let dx = other.x - this.x;
        let dy = other.y - this.y;
        let distance = sqrt(dx * dx + dy * dy);

        if (distance > 0 && distance < 250) {
          dx /= distance;
          dy /= distance;

          if (distance < 25) {
            // Strong repulsion at close range
            let repulsion =
              REPULSION_FORCE * (1 - distance / 25) * this.intensity;
            this.applyForce(-dx * repulsion, -dy * repulsion);
          } else if (distance < 120) {
            // Medium range attraction
            let attraction =
              ATTRACTION_FORCE * (1 + chaosFactor * 4) * this.intensity;

            // Stronger attraction for same-source particles
            if (this.sourceNode === other.sourceNode) {
              attraction *= 1.8;
            }

            attraction *= other.mass / ((distance * distance) / 1200);
            this.applyForce(dx * attraction, dy * attraction);
          }
        }
      }
    });

    // Target node attraction
    let targetDx = this.endNode.x - this.x;
    let targetDy = this.endNode.y - this.y;
    let targetDist = sqrt(targetDx * targetDx + targetDy * targetDy);
    if (targetDist > 25) {
      this.applyForce(
        targetDx * 0.002 * this.intensity,
        targetDy * 0.002 * this.intensity
      );
    }

    // Damping and speed limit
    this.vx *= 0.985;
    this.vy *= 0.985;

    let speed = sqrt(this.vx * this.vx + this.vy * this.vy);
    if (speed > MAX_SPEED) {
      this.vx = (this.vx / speed) * MAX_SPEED;
      this.vy = (this.vy / speed) * MAX_SPEED;
    }

    this.x += this.vx;
    this.y += this.vy;

    // Boundary handling
    let edgeDistance = 60;
    if (this.x < edgeDistance) {
      this.vx += (edgeDistance - this.x) * 0.015;
    } else if (this.x > width - edgeDistance) {
      this.vx -= (this.x - (width - edgeDistance)) * 0.015;
    }
    if (this.y < edgeDistance) {
      this.vy += (edgeDistance - this.y) * 0.015;
    } else if (this.y > height - edgeDistance) {
      this.vy -= (this.y - (height - edgeDistance)) * 0.015;
    }

    // Trail recording
    this.trail.push({ x: this.x, y: this.y, size: this.size });
    if (this.trail.length > this.maxTrailLength) {
      this.trail.shift();
    }

    // Lifecycle
    if (this.lifetime > this.maxLifetime * 0.75) {
      this.lifespan = map(
        this.lifetime,
        this.maxLifetime * 0.75,
        this.maxLifetime,
        1,
        0
      );
    }
  }

  draw() {
    let r = red(this.color);
    let g = green(this.color);
    let b = blue(this.color);

    // Draw trail
    noFill();
    for (let i = 1; i < this.trail.length; i++) {
      let alpha =
        map(i, 0, this.trail.length, 0, 100) * this.lifespan * this.intensity;
      let size = map(i, 0, this.trail.length, 0.8, this.size);

      stroke(r, g, b, alpha);
      strokeWeight(size);
      line(
        this.trail[i - 1].x,
        this.trail[i - 1].y,
        this.trail[i].x,
        this.trail[i].y
      );
    }

    // Draw particle core
    push();
    translate(this.x, this.y);

    // Soft glow
    for (let i = 4; i > 0; i--) {
      let alpha = 40 * this.lifespan * this.intensity * (1 / i);
      fill(r, g, b, alpha);
      noStroke();
      circle(0, 0, this.size * i * 1.8);
    }

    // Bright core
    fill(255, 255, 255, 255 * this.lifespan);
    noStroke();
    circle(0, 0, this.size * 0.9);

    pop();
  }

  isDead() {
    return this.lifetime > this.maxLifetime;
  }
}

// Connection visualization class
class Connection {
  constructor(node1, node2) {
    this.node1 = node1;
    this.node2 = node2;
    this.strength = 0;
    this.phase = random(TWO_PI);
    this.dataFlow = 0;
  }

  update() {
    this.phase += 0.04;

    // Based on connection status and data intensity of both nodes
    let targetStrength = 0;
    if (this.node1.connected && this.node2.connected) {
      let avgIntensity =
        (this.node1.pulseIntensity + this.node2.pulseIntensity) / 2;
      targetStrength = 0.3 + avgIntensity * 0.7;
    }

    this.strength = lerp(this.strength, targetStrength, 0.05);

    // Data flow effect
    this.dataFlow = (this.node1.pulseIntensity + this.node2.pulseIntensity) / 2;
  }

  draw() {
    if (this.strength > 0.05) {
      push();

      // Blend colors of both nodes
      let r = lerp(this.node1.color[0], this.node2.color[0], 0.5);
      let g = lerp(this.node1.color[1], this.node2.color[1], 0.5);
      let b = lerp(this.node1.color[2], this.node2.color[2], 0.5);

      // Create gradient connection lines
      for (let i = 0; i < 4; i++) {
        stroke(r, g, b, 20 * this.strength * (1 - i * 0.25));
        strokeWeight((4 - i) * 0.8 * this.strength);

        beginShape();
        noFill();

        let steps = 25;
        for (let j = 0; j <= steps; j++) {
          let t = j / steps;
          let x = lerp(this.node1.x, this.node2.x, t);
          let y = lerp(this.node1.y, this.node2.y, t);

          // Wave effect based on data flow
          let wave =
            sin(t * PI * 3 + this.phase + i * 0.8) *
            12 *
            this.strength *
            (1 + this.dataFlow);
          let perpAngle =
            atan2(this.node2.y - this.node1.y, this.node2.x - this.node1.x) +
            HALF_PI;
          x += cos(perpAngle) * wave;
          y += sin(perpAngle) * wave;

          curveVertex(x, y);
        }

        endShape();
      }

      pop();
    }
  }
}

// Background ambient particles
class AmbientParticle {
  constructor() {
    this.x = random(width);
    this.y = random(height);
    this.size = random(0.8, 2.5);
    this.speedX = random(-0.3, 0.3);
    this.speedY = random(-0.3, 0.3);
    this.alpha = random(8, 25);
    this.color = random(NODE_COLORS);
    this.phase = random(TWO_PI);
  }

  update() {
    this.phase += 0.02;
    this.x += this.speedX;
    this.y += this.speedY;

    // Boundary wrapping
    if (this.x < 0) this.x = width;
    if (this.x > width) this.x = 0;
    if (this.y < 0) this.y = height;
    if (this.y > height) this.y = 0;

    // Subtle flickering
    this.alpha = this.alpha + sin(this.phase) * 5;
  }

  draw() {
    fill(this.color[0], this.color[1], this.color[2], this.alpha);
    noStroke();
    circle(this.x, this.y, this.size);
  }
}

function setup() {
  createCanvas(windowWidth, windowHeight);

  // Initialize WebSocket connection
  initializeSocket();

  // Initialize node positions (5 sensors)
  let centerX = width / 2;
  let centerY = height / 2 - 50;
  let radius = min(width, height) * 0.25;

  for (let i = 0; i < NUM_NODES; i++) {
    let angle = (i / NUM_NODES) * TWO_PI - PI / 2;
    let radiusVariation = radius + random(-25, 25);
    let x = centerX + cos(angle) * radiusVariation;
    let y = centerY + sin(angle) * radiusVariation;
    nodes.push(new Node(i, x, y));
  }

  // Create all possible connections
  for (let i = 0; i < NUM_NODES; i++) {
    for (let j = i + 1; j < NUM_NODES; j++) {
      connections.push(new Connection(nodes[i], nodes[j]));
    }
  }

  // Create background ambient particles
  for (let i = 0; i < 60; i++) {
    backgroundParticles.push(new AmbientParticle());
  }

  // Setup button event listeners for simulation
  setupButtonListeners();

  console.log("Visualization system initialized");
  console.log("💡 Click sensor buttons for simulation/debugging");
}

function draw() {
  // Dark background
  background(5, 2, 10);

  // Draw background particles
  backgroundParticles.forEach((p) => {
    p.update();
    p.draw();
  });

  // Draw connections
  connections.forEach((conn) => {
    conn.update();
    conn.draw();
  });

  // Emit particles based on sensor data
  let connectedNodes = nodes.filter((n) => n.connected);
  if (connectedNodes.length >= 2 && particles.length < MAX_PARTICLES) {
    // Emission rate based on data activity
    let totalIntensity = connectedNodes.reduce(
      (sum, n) => sum + n.pulseIntensity,
      0
    );
    let emitCount = Math.floor(PARTICLE_EMIT_RATE * totalIntensity * 15);

    for (let i = 0; i < emitCount; i++) {
      if (random() < 0.85) {
        let start = random(connectedNodes);
        let end = random(connectedNodes.filter((n) => n !== start));
        if (end && start.pulseIntensity > 0.1) {
          let intensity = (start.pulseIntensity + end.pulseIntensity) / 2;
          particles.push(new DataParticle(start, end, intensity));
        }
      }
    }
  }

  // Update and draw data particles
  particles = particles.filter((p) => !p.isDead());
  particles.forEach((p) => {
    p.update();
    p.draw();
  });

  // Draw sensor nodes
  nodes.forEach((node) => {
    node.update();
    node.draw();
  });

  // Update UI display
  if (frameCount % 15 === 0) {
    updateUI();
  }
}

function initializeSocket() {
  socket = io();

  socket.on("connect", () => {
    console.log("Connected to server");
    serverConnected = true;
    document.getElementById("server-status").textContent = "Connected";
    document.getElementById("server-status").className =
      "info-value status-connected";

    // Register as visualization client
    socket.emit("register_client", {
      clientType: "visualizer",
    });
  });

  socket.on("disconnect", () => {
    console.log("Disconnected from server");
    serverConnected = false;
    document.getElementById("server-status").textContent = "Disconnected";
    document.getElementById("server-status").className =
      "info-value status-disconnected";
  });

  // Receive initial data
  socket.on("initial_data", (data) => {
    console.log("Received initial data:", data);
    updateSensorData(data.clients);
  });

  // Receive data updates
  socket.on("data_update", (data) => {
    const { clientId, value, timestamp } = data;
    console.log(`Sensor ${clientId} data update: ${value}`);

    if (clientId < NUM_NODES) {
      sensorData[clientId].value = value;
      sensorData[clientId].lastUpdate = timestamp;

      // Update corresponding node
      let normalizedValue = map(constrain(value, 0, 1023), 0, 1023, 0, 1);
      nodes[clientId].updateData(value, sensorData[clientId].connected);
    }
  });

  // Receive client status updates
  socket.on("client_status_update", (data) => {
    const { clientId, connected, timestamp } = data;
    console.log(`Sensor ${clientId} connection status: ${connected}`);

    if (clientId < NUM_NODES) {
      sensorData[clientId].connected = connected;
      sensorData[clientId].lastUpdate = timestamp;

      // Update node status
      nodes[clientId].updateData(sensorData[clientId].value, connected);

      // Update UI button status
      updateNodeButton(clientId, connected);
    }
  });

  socket.on("connect_error", (error) => {
    console.error("Connection error:", error);
    document.getElementById("server-status").textContent = "Connection Failed";
    document.getElementById("server-status").className =
      "info-value status-disconnected";
  });
}

function updateSensorData(clients) {
  clients.forEach((client, index) => {
    if (index < NUM_NODES) {
      sensorData[index] = {
        id: index,
        connected: client.connected,
        value: client.data,
        lastUpdate: client.lastUpdate,
        color: NODE_COLORS[index],
      };

      // Update node
      let normalizedValue = map(constrain(client.data, 0, 1023), 0, 1023, 0, 1);
      nodes[index].updateData(client.data, client.connected);

      // Update UI
      updateNodeButton(index, client.connected);
    }
  });
}

function updateNodeButton(nodeId, connected) {
  const button = document.querySelector(`[data-node="${nodeId}"]`);
  if (button) {
    if (connected) {
      button.classList.add("connected");
    } else {
      button.classList.remove("connected");
    }

    // Add simulation indicator
    if (simulationMode[nodeId]) {
      button.classList.add("simulated");
    } else {
      button.classList.remove("simulated");
    }
  }
}

function updateUI() {
  // Update active sensor count
  let activeCount = nodes.filter((n) => n.connected).length;
  document.getElementById("active-count").textContent = activeCount;

  // Update particle count
  document.getElementById("particle-count").textContent = particles.length;

  // Calculate data activity
  let avgIntensity =
    nodes.reduce((sum, n) => sum + n.pulseIntensity, 0) / NUM_NODES;
  let chaosLevel = Math.round(
    avgIntensity *
      (activeCount / NUM_NODES) *
      (particles.length / MAX_PARTICLES) *
      100
  );
  document.getElementById("chaos-level").textContent = chaosLevel;
}

// Simulation state for debugging
let simulationMode = Array(NUM_NODES).fill(false);
let simulatedValues = Array(NUM_NODES).fill(0);

function setupButtonListeners() {
  // Add click listeners to sensor buttons
  document.querySelectorAll(".node-btn").forEach((btn, index) => {
    btn.addEventListener("click", function () {
      const nodeId = parseInt(this.dataset.node);
      toggleSimulation(nodeId);
    });
  });
}

function toggleSimulation(nodeId) {
  if (nodeId >= 0 && nodeId < NUM_NODES) {
    simulationMode[nodeId] = !simulationMode[nodeId];

    if (simulationMode[nodeId]) {
      // Turn on simulation for this node
      console.log(`🟢 Simulation ON for sensor ${nodeId + 1}`);

      // Set initial simulated value
      simulatedValues[nodeId] = random(300, 800);

      // Update node state
      nodes[nodeId].updateData(simulatedValues[nodeId], true);
      sensorData[nodeId].connected = true;
      sensorData[nodeId].value = simulatedValues[nodeId];
      sensorData[nodeId].lastUpdate = new Date();

      // Update button visual state
      updateNodeButton(nodeId, true);

      // Start simulation data generation
      startNodeSimulation(nodeId);
    } else {
      // Turn off simulation for this node
      console.log(`🔴 Simulation OFF for sensor ${nodeId + 1}`);

      // Update node state
      nodes[nodeId].updateData(0, false);
      sensorData[nodeId].connected = false;
      sensorData[nodeId].value = 0;

      // Update button visual state
      updateNodeButton(nodeId, false);

      // Stop simulation
      stopNodeSimulation(nodeId);
    }
  }
}

// Store simulation intervals
let simulationIntervals = {};

function startNodeSimulation(nodeId) {
  // Clear existing interval if any
  if (simulationIntervals[nodeId]) {
    clearInterval(simulationIntervals[nodeId]);
  }

  // Create different simulation patterns for each sensor
  simulationIntervals[nodeId] = setInterval(() => {
    if (simulationMode[nodeId]) {
      let newValue;
      const time = millis() * 0.001;

      // Different patterns for each sensor
      switch (nodeId) {
        case 0: // Sine wave
          newValue = 500 + 300 * sin(time * 0.5);
          break;
        case 1: // Random walk
          simulatedValues[nodeId] += random(-50, 50);
          newValue = constrain(simulatedValues[nodeId], 100, 900);
          break;
        case 2: // Pulse pattern
          newValue = random() > 0.8 ? random(700, 900) : random(200, 400);
          break;
        case 3: // Sawtooth
          newValue = ((time * 100) % 600) + 200;
          break;
        case 4: // Heartbeat pattern
          let heartbeat = sin(time * 2) > 0.7 ? 800 : 400;
          newValue = heartbeat + random(-50, 50);
          break;
        default:
          newValue = random(200, 800);
      }

      simulatedValues[nodeId] = newValue;

      // Update node
      nodes[nodeId].updateData(newValue, true);
      sensorData[nodeId].value = newValue;
      sensorData[nodeId].lastUpdate = new Date();

      console.log(`📊 Simulated sensor ${nodeId + 1}: ${newValue.toFixed(1)}`);
    }
  }, 200 + random(100)); // Vary interval slightly
}

function stopNodeSimulation(nodeId) {
  if (simulationIntervals[nodeId]) {
    clearInterval(simulationIntervals[nodeId]);
    delete simulationIntervals[nodeId];
  }
}

function windowResized() {
  resizeCanvas(windowWidth, windowHeight);

  // Recalculate node positions
  let centerX = width / 2;
  let centerY = height / 2 - 50;
  let radius = min(width, height) * 0.25;

  for (let i = 0; i < NUM_NODES; i++) {
    let angle = (i / NUM_NODES) * TWO_PI - PI / 2;
    let radiusVariation = radius + random(-25, 25);
    nodes[i].x = centerX + cos(angle) * radiusVariation;
    nodes[i].y = centerY + sin(angle) * radiusVariation;
  }
}

// Keyboard shortcuts for quick testing
function keyPressed() {
  // Press number keys 1-5 to toggle simulation for each sensor
  if (key >= "1" && key <= "5") {
    const nodeId = parseInt(key) - 1;
    toggleSimulation(nodeId);
  }

  // Press 'A' to turn on all simulations
  if (key === "a" || key === "A") {
    console.log("🟢 Turning ON all simulations");
    for (let i = 0; i < NUM_NODES; i++) {
      if (!simulationMode[i]) {
        toggleSimulation(i);
      }
    }
  }

  // Press 'X' to turn off all simulations
  if (key === "x" || key === "X") {
    console.log("🔴 Turning OFF all simulations");
    for (let i = 0; i < NUM_NODES; i++) {
      if (simulationMode[i]) {
        toggleSimulation(i);
      }
    }
  }
}
