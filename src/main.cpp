#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// wifi config
const char* AP_SSID = "CardTalk";
String wifiPassword = "";

WebServer server(80);
WebSocketsServer webSocket(81);

//blinking cursor globals
bool cursorVisible = true;
unsigned long lastCursorBlink = 0;

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
String jsonEscape(String s);
void broadcastClients();
void sendChatJSON(const String& text);
void handleIncomingText(uint8_t num, const String& text);
void setupWiFiPassword();

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
                        document.getElementById("status").textContent =
                            "● Connected (" + msg.count + ")";
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


String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", " ");
  s.replace("\r", " ");
  return s;
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
  M5Cardputer.Display.setCursor(0, 0);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.println("CardTalk");
  M5Cardputer.Display.println(
    "Clients: " + String(clientCount)
  );
  M5Cardputer.Display.println("----------------");

  const int visibleMessages = 8;

  int start = max(0, messageCount - visibleMessages);

  for (int i = start; i < messageCount; i++) {

    printWrapped(messages[i]);

  }

  M5Cardputer.Display.println("----------------");

  M5Cardputer.Display.print("> ");

  String input = currentInput;

  if (cursorVisible) {
      input += "_";
  }

  printWrapped(input, 24);
}

void sendChatJSON(const String& text) {
  String json =
  "{\"type\":\"chat\",\"text\":\"" +
  jsonEscape(text) +
  "\",\"role\":\"host\"}";
  webSocket.broadcastTXT(json);
}

void handleIncomingText(uint8_t num, const String& text) {
  IPAddress ip=webSocket.remoteIP(num);
  String line=getTimestamp()+" "+ip.toString()+": "+text;

  addMessageLine(line);
  notificationBeep();
  drawChat();

  String json=
  "{\"type\":\"chat\",\"text\":\""+
  jsonEscape(line)+
  "\",\"role\":\"client\"}";

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

        String json =
        "{\"type\":\"chat\",\"text\":\"" +
        jsonEscape(messages[i]) +
        "\",\"role\":\"system\"}";

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
      String text="";
      for(size_t i=0;i<length;i++){
        text+=(char)payload[i];
      }

      if(text.startsWith("{") && text.indexOf("\"type\":\"chat\"") >= 0){
        int p=text.indexOf("\"text\":\"");
        if(p >= 0){
          p += 8;
          String body=text.substring(p);
          int end=body.lastIndexOf('"');
          if(end > 0) body=body.substring(0,end);
          handleIncomingText(num,body);
          break;
        }
      }

      handleIncomingText(num,text);
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

void setup(){

  Serial.begin(115200);

  auto cfg=M5.config();
  M5Cardputer.begin(cfg,true);
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
        addMessageLine(getTimestamp()+" HOST: "+msg);
        drawChat();
        sendChatJSON(getTimestamp()+" HOST: "+msg);
    }

    drawChat();
  }
}
