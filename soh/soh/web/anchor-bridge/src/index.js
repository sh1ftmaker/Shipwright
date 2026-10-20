/**
 * Ship of Harkinian - Anchor WebSocket bridge
 *
 * Browsers can't open TCP sockets, so the web build's Anchor client connects here over a WebSocket and this
 * Worker relays it to the same Anchor server desktop builds use (anchor.hm64.org:43383). The server speaks JSON
 * messages separated by NUL bytes; each WebSocket text message is one of those JSON messages. The room comes
 * from the client's HANDSHAKE packet, so web and desktop players in the same room meet.
 *
 * Each connection is relayed by its own Durable Object. A plain Worker counts a whole WebSocket as one request, so
 * on the free plan (10 ms of CPU per request) Cloudflare cut connections after a few seconds of gameplay traffic;
 * a Durable Object gets fresh CPU time with every incoming WebSocket message.
 *
 * Nothing is stored: each WebSocket gets its own TCP connection and both close together.
 *
 * Clients that send keepalives (PING, every 3 s) are closed after IDLE_TIMEOUT_MS without a message, so a browser
 * that vanished without closing (wifi gone, phone asleep) shows up as offline to the others quickly; otherwise the
 * server would keep it "online" until a write failed, which can take minutes. Older clients don't send keepalives,
 * so this only starts once one has been seen.
 */
import { connect } from "cloudflare:sockets";
import { DurableObject } from "cloudflare:workers";

const IDLE_TIMEOUT_MS = 30000;

export default {
  async fetch(request, env) {
    if (request.headers.get("Upgrade") !== "websocket") {
      return new Response("Ship of Harkinian Anchor bridge: connect with a WebSocket.\n", {
        headers: { "content-type": "text/plain" },
      });
    }
    const relay = env.RELAY.get(env.RELAY.newUniqueId());
    return relay.fetch(request);
  },
};

export class AnchorRelay extends DurableObject {
  async fetch(request) {
    const env = this.env;
    const host = env.ANCHOR_HOST || "anchor.hm64.org";
    const port = Number(env.ANCHOR_PORT || 43383);

    let socket;
    try {
      socket = connect({ hostname: host, port });
      await socket.opened;
    } catch (err) {
      return new Response(`Could not reach the Anchor server ${host}:${port}: ${err}`, { status: 502 });
    }

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    server.accept();

    const writer = socket.writable.getWriter();
    const encoder = new TextEncoder();
    let closed = false;
    let idleTimer;
    const closeBoth = (code, reason) => {
      if (closed) return;
      closed = true;
      clearInterval(idleTimer);
      try { server.close(code, reason); } catch {}
      try { writer.releaseLock(); } catch {}
      socket.close().catch(() => {});
    };

    // Browser -> Anchor server: one JSON message per WebSocket message, NUL-terminated on the wire
    let writing = Promise.resolve();
    let lastMessage = Date.now();
    let sendsKeepalives = false;
    server.addEventListener("message", (event) => {
      const text = typeof event.data === "string" ? event.data : new TextDecoder().decode(event.data);
      lastMessage = Date.now();
      if (!sendsKeepalives && text.includes('"PING"')) {
        sendsKeepalives = true;
        idleTimer = setInterval(() => {
          if (Date.now() - lastMessage > IDLE_TIMEOUT_MS) closeBoth(4000, "browser went quiet");
        }, 5000);
      }
      writing = writing.then(() => writer.write(encoder.encode(text + "\0"))).catch(() => closeBoth(1011, "server write failed"));
    });
    server.addEventListener("close", () => closeBoth(1000, "client closed"));
    server.addEventListener("error", () => closeBoth(1011, "client error"));

    // Anchor server -> browser: split the byte stream on NUL into WebSocket messages
    (async () => {
      const decoder = new TextDecoder();
      const reader = socket.readable.getReader();
      let pending = "";
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          pending += decoder.decode(value, { stream: true });
          let end;
          while ((end = pending.indexOf("\0")) !== -1) {
            const message = pending.slice(0, end);
            pending = pending.slice(end + 1);
            if (message.length) server.send(message);
          }
        }
        closeBoth(1000, "Anchor server closed the connection");
      } catch {
        closeBoth(1011, "Anchor server connection lost");
      }
    })();

    return new Response(null, { status: 101, webSocket: client });
  }
}
