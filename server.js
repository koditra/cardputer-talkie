const express = require("express");
const http = require("http");
const WebSocket = require("ws");

const app = express();

app.use(express.static("public"));

const server = http.createServer(app);

const wss = new WebSocket.Server({
    server
});

server.listen(3000, () => {
    console.log("CardTalk running on port 3000");
});

function updateClients() {

    const count = [...wss.clients]
        .filter(c => c.readyState === WebSocket.OPEN)
        .length;

    const msg = JSON.stringify({
        type: "clients",
        count
    });

    for (const client of wss.clients) {

        if (client.readyState === WebSocket.OPEN) {
            client.send(msg);
        }

    }

}

wss.on("connection", (ws, req) => {

    const ip =
        req.headers["cf-connecting-ip"] ||
        req.socket.remoteAddress;

    ws.ip = ip;

    console.log("Client connected:", ip);

    updateClients();

    ws.on("message", (data) => {

        try {

            const msg = JSON.parse(data);

            if (msg.type === "message") {

                for (const client of wss.clients) {

                    if (
                        client !== ws &&
                        client.readyState === WebSocket.OPEN
                    ) {

                        client.send(JSON.stringify({
                            type: "message",
                            text: msg.text,
                            sender: ws.ip
                        }));

                    }

                }

            }

        } catch (e) {}

    });

    ws.on("close", () => {

        console.log("Client disconnected:", ws.ip);

        updateClients();

    });

});
