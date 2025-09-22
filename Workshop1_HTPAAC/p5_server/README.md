# P5.js Sensor Network Visualization System

This is a real-time sensor data visualization system based on p5.js that can monitor up to 5 client sensors and visualize transmitted data in real-time.

## Features

- **Real-time Data Visualization**: Beautiful particle systems and node networks using p5.js
- **Dual Protocol Communication**: Supports both WebSocket and Socket.IO for different client types
- **Multi-client Support**: Supports up to 5 sensor clients simultaneously
- **Dynamic Visual Effects**: Dynamic particle emission and node pulsing based on sensor data intensity
- **Connection Status Monitoring**: Real-time display of client connection status and data activity
- **Responsive Design**: Adapts to different screen sizes

## System Architecture

```
Sensor Clients (p5.js + ESP32)     Visualization Interface (p5.js + HTML)
    ↓ WebSocket (/ws)                    ↓ Socket.IO
         Server (Node.js)
    (WebSocket Server + Socket.IO Server)
```

**Two Connection Types:**

- **WebSocket** (`ws://localhost:3000/ws`) - For sensor clients (your p5.js client)
- **Socket.IO** (`http://localhost:3000`) - For visualization interface

## Installation and Setup

### 1. Install Dependencies

```bash
npm install
```

### 2. Start Server

```bash
npm start
```

Or use development mode (auto-restart):

```bash
npm run dev
```

### 3. Access Visualization Interface

Open browser and go to: `http://localhost:3000`

### 4. Connect Sensor Clients

Each sensor client should connect to: `ws://localhost:3000/ws`

## Client Integration

### Your P5.js Client Setup

1. **Change NODE_ID** for each group (0-4):

   ```javascript
   const NODE_ID = 0; // Change this for each group!
   ```

2. **Set correct server URL**:

   ```javascript
   const SERVER_URL = "ws://localhost:3000/ws"; // Note the /ws path!
   ```

3. **Use the provided client example**: `client-example.html`

### Connection Protocol

Your p5.js client should send:

**Registration:**

```javascript
ws.send(
  JSON.stringify({
    type: "register",
    clientType: "gsr_node",
    nodeId: NODE_ID, // 0-4
    groupId: NODE_ID + 1, // 1-5
  })
);
```

**Data Transmission:**

```javascript
ws.send(
  JSON.stringify({
    type: "gsr_data",
    nodeId: NODE_ID,
    groupId: NODE_ID + 1,
    ema: sensorValue, // Your GSR/sensor data
    timestamp: Date.now(),
  })
);
```

## How It Works

### Server Behavior

1. **No Auto-Connection**: Sensor buttons remain off until actual clients connect
2. **Real Client Detection**: Only lights up when a real p5.js client connects via WebSocket
3. **Dual Protocol Support**:
   - WebSocket server handles sensor clients
   - Socket.IO server handles visualization interface

### Client Behavior

1. **Manual Connection**: Click "SERVER" button in your p5.js client to connect
2. **Real-time Data**: ESP32 sensor data flows through WebSocket to visualization
3. **Visual Feedback**: LED strip shows data flow, server shows particle effects

### Visualization Features

#### Node Colors (Automatically assigned by NODE_ID)

- **Group 1 (NODE_ID=0)**: Deep Pink
- **Group 2 (NODE_ID=1)**: Cyan
- **Group 3 (NODE_ID=2)**: Orange
- **Group 4 (NODE_ID=3)**: Lime Green
- **Group 5 (NODE_ID=4)**: Medium Purple

#### Visual Effects

- **Node Pulsing**: Based on sensor data intensity
- **Particle Emission**: Data particles flow between connected sensors
- **Orbital Rings**: Dynamic rings around active sensors
- **Connection Lines**: Data flow visualization between sensors
- **Background Particles**: Ambient tech atmosphere

#### UI Elements

- **Network Status**: Shows connected sensor count and particle count
- **Data Activity**: Shows overall data transmission activity level
- **Connection Status**: Shows server connection status
- **Sensor Buttons**: Bottom panel shows each sensor's connection status

## Configuration Options

In `sketch.js` you can adjust:

