#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// wifi config
const char* AP_SSID = "CardTalk";
String wifiPassword = "";

WebServer server(80);
WebSocketsServer webSocket(81);

//blinking cursor globals
bool cursorVisible = true;
unsigned long lastCursorBlink = 0;

//battery globals
unsigned long lastBatteryUpdate = 0;
int batteryPercent = 0;

//client counter
int clientCount = 0;

//messages and input variables
String messages[8];
int messageCount = 0;
String currentInput = "";

//declares functions by function prototype
void drawChat();
void notificationBeep();
void addMessageLine(const String& line);
void broadcastClients();
void sendChatJSON(const String& text);
void handleIncomingText(uint8_t num, const String& text);
void setupWiFiPassword();
void drawBattery();
int getBatteryPercent();

// web ui
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>CardTalk</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: monospace;
        }

        body {
            background: #06080b;
            color: #33ff88;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
        }

        #app {
            width: min(700px, 95vw);
            height: 90vh;
            display: flex;
            flex-direction: column;
            overflow: hidden;

            border: 2px solid #33ff88;
            border-radius: 12px;

            box-shadow: 0 0 20px #00ff6640;
        }

        header {
            display: flex;
            justify-content: space-between;
            align-items: center;

            padding: 14px;

            background: #09110d;
            border-bottom: 1px solid #1d5;
        }

        header h1 {
            font-size: 20px;
            letter-spacing: 2px;
        }

        #status {
            font-size: 14px;
        }

        .connected {
            color: #33ff88;
        }

        .disconnected {
            color: #ff5555;
        }

        #chat {
            flex: 1;
            overflow-y: auto;

            display: flex;
            flex-direction: column;
            gap: 8px;

            padding: 14px;

            background: #050607;
        }

        .msg {
            padding: 8px 10px;

            background: #111;
            border-left: 3px solid #33ff88;

            white-space: pre-wrap;
            word-break: break-word;
        }

        .system {
            border-left-color: #ffaa00;
            color: #ffcc66;
        }

        .host {
            border-left-color: #44ccff;
        }

        .client {
            border-left-color: #33ff88;
        }

        footer {
            display: flex;
            gap: 10px;

            padding: 12px;

            background: #09110d;
            border-top: 1px solid #1d5;
        }

        input {
            flex: 1;

            padding: 12px;

            background: #111;
            color: #33ff88;

            border: 1px solid #33ff88;
            outline: none;

            font-size: 15px;
        }

        button {
            padding: 12px 22px;

            background: #33ff88;
            color: black;

            border: none;
            cursor: pointer;

            font-weight: bold;
            transition: .15s;
        }

        button:hover {
            background: #55ffaa;
        }

        button:active {
            transform: scale(.97);
        }

        ::-webkit-scrollbar {
            width: 8px;
        }

        ::-webkit-scrollbar-thumb {
            background: #33ff88;
        }
    </style>
</head>

<body>

    <div id="app">

        <header>
            <h1>CARDTALK</h1>
            <div id="status" class="disconnected">● Offline</div>
        </header>

        <div id="chat"></div>

        <footer>
            <input id="message" placeholder="Type a message...">
            <button onclick="sendMessage()">SEND</button>
        </footer>

    </div>

    <script>
        let ws;

        const chat = document.getElementById("chat");
        const input = document.getElementById("message");
        const status = document.getElementById("status");

        function addMessage(text, type = "client") {
            const div = document.createElement("div");

            div.className = "msg " + type;
            div.textContent = text;

            chat.appendChild(div);
            chat.scrollTop = chat.scrollHeight;
        }

        function connect() {
            ws = new WebSocket("ws://" + location.hostname + ":81/");

            ws.onopen = () => {
                status.textContent = "● Connected";
                status.className = "connected";

                addMessage("Connected to CardTalk", "system");
            };

            ws.onclose = () => {
                status.textContent = "● Offline";
                status.className = "disconnected";

                addMessage("Connection lost", "system");

                setTimeout(connect, 2000);
            };

            ws.onmessage = (event) => {
                try {
                    const msg = JSON.parse(event.data);

                    if (msg.type === "chat")
                        addMessage(msg.text, msg.role || "client");

                    if (msg.type === "clients") {
                        status.textContent =
                            "● Connected (" + msg.count + ")";
                        status.className = "connected";
                    }

                } catch (e) {
                    addMessage(event.data, "system");
                }
            };
        }

        function sendMessage() {
            const text = input.value.trim();

            if (text === "" || !ws || ws.readyState !== 1)
                return;

            ws.send(JSON.stringify({
                type: "chat",
                text: text
            }));

            input.value = "";
        }

        input.addEventListener("keydown", (e) => {
            if (e.key === "Enter")
                sendMessage();
        });

        connect();
    </script>

