// Test client - Simulate sensor data transmission
const io = require("socket.io-client");

class SensorClient {
  constructor(clientId, serverUrl = "http://localhost:3000") {
    this.clientId = clientId;
    this.socket = io(serverUrl);
    this.connected = false;
    this.dataInterval = null;

    this.setupEventHandlers();
  }

  setupEventHandlers() {
    this.socket.on("connect", () => {
      console.log(`Sensor client ${this.clientId} connected to server`);
      this.connected = true;

      // Register as sensor client
      this.socket.emit("register_client", {
        clientId: this.clientId,
        clientType: "sensor",
      });

      // Start sending mock data
      this.startSendingData();
    });

    this.socket.on("disconnect", () => {
      console.log(`Sensor client ${this.clientId} disconnected`);
      this.connected = false;
      this.stopSendingData();
    });

    this.socket.on("connect_error", (error) => {
      console.error(
        `Sensor client ${this.clientId} connection error:`,
        error.message
      );
    });
  }

  startSendingData() {
    if (this.dataInterval) {
      clearInterval(this.dataInterval);
    }

    // Send data every 200-800ms (simulate different sensor frequencies)
    const interval = 200 + Math.random() * 600;

    this.dataInterval = setInterval(() => {
      if (this.connected) {
        // Generate mock sensor data
        // Different data patterns can be generated for different sensor types
        const sensorValue = this.generateSensorData();

        this.socket.emit("sensor_data", {
          value: sensorValue,
          timestamp: new Date().toISOString(),
        });

        console.log(`Sensor ${this.clientId}: ${sensorValue}`);
      }
    }, interval);
  }

  stopSendingData() {
    if (this.dataInterval) {
      clearInterval(this.dataInterval);
      this.dataInterval = null;
    }
  }

  generateSensorData() {
    const now = Date.now();

    // Generate different data patterns for different sensors
    switch (this.clientId) {
      case 0: // Sensor 1 - Sine wave pattern
        return Math.floor(512 + 400 * Math.sin(now * 0.001));

      case 1: // Sensor 2 - Random noise
        return Math.floor(Math.random() * 1024);

      case 2: // Sensor 3 - Pulse pattern
        return Math.floor(
          Math.random() > 0.8
            ? 800 + Math.random() * 223
            : 100 + Math.random() * 200
        );

      case 3: // Sensor 4 - Sawtooth wave
        return Math.floor(((now % 5000) / 5000) * 1024);

      case 4: // Sensor 5 - Combined pattern
        return Math.floor(
          300 + 200 * Math.sin(now * 0.002) + 100 * Math.random()
        );

      default:
        return Math.floor(Math.random() * 1024);
    }
  }

  disconnect() {
    this.stopSendingData();
    this.socket.disconnect();
  }
}

// If running this file directly, create multiple test clients
if (require.main === module) {
  const clients = [];
  const numClients = 5;

  console.log(`Starting ${numClients} test sensor clients...`);

  // Create multiple clients, simulating different connection times
  for (let i = 0; i < numClients; i++) {
    setTimeout(() => {
      const client = new SensorClient(i);
      clients.push(client);
      console.log(`Sensor client ${i} starting...`);
    }, i * 1000); // Start each client 1 second apart
  }

  // Graceful shutdown handling
  process.on("SIGINT", () => {
    console.log("\nClosing all test clients...");
    clients.forEach((client) => client.disconnect());
    process.exit(0);
  });

  // Randomly disconnect and reconnect clients (optional)
  setInterval(() => {
    if (clients.length > 0 && Math.random() > 0.9) {
      const randomClient = clients[Math.floor(Math.random() * clients.length)];
      console.log(`Randomly disconnecting client ${randomClient.clientId}`);
      randomClient.disconnect();

      // Reconnect after 5 seconds
      setTimeout(() => {
        console.log(`Reconnecting client ${randomClient.clientId}`);
        const newClient = new SensorClient(randomClient.clientId);
        const index = clients.indexOf(randomClient);
        clients[index] = newClient;
      }, 5000);
    }
  }, 15000); // Check every 15 seconds
}

module.exports = SensorClient;
