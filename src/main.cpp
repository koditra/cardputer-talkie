#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// wifi config
const char* AP_SSID = "CardTalk";
const char* AP_PASS = "";

WebServer server(80);
WebSocketsServer webSocket(81);

//messages and input variables
String messages[8];
int messageCount = 0;
String currentInput = "";


// web ui
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">

<title>CardTalk</title>

<style>

* {
  box-sizing:border-box;
  margin:0;
  padding:0;
  user-select:none;
}

body {
  background:#0b0e14;
  color:#e0e6ed;
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
  height:100vh;
  display:flex;
  flex-direction:column;
  justify-content:space-between;
  align-items:center;
  padding:24px 16px;
}

header {
  text-align:center;
}

h1 {
  font-size:2rem;
  letter-spacing:3px;
  color:#00e676;
}

.status-bar {
  margin-top:8px;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:8px;
  color:#8a99ad;
}

.status-dot {
  width:10px;
  height:10px;
  border-radius:50%;
  background:#ff5252;
}

.status-dot.connected {
  background:#00e676;
}

#chat {
  width:95%;
  max-width:500px;
  height:60vh;
  overflow-y:auto;
  background:#111827;
  border-radius:15px;
  padding:15px;
}

.message {
  background:#1f2937;
  padding:10px;
  margin-bottom:10px;
  border-radius:10px;
}

.ip {
  color:#00e676;
  font-weight:bold;
}

.input-area {
  width:95%;
  max-width:500px;
  display:flex;
  gap:8px;
}

input {
  flex:1;
  padding:12px;
  border-radius:10px;
  border:none;
  font-size:16px;
}

button {
  padding:12px 20px;
  border:none;
  border-radius:10px;
  background:#00e676;
  font-weight:bold;
}

footer {
  color:#64748b;
}

</style>

</head>

<body>

<header>

<h1>CardTalk</h1>

<div class="status-bar">
<span id="dot" class="status-dot"></span>
<span id="status">Disconnected</span>
</div>

</header>


<div id="chat"></div>


<div class="input-area">

<input id="message" placeholder="Message">

<button onclick="sendMessage()">
Send
</button>

</div>


<footer>
ESP32 Local Chat
</footer>


<script>

let ws;


const chat=document.getElementById("chat");
const input=document.getElementById("message");
const status=document.getElementById("status");
const dot=document.getElementById("dot");


function connect(){

  ws=new WebSocket(
    "ws://"+location.hostname+":81/"
  );


  ws.onopen=()=>{

    status.innerText="Connected";
    dot.classList.add("connected");

  };


  ws.onclose=()=>{

    status.innerText="Disconnected";
    dot.classList.remove("connected");

    setTimeout(connect,2000);

  };


  ws.onmessage=(event)=>{

    let msg=JSON.parse(event.data);


    if(msg.type==="chat"){

      let div=document.createElement("div");

      div.className="message";

      div.innerHTML=
      "<span class='ip'>"+
      msg.ip+
      "</span><br>"+
      msg.text;


      chat.appendChild(div);

      chat.scrollTop=chat.scrollHeight;

    }

  };

}


function sendMessage(){

  if(input.value.length===0)
    return;


  if(ws.readyState===WebSocket.OPEN){

    ws.send(
      JSON.stringify(input.value)
    );

    input.value="";

  }

}


input.addEventListener("keydown",(e)=>{

  if(e.key==="Enter")
    sendMessage();

});


connect();

</script>

</body>
</html>

)rawliteral";

void broadcastClients(){
  String msg="{\"type\":\"clients\",\"count\":"+String(webSocket.connectedClients())+"}";
  webSocket.broadcastTXT(msg);
}

void webSocketEvent(uint8_t num,WStype_t type,uint8_t *payload,size_t length){

  switch(type){

    case WStype_CONNECTED:{
      IPAddress ip=webSocket.remoteIP(num);
      Serial.print("Client connected: ");
      Serial.println(ip);
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
      IPAddress ip=webSocket.remoteIP(num);

      String text="";
      for(size_t i=0;i<length;i++){
        text+=(char)payload[i];
      }

      String json="{\"type\":\"chat\",\"ip\":\""+ip.toString()+"\",\"text\":"+text+"}";

      for(uint8_t i=0;i<WEBSOCKETS_SERVER_CLIENT_MAX;i++){
        if(i != num){
          webSocket.sendTXT(i,json);
        }
      }

      break;
    }

    default:
      break;
  }
}



void setup(){

  Serial.begin(115200);

  M5Cardputer.begin();

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_GREEN);
  M5Cardputer.Display.setTextSize(1);

  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP,gateway,subnet);
  WiFi.softAP(AP_SSID,AP_PASS);

  Serial.println("CardTalk AP Active");
  Serial.print("Connect to: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/",[](){
    server.send(200,"text/html",INDEX_HTML);
  });

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop(){
  server.handleClient();
  webSocket.loop();
}