</body>

</html>
)rawliteral";

String getTimestamp() {

  unsigned long seconds = millis() / 1000;

  int minutes = seconds / 60;
  int secs = seconds % 60;

  char buffer[8];
  sprintf(buffer, "[%02d:%02d]", minutes, secs);

  return String(buffer);
}

void addMessageLine(const String& line) {
  if (messageCount < 8) {
    messages[messageCount++] = line;
  } else {
    for (int i = 1; i < 8; i++) messages[i - 1] = messages[i];
    messages[7] = line;
  }
}

void broadcastClients(){
  clientCount = webSocket.connectedClients();

  String msg =
  "{\"type\":\"clients\",\"count\":" +
  String(clientCount) +
  "}";

  webSocket.broadcastTXT(msg);

  drawChat();
}

void notificationBeep() {
  M5Cardputer.Speaker.tone(1000, 100);
}

void printWrapped(String text, int maxChars = 26) {

  while (text.length() > 0) {

    if (text.length() <= maxChars) {
      M5Cardputer.Display.println(text);
      return;
    }

    int split = text.lastIndexOf(' ', maxChars);

    if (split <= 0) {
      split = maxChars;
    }

    M5Cardputer.Display.println(text.substring(0, split));

    if (split == maxChars)
      text = text.substring(split);
    else
      text = text.substring(split + 1);

    while (text.startsWith(" ")) {
      text.remove(0, 1);
    }
  }
}

void drawChat() {

  clientCount = webSocket.connectedClients();

  M5Cardputer.Display.fillScreen(TFT_BLACK);

  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);

  //title
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.print("CardTalk");

  //battery percent
  String batteryText = String(batteryPercent) + "%";

  int batteryX = 240 - M5Cardputer.Display.textWidth(batteryText) - 2;

  M5Cardputer.Display.setCursor(batteryX, 0);
  M5Cardputer.Display.print(batteryText);

  //client num
  M5Cardputer.Display.setCursor(0, 12);
  M5Cardputer.Display.println(
    "Clients: " + String(clientCount)
  );

  M5Cardputer.Display.println("----------------");

  //chat messages
  const int visibleMessages = 8;

  int start = max(0, messageCount - visibleMessages);

  for (int i = start; i < messageCount; i++) {
    printWrapped(messages[i]);
  }

  M5Cardputer.Display.println("----------------");

  //input area
  M5Cardputer.Display.print("> ");

  String input = currentInput;

  if (cursorVisible) {
    input += "_";
  }

  printWrapped(input, 24);
}

void sendChatJSON(const String& text) {

  StaticJsonDocument<512> doc;

  doc["type"] = "chat";
  doc["text"] = text;
  doc["role"] = "host";

  String json;
  serializeJson(doc, json);

  webSocket.broadcastTXT(json);
}

void handleIncomingText(uint8_t num, const String& text) {

  IPAddress ip = webSocket.remoteIP(num);

  String line = getTimestamp() + " " + ip.toString() + ": " + text;

  addMessageLine(line);
  notificationBeep();
  drawChat();


  StaticJsonDocument<256> doc;

  doc["type"] = "chat";
  doc["text"] = line;
  doc["role"] = "client";

  String json;
  serializeJson(doc, json);

  webSocket.broadcastTXT(json);
}

void webSocketEvent(uint8_t num,WStype_t type,uint8_t *payload,size_t length){

  switch(type){

    case WStype_CONNECTED:{
      IPAddress ip=webSocket.remoteIP(num);
      Serial.print("Client connected: ");
      Serial.println(ip);

      addMessageLine(getTimestamp()+" "+ip.toString()+" joined");


      //send message history
      for (int i = 0; i < messageCount; i++) {

        StaticJsonDocument<256> doc;

        doc["type"] = "chat";
        doc["text"] = messages[i];
        doc["role"] = "system";

        String json;
        serializeJson(doc, json);

        webSocket.sendTXT(num, json);
      }

      drawChat();
      broadcastClients();
      break;
    }

    case WStype_DISCONNECTED:{
      Serial.print("Client disconnected: ");
      Serial.println(num);
      broadcastClients();
      break;
    }

    case WStype_TEXT:{
      StaticJsonDocument<1024> doc;

      DeserializationError error =
        deserializeJson(doc, payload, length);

      if(error){
      Serial.println("JSON parse failed");

      String raw = "";

      for(size_t i = 0; i < length; i++){
        raw += (char)payload[i];
      }

      handleIncomingText(num, raw);

    break;
    }

    String type = doc["type"] | "";

    if(type == "chat"){

      String text = doc["text"] | "";

      if(text.length() > 200){
        text = text.substring(0,200);
      }

      if(text.length() > 0){
        handleIncomingText(num, text);
      }
    }
  break;
}

    default:
      break;
  }
}