```javascript
const NUM_NODES = 5; // Number of sensors
const PARTICLE_EMIT_RATE = 0.2; // Particle emission rate
const MAX_PARTICLES = 600; // Maximum particles
const ATTRACTION_FORCE = 0.025; // Particle attraction
const REPULSION_FORCE = 0.6; // Particle repulsion
const MAX_SPEED = 8; // Maximum particle speed
```

## API Endpoints

### REST API

- `GET /api/status`: Get system status
  ```json
  {
    "connectedClients": 1,
    "sensorClients": 0,
    "webSocketClients": 0,
    "socketIOClients": 1,
    "data": { ... }
  }
  ```

### WebSocket Events (for sensor clients)

**Client → Server:**

- `register`: Register as sensor client
- `gsr_data`: Send sensor data

**Server → Client:**

- `register_ack`: Registration confirmation
- `server_ack`: Data received acknowledgment

### Socket.IO Events (for visualization)

**Client → Server:**

- `register_client`: Register as visualizer

**Server → Client:**

- `initial_data`: Initial system state
- `data_update`: Real-time data updates
- `client_status_update`: Connection status changes

## Troubleshooting

### Common Issues

1. **Clients Don't Connect**

   - Check server is running on correct port
   - Confirm firewall settings allow connections
   - Verify WebSocket connection URL includes `/ws` path
   - Make sure you're using `ws://` not `wss://` for local connections

2. **Data Not Updating**

   - Check client registration is successful
   - Confirm data format matches requirements
   - Look at server console logs
   - Verify NODE_ID is between 0-4

3. **Visualization Not Working**
   - Refresh browser page
   - Check browser console for errors
   - Confirm p5.js library loads correctly
   - Make sure you're accessing `http://localhost:3000` not the WebSocket URL

### Debug Mode

Enable detailed logging:

```bash
DEBUG=ws,socket.io* npm start
```

### Network Setup

The server automatically detects and displays all available network addresses when starting.

#### For Same Network (WiFi/Ethernet)

1. **Start the server** - it will display all available URLs:

   ```
   🚀 Server running on port 3000
   📱 Visualization interface:
      Local:    http://localhost:3000
      Network:  http://192.168.1.100:3000

   🔌 WebSocket endpoints for sensor clients:
      Local:    ws://localhost:3000/ws
      Network:  ws://192.168.1.100:3000/ws
   ```

2. **Use the auto-configuring client**: `http://192.168.1.100:3000/client`

   - Automatically detects server URL
   - Easy group selection dropdown
   - Real-time connection status
   - No code editing required!

3. **Or manually configure** your client:
   ```javascript
   const SERVER_URL = "ws://192.168.1.100:3000/ws"; // Use Network URL
   ```

#### For Different Networks

If clients are on completely different networks (different WiFi, mobile hotspot, etc.):

1. **Port forwarding** (if server is behind router):

   - Forward port 3000 to server computer
   - Use public IP: `ws://YOUR_PUBLIC_IP:3000/ws`

2. **Cloud deployment** (recommended for different networks):

   - Deploy to Heroku, Railway, or similar
   - Use: `wss://your-app.herokuapp.com/ws` (note `wss://` for SSL)

3. **Mobile hotspot method**:
   - Connect server computer to mobile hotspot
   - Connect client devices to same hotspot
   - Use hotspot network IP

## Files Structure

```
p5_server/
├── server.js              # Main server (WebSocket + Socket.IO)
├── package.json           # Dependencies
├── public/
│   ├── index.html         # Visualization interface
│   ├── sketch.js          # P5.js visualization code
│   └── client-template.html # Auto-configuring sensor client
├── client-example.html    # Manual sensor client example
├── test-client.js         # Node.js test client (not auto-run)
└── README.md             # This file
```

## Quick Start for Different Networks

### Easy Method (Recommended)

1. **Start server**: `npm start`
2. **Note the Network URLs** shown in console output
3. **On client devices**: Visit `http://[SERVER_IP]:3000/client`
4. **Select your group** and **enter server WebSocket URL**
5. **Click Connect** - no code editing needed!

### Manual Method

1. **Copy** `client-example.html` to client devices
2. **Edit** the `SERVER_URL` and `NODE_ID` variables
3. **Open** the file in browser on each client device

## License

MIT License

## Contributing

Feel free to submit Issues and Pull Requests!
