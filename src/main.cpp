#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// wifi config
const char* AP_SSID = "CardTalk";
const char* AP_PASS = ""; // open access point

WebServer server(80);
WebSocketsServer webSocket(81);

// web ui
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en"> 
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>CardTalk</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
    body {
      background-color: #0b0e14;
      color: #e0e6ed;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      display: flex;
      flex-direction: column;
      height: 100vh;
      justify-content: space-between;
      align-items: center;
      padding: 24px 16px;
    }
    header { text-align: center; margin-top: 8px; }
    h1 {
      font-size: 2rem;
      letter-spacing: 3px;
      color: #00e676;
      text-transform: uppercase;
    }
    .status-bar {
      margin-top: 6px;
      font-size: 0.85rem;
      color: #8a99ad;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
    }
    .status-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background-color: #ff5252;
      transition: background-color 0.3s;
    }
    .status-dot.connected { background-color: #00e676; }
    .ptt-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      width: 100%;
    }
    .ptt-btn {
      width: 210px;
      height: 210px;
      border-radius: 50%;
      border: 4px solid #1a2332;
      background: radial-gradient(circle, #1e293b 0%, #0f172a 100%);
      color: #ffffff;
      font-size: 1.1rem;
      font-weight: 700;
      letter-spacing: 1px;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      box-shadow: 0 10px 30px rgba(0,0,0,0.6);
      cursor: pointer;
      transition: transform 0.1s ease, border-color 0.2s;
      touch-action: manipulation;
    }
    .ptt-btn:active, .ptt-btn.transmitting {
      transform: scale(0.95);
      background: radial-gradient(circle, #ff1744 0%, #b71c1c 100%);
      border-color: #ff5252;
      box-shadow: 0 0 35px rgba(255, 23, 68, 0.5);
    }
    .tx-label {
      margin-top: 20px;
      font-size: 0.9rem;
      min-height: 24px;
      color: #ff5252;
      font-weight: 600;
      letter-spacing: 1px;
    }
    footer {
      font-size: 0.85rem;
      color: #64748b;
      text-align: center;
    }
  </style>
</head>

  <body>
  <header>
    <h1>CardTalk</h1>
    <div class="status-bar">
      <span id="dot" class="status-dot"></span>
      <span id="status-text">Disconnected</span>
    </div>
  </header>

  <div class="ptt-container">
    <div id="ptt" class="ptt-btn">
      <span>HOLD TO</span>
      <span>TALK</span>
    </div>
    <div id="tx-status" class="tx-label"></div>
  </div>

  <footer>
    Connected Clients: <strong id="client-count" style="color: #00e676;">1</strong>
  </footer>

<script>
    const pttBtn = document.getElementById('ptt');
    const statusText = document.getElementById('status-text');
    const dot = document.getElementById('dot');
    const txStatus = document.getElementById('tx-status');
    const clientCount = document.getElementById('client-count');

    let ws;
    let mediaRecorder;
    let audioStream;
    let isTransmitting = false;
    let audioCtx;

    function initAudioContext() {
      if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      }
      if (audioCtx.state === 'suspended') {
        audioCtx.resume();
      }
    }

    function connectWebSocket() {
      const wsUrl = `ws://${window.location.hostname}:81/`;
      ws = new WebSocket(wsUrl);
      ws.binaryType = 'arraybuffer';

      ws.onopen = () => {
        statusText.innerText = 'Connected';
        dot.classList.add('connected');
      };

      ws.onclose = () => {
        statusText.innerText = 'Disconnected';
        dot.classList.remove('connected');
        setTimeout(connectWebSocket, 2000);
      };

      ws.onmessage = async (event) => {
        if (typeof event.data === 'string') {
          try {
            const msg = JSON.parse(event.data);
            if (msg.type === 'clients') clientCount.innerText = msg.count;
            if (msg.type === 'tx') txStatus.innerText = msg.active ? 'RECEIVING AUDIO...' : '';
          } catch(e){}
        } else if (event.data instanceof ArrayBuffer) {
          playIncomingAudio(event.data);
        }
      };
    }

    function playIncomingAudio(buffer) {
      initAudioContext();
      const blob = new Blob([buffer], { type: 'audio/webm; codecs=opus' });
      const url = URL.createObjectURL(blob);
      const audio = new Audio(url);
      audio.play().catch(() => {});
    }

    async function startRecording() {
      initAudioContext();
      if (isTransmitting) return;

      try {
        if (!audioStream) {
          audioStream = await navigator.mediaDevices.getUserMedia({
            audio: { echoCancellation: true, noiseSuppression: true }
          });
        }

        isTransmitting = true;
        pttBtn.classList.add('transmitting');
        txStatus.innerText = 'TRANSMITTING...';

        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ type: 'tx', active: true }));
        }

        const options = MediaRecorder.isTypeSupported('audio/webm;codecs=opus')
          ? { mimeType: 'audio/webm;codecs=opus' }
          : {};

        mediaRecorder = new MediaRecorder(audioStream, options);

        mediaRecorder.ondataavailable = (e) => {
          if (e.data.size > 0 && ws && ws.readyState === WebSocket.OPEN && isTransmitting) {
            ws.send(e.data);
          }
        };

        mediaRecorder.start(100);
      } catch (err) {
        alert('Microphone access required for walkie-talkie functionality.');
        stopRecording();
      }
    }

    function stopRecording() {
      if (!isTransmitting) return;
      isTransmitting = false;
      pttBtn.classList.remove('transmitting');
      txStatus.innerText = '';

      if (mediaRecorder && mediaRecorder.state !== 'inactive') {
        mediaRecorder.stop();
      }

      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'tx', active: false }));
      }
    }

    pttBtn.addEventListener('pointerdown', (e) => { e.preventDefault(); startRecording(); });
    window.addEventListener('pointerup', () => stopRecording());
    window.addEventListener('pointercancel', () => stopRecording());

    window.onload = connectWebSocket;
  </script>
</body>
</html>
)rawliteral";

void broadcastClientCount() {
  uint8_t count = webSocket.connectedClients();
  String json = "{\"type\":\"clients\",\"count\":" + String(count) + "}";
  webSocket.broadcastTXT(json);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      broadcastClientCount();
      break;

    case WStype_CONNECTED:
      broadcastClientCount();
      break;

    case WStype_TEXT:
      for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (i != num && webSocket.isConnected(i)) {
          webSocket.sendTXT(i, payload, length);
        }
      }
      break;

    case WStype_BIN:
      for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (i != num && webSocket.isConnected(i)) {
          webSocket.sendBIN(i, payload, length);
        }
      }
      break;

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println("CardTalk AP Active");
  Serial.print("Connect to: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop();
}
