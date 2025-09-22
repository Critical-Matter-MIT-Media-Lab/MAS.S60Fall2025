// Simple WebSocket connection test
const WebSocket = require("ws");

function testConnection(url) {
  console.log(`\n🔄 Testing connection to: ${url}`);

  const ws = new WebSocket(url);

  const timeout = setTimeout(() => {
    console.log("❌ Connection timeout (10 seconds)");
    ws.close();
    process.exit(1);
  }, 10000);

  ws.on("open", () => {
    clearTimeout(timeout);
    console.log("✅ WebSocket connected successfully!");

    // Test registration
    console.log("📝 Testing registration...");
    ws.send(
      JSON.stringify({
        type: "register",
        clientType: "gsr_node",
        nodeId: 0,
        groupId: 1,
      })
    );
  });

  ws.on("message", (data) => {
    try {
      const parsed = JSON.parse(data.toString());
      console.log("📨 Received:", parsed);

      if (parsed.type === "register_ack") {
        console.log("🎉 Registration successful!");

        // Send test data
        console.log("📊 Sending test data...");
        ws.send(
          JSON.stringify({
            type: "gsr_data",
            nodeId: 0,
            groupId: 1,
            ema: 3200,
            timestamp: Date.now(),
          })
        );
      } else if (parsed.type === "server_ack") {
        console.log("✅ Server acknowledged data!");
        console.log("🎉 All tests passed! Connection working perfectly.");

        setTimeout(() => {
          ws.close();
          process.exit(0);
        }, 1000);
      }
    } catch (e) {
      console.log("📨 Received (raw):", data.toString());
    }
  });

  ws.on("close", (code, reason) => {
    clearTimeout(timeout);
    console.log(`🔌 Connection closed (code: ${code}, reason: ${reason})`);
  });

  ws.on("error", (error) => {
    clearTimeout(timeout);
    console.log("❌ WebSocket error:", error.message);

    // Provide specific troubleshooting
    console.log("\n🔍 Troubleshooting:");
    console.log("1. Check if server is running");
    console.log("2. Verify the IP address is correct");
    console.log("3. Make sure the path includes /ws");
    console.log("4. Check firewall settings");
    console.log("5. Try localhost first: ws://localhost:3000/ws");

    process.exit(1);
  });
}

// Get URL from command line or use default
const url = process.argv[2] || "ws://localhost:3000/ws";
testConnection(url);
