const express = require("express");
const http = require("http");
const WebSocket = require("ws");

const app = express();
app.use(express.static("public"));

const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = 3000;

const HISTORY_LIMIT = 64;
const history = [];

function getTime() {
    const d = new Date();

    return (
        String(d.getHours()).padStart(2, "0") +
        ":" +
        String(d.getMinutes()).padStart(2, "0")
    );
}

function addHistory(sender, text) {
    history.push({
        time: getTime(),
        sender,
        text
    });

    if (history.length > HISTORY_LIMIT) {
        history.shift();
    }
}

function clientCount() {
    let count = 0;

    for (const client of wss.clients) {
        if (client.readyState === WebSocket.OPEN) {
            count++;
        }
    }

    return count;
}

function broadcast(obj) {
    const json = JSON.stringify(obj);

    for (const client of wss.clients) {
        if (client.readyState === WebSocket.OPEN) {
            client.send(json);
        }
    }
}

function updateClients() {
    broadcast({
        type: "clients",
        count: clientCount()
    });
}

wss.on("connection", (ws, req) => {
    const ip =
        req.headers["cf-connecting-ip"] ||
        req.socket.remoteAddress;

    ws.ip = ip;

    ws.send(JSON.stringify({
        type: "history",
        messages: history
    }));

    const join = {
        type: "chat",
        time: getTime(),
        sender: "SYSTEM",
        text: `${ip} joined`
    };

    addHistory(join.sender, join.text);
    broadcast(join);

    updateClients();

    ws.on("message", raw => {
        let packet;

        try {
            packet = JSON.parse(raw);
        } catch {
            return;
        }

        if (packet.type !== "chat") return;

        const text = String(packet.text || "").trim();

        if (!text) return;

        const msg = {
            type: "chat",
            time: getTime(),
            sender: "Me",
            text
        };

        addHistory(ip, text);

        ws.send(JSON.stringify(msg));

        const others = JSON.stringify({
            type: "chat",
            time: msg.time,
            sender: ip,
            text
        });

        for (const client of wss.clients) {
            if (
                client !== ws &&
                client.readyState === WebSocket.OPEN
            ) {
                client.send(others);
            }
        }
    });

    ws.on("close", () => {
        const leave = {
            type: "chat",
            time: getTime(),
            sender: "SYSTEM",
            text: `${ip} left`
        };

        addHistory(leave.sender, leave.text);

        broadcast(leave);

        updateClients();
    });
});

server.listen(PORT, () => {
    console.log(`CardTalk running on http://localhost:${PORT}`);
});
