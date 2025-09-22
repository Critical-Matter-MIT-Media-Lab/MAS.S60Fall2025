const express = require("express");
const http = require("http");
const socketIo = require("socket.io");
const WebSocket = require("ws");
const path = require("path");
const cors = require("cors");

const app = express();
const server = http.createServer(app);

// Socket.IO server for visualization client
const io = socketIo(server, {
  cors: {
    origin: "*",
    methods: ["GET", "POST"],
  },
});

// WebSocket server for sensor clients
const wss = new WebSocket.Server({
  server: server,
  path: "/ws",
});

// Middleware
app.use(cors());
app.use(express.static(path.join(__dirname, "public")));
app.use(express.json());

// Client connection state management
const socketIOClients = new Map(); // For visualization clients
const webSocketClients = new Map(); // For sensor clients
const MAX_CLIENTS = 5;

// Data storage
let sensorData = {
  clients: Array(MAX_CLIENTS)
    .fill(null)
    .map((_, i) => ({
      id: i,
      connected: false,
      data: 0,
      lastUpdate: null,
      color: getNodeColor(i),
    })),
};

// Get node color
function getNodeColor(nodeId) {
  const colors = [
    [255, 20, 147], // Deep Pink
    [0, 255, 255], // Cyan
    [255, 165, 0], // Orange
    [50, 255, 50], // Lime Green
    [147, 112, 219], // Medium Purple
  ];
  return colors[nodeId] || [255, 255, 255];
}

// WebSocket server handling (for sensor clients)
wss.on("connection", (ws, req) => {
  console.log("New WebSocket client connected from:", req.socket.remoteAddress);

  let clientId = null;
  let clientType = null;

  ws.on("message", (message) => {
    try {
      const data = JSON.parse(message.toString());

      if (data.type === "register") {
        clientId = data.nodeId;
        clientType = data.clientType;

        if (
          clientType === "gsr_node" &&
          clientId >= 0 &&
          clientId < MAX_CLIENTS
        ) {
          webSocketClients.set(ws, { clientId, type: "gsr_node" });
          sensorData.clients[clientId].connected = true;
          sensorData.clients[clientId].lastUpdate = new Date();

          console.log(
            `GSR sensor node ${clientId} (Group ${data.groupId}) registered via WebSocket`
          );

          // Send acknowledgment
          ws.send(
            JSON.stringify({
              type: "register_ack",
              nodeId: clientId,
              message: "Registration successful",
            })
          );

          // Broadcast connection status to visualization clients
          io.emit("client_status_update", {
            clientId,
            connected: true,
            timestamp: new Date(),
          });
        }
      } else if (data.type === "gsr_data" && clientId !== null) {
        // Update sensor data
        sensorData.clients[clientId].data = data.ema || data.value || 0;
        sensorData.clients[clientId].lastUpdate = new Date();

        console.log(
          `Received GSR data from node ${clientId}: ${data.ema || data.value}`
        );

        // Send server acknowledgment
        ws.send(
          JSON.stringify({
            type: "server_ack",
            nodeId: clientId,
            timestamp: Date.now(),
          })
        );

        // Broadcast data update to visualization clients
        io.emit("data_update", {
          clientId,
          value: data.ema || data.value || 0,
          timestamp: sensorData.clients[clientId].lastUpdate,
        });
      }
    } catch (error) {
      console.error("WebSocket message parse error:", error);
    }
  });

  ws.on("close", () => {
    if (clientId !== null && clientType === "gsr_node") {
      sensorData.clients[clientId].connected = false;

      console.log(`GSR sensor node ${clientId} disconnected via WebSocket`);

      // Broadcast disconnection status to visualization clients
      io.emit("client_status_update", {
        clientId,
        connected: false,
        timestamp: new Date(),
      });
    }

    webSocketClients.delete(ws);
    console.log("WebSocket client disconnected");
  });

  ws.on("error", (error) => {
    console.error("WebSocket error:", error);
  });
});

// Socket.IO handling (for visualization clients)
io.on("connection", (socket) => {
  console.log("New Socket.IO client connected:", socket.id);

  // Client registration for visualization
  socket.on("register_client", (data) => {
    const { clientType } = data;

    if (clientType === "visualizer") {
      socketIOClients.set(socket.id, { type: "visualizer" });

      // Send current state to visualization client
      socket.emit("initial_data", sensorData);
      console.log("Visualization client connected");
    }
  });

  // Client disconnection
  socket.on("disconnect", () => {
    socketIOClients.delete(socket.id);
    console.log("Socket.IO client disconnected:", socket.id);
  });

  // Heartbeat detection
  socket.on("ping", (callback) => {
    if (callback) callback("pong");
  });
});

// REST API endpoints
app.get("/api/status", (req, res) => {
  res.json({
    connectedClients: socketIOClients.size + webSocketClients.size,
    sensorClients: sensorData.clients.filter((c) => c.connected).length,
    webSocketClients: webSocketClients.size,
    socketIOClients: socketIOClients.size,
    data: sensorData,
  });
});

// Home page route
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "index.html"));
});

// Client template route
app.get("/client", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "client-template.html"));
});

// Connection test route
app.get("/test", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "connection-test.html"));
});

// Fixed client route
app.get("/fixed-client", (req, res) => {
  res.sendFile(path.join(__dirname, "fixed-client.html"));
});

// Get network interfaces
const os = require("os");

function getNetworkIPs() {
  const interfaces = os.networkInterfaces();
  const ips = [];

  Object.keys(interfaces).forEach((ifname) => {
    interfaces[ifname].forEach((iface) => {
      // Skip internal (loopback) and non-IPv4 addresses
      if (iface.family !== "IPv4" || iface.internal !== false) {
        return;
      }
      ips.push(iface.address);
    });
  });

  return ips;
}

// Start server
const PORT = process.env.PORT || 3000;
server.listen(PORT, "0.0.0.0", () => {
  const networkIPs = getNetworkIPs();

  console.log(`\n🚀 Server running on port ${PORT}`);
  console.log(`📱 Visualization interface:`);
  console.log(`   Local:    http://localhost:${PORT}`);

  networkIPs.forEach((ip) => {
    console.log(`   Network:  http://${ip}:${PORT}`);
  });

  console.log(`\n🔌 WebSocket endpoints for sensor clients:`);
  console.log(`   Local:    ws://localhost:${PORT}/ws`);

  networkIPs.forEach((ip) => {
    console.log(`   Network:  ws://${ip}:${PORT}/ws`);
  });

  console.log(`\n📊 API status: http://localhost:${PORT}/api/status`);
  console.log(
    `\n💡 For clients on different networks, use the Network URLs above`
  );
});

// Periodic cleanup of disconnected client data
setInterval(() => {
  const now = new Date();
  sensorData.clients.forEach((client, index) => {
    if (client.connected && client.lastUpdate) {
      const timeDiff = now - new Date(client.lastUpdate);
      if (timeDiff > 30000) {
        // Consider disconnected if no data for 30 seconds
        client.connected = false;
        io.emit("client_status_update", {
          clientId: index,
          connected: false,
          timestamp: now,
        });
      }
    }
  });
}, 10000); // Check every 10 seconds
