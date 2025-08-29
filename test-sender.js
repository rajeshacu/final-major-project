// test-sender.js
const fs = require('fs');

let toggle = true; // alternate between P1 and P2

function writePacket() {
  const id = toggle ? "P1" : "P2";
  toggle = !toggle;

  // generate some slightly different test values
  const data = {
    id,
    temperature: (25 + Math.random() * 5).toFixed(1),
    pressure: (920 + Math.random() * 5).toFixed(1),
    altitude: (790 + Math.random() * 20).toFixed(1),
    latitude: id === "P1" ? 12.9238 + Math.random() * 0.001 : 0.000000,
    longitude: id === "P1" ? 77.4988 + Math.random() * 0.001 : 0.000000,
    battery: Math.floor(40 + Math.random() * 20),
    alert: Math.random() > 0.8 ? 1 : 0  // sometimes trigger alert
  };

  fs.writeFileSync("latest.txt", JSON.stringify(data));
  console.log("Wrote:", data);
}

// update every 2 seconds
setInterval(writePacket, 4000);