void setupWiFiPassword() {

  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(0,0);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.println("CardTalk Setup");
  M5Cardputer.Display.println("----------------");
  M5Cardputer.Display.println("Enter WiFi password:");
  M5Cardputer.Display.println("> ");

  String input = "";

  while(true){

    M5Cardputer.update();

    if(M5Cardputer.Keyboard.isChange()){

      Keyboard_Class::KeysState status =
        M5Cardputer.Keyboard.keysState();


      for(char c : status.word){
        input += c;

        M5Cardputer.Display.print("*");
      }


      if(status.del && input.length() > 0){

        input.remove(input.length()-1);

        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setCursor(0,0);

        M5Cardputer.Display.println("CardTalk Setup");
        M5Cardputer.Display.println("----------------");
        M5Cardputer.Display.println("Enter WiFi password:");
        M5Cardputer.Display.print("> ");

        for(int i=0;i<input.length();i++)
          M5Cardputer.Display.print("*");
      }


      if(status.enter){

        if(input.length() < 8){

          M5Cardputer.Display.println();
          M5Cardputer.Display.println("Password too short!");
          delay(1500);

          input = "";

          M5Cardputer.Display.fillScreen(TFT_BLACK);
          M5Cardputer.Display.setCursor(0,0);

          M5Cardputer.Display.println("CardTalk Setup");
          M5Cardputer.Display.println("----------------");
          M5Cardputer.Display.println("Enter WiFi password:");
          M5Cardputer.Display.print("> ");

          continue;
        }

        wifiPassword = input;

        M5Cardputer.Display.println();
        M5Cardputer.Display.println("Password saved!");
        delay(1000);

        return;
      }
    }
  }
}

void drawBattery() {

  String batteryText = String(batteryPercent) + "%";

  int batteryX = 240 - M5Cardputer.Display.textWidth(batteryText) - 2;

  //clear old text
  M5Cardputer.Display.fillRect(180, 0, 60, 12, TFT_BLACK);

  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.setCursor(batteryX, 0);
  M5Cardputer.Display.print(batteryText);
}

int getBatteryPercent() {
  float voltage = M5Cardputer.Power.getBatteryVoltage();

  int percent;

  if (voltage >= 4200)
    percent = 100;
  else if (voltage <= 3300)
    percent = 0;
  else
    percent = (voltage - 3300) * 100 / 900;

  return percent;
}

void updateBattery() {
  batteryPercent = getBatteryPercent();
}

void setup(){

  Serial.begin(115200);

  auto cfg=M5.config();
  M5Cardputer.begin(cfg,true);
  updateBattery();
  M5Cardputer.Display.setRotation(1);

  setupWiFiPassword();

  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);

  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP,gateway,subnet);
  WiFi.softAP(AP_SSID,wifiPassword.c_str());

  Serial.println("CardTalk AP Active");
  Serial.print("Connect to: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/",[](){
    server.send(200,"text/html",INDEX_HTML);
  });

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //print messages
  addMessageLine(getTimestamp()+" CardTalk Started");

  updateBattery();
  drawChat();
}

void loop() {

  server.handleClient();
  webSocket.loop();
  M5Cardputer.update();

  if (millis() - lastCursorBlink >= 500) {
    lastCursorBlink = millis();
    cursorVisible = !cursorVisible;
    drawChat();
  }

  if (millis() - lastBatteryUpdate > 30000) {
    lastBatteryUpdate = millis();

    updateBattery();
    drawBattery();
  }

  if (M5Cardputer.Keyboard.isChange()) {

    Keyboard_Class::KeysState status =
        M5Cardputer.Keyboard.keysState();

    //type normally
    for (char c : status.word) {
        currentInput += c;
    }

    //backspace
    if (status.del && currentInput.length() > 0) {
        currentInput.remove(currentInput.length() - 1);
    }

    //send
    if (status.enter && currentInput.length() > 0) {
        String msg = currentInput;
        currentInput = "";
        
        String line = getTimestamp() + " HOST: " + msg;

        addMessageLine(line);
        drawChat();
        sendChatJSON(line);
    }

    drawChat();
  }
}
