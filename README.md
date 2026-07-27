# CardTalk

CardTalk is a simple real-time chat application inspired by the simplicity of a walkie-talkie. Users can join the same room and send messages instantly with no accounts, setup, or downloads required.

The project includes both a Node.js server version for internet communication and an ESP32 version that creates its own Wi-Fi network for local communication.

## Interface

### Chat Screen

<img width="994" height="1404" alt="image" src="https://github.com/user-attachments/assets/65ab1493-321c-46da-9221-9490b8595ac1" />

## Features

* Real-time messaging using WebSockets
* Displays the sender's IP address
* A live counter which counts connected clients
* Works on desktop and mobile
* Runs on ESP32 or Cardputer!

## Project Structure

```text
public/        → Frontend files
server.js      → Node.js WebSocket server
package.json   → Project dependencies
```

## Flashing

1. Download `firmware.bin` from the latest GitHub Release.
2. Connect your M5Stack Cardputer with USB.
3. Open https://espflash.app/
4. Select the firmware file.
5. Click **Flash**.
6. After flashing, reboot the Cardputer.

## Running

Requirements:

* Node.js
* npm

Install dependencies:

```bash
npm install
```

Start the server:

```bash
node server.js
```

Open:

```text
http://localhost:3000
```

To make it publicly accessible, expose port **3000** using Cloudflare Tunnel or another reverse proxy.

## ESP32 Version

The ESP32 version hosts everything directly on the board.

After flashing the sketch, it creates a Wi-Fi network:

```text
CardTalk
```

Then open:

```text
http://192.168.4.1
```

No internet connection is required for ESP32 Version!

## Technologies

* Node.js
* Express
* WebSockets
* HTML
* CSS
* JavaScript
* Arduino
* ESP32

## Future Plans

* Voice communication
* Usernames instead of IP addresses
* Private rooms
