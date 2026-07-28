# CardTalk

CardTalk is a simple texting application inspired by the simplicity of a walkie talkie. Users can join the same room and send messages and dont need accounts, setup, or any downloads.

The project includes both a Node.js server version for internet communication and an ESP32 version that creates its own Wi-Fi network for local communication.

## Interface

### Chat Screen

<img width="3024" height="1416" alt="image" src="https://github.com/user-attachments/assets/d2b088d6-b1d1-4711-a1c6-f8494ebe0e69" />

## Features

- chat directly from the cardputer using its keyboard
- browser interface for desktop and mobile
- live messages
- stores the last 64 messages for new connections (last 8 on cardputer)
- battery percentage display
- notification sound for incoming messages
- connected client counter

---

## Requirements

### Cardputer

- M5Stack cardputer
- usb-c cable

### Server

- node.js
- npm
- a computer connected to the same Wi-Fi network

---

## Flashing the Cardputer

### easy method (recommended)

1. Download the latest **firmware.bin** from the Releases page.
2. Open https://espflash.app
3. Connect your Cardputer using a USB-C cable.
4. Select the downloaded **firmware.bin**.
5. Click **Flash**.
6. Reboot the Cardputer.

### harder method (build from source)

Install PlatformIO:

```bash
pip install platformio
```

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/cardputer-talkie.git
cd cardputer-talkie
```

Build:

```bash
pio run
```

Upload:

```bash
pio run -t upload
```

---

## Running the Server

Install dependencies:

```bash
npm install
```

Start the server:

```bash
node server.js
```

The server runs on port **3000** by default.

---

## Connecting

1. Start the server.
2. Connect the Cardputer to the same Wi-Fi network.
3. Enter the server's local IP address on the Cardputer.
4. Open a browser and visit:

```
http://SERVER_IP:3000
```

Replace `SERVER_IP` with the IP address of the computer running the server.

---

## Browser Features

- live chat
- connected client counter
- displays the last 64 messages
- works on desktop and mobile browsers

---

## Cardputer Features

- native keyboard typing
- battery percentage display
- beep sound for notification alert
- message history saves for last 8
- text wrapping and quality of life features

---

## Project Structure

```
cardputer-talkie/
├── public/
│   └── index.html
├── src/
│   └── main.cpp
├── server.js
├── platformio.ini
└── README.md
```

---

## Releases

Each GitHub release includes:

- firmware.bin
- Source code
- Release notes

u can flash the included firmware using https://espflash.app.

---

## Why This Exists

The M5Stack Cardputer pretty cool, and I thought it would be even cooler if i could talk to my friends who live right next door even when my parents have my phone! :)

CardTalk was created as an easy way to chat between Cardputers and web browsers using a local AP (local wifi). I rlly liked how it turned out because now anyone can message me with any device with a web browser (as long as they are close by).

---

## future ideas :)

- personal usernames
- private messaging
- encryption for messages
- mesh network with LORA?

---

## Credits

i made it :)

Powered by:

- PlatformIO
- Arduino
- ESP32
- M5Cardputer
- Express
- ws (WebSocket)

Special thanks to Lily Flowers (@lily the milk cool) for testing and feedback :)
