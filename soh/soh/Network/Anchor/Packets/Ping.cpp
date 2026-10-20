#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/**
 * PING
 *
 * Web only. A keepalive this client addresses to itself (targetClientId), so the server sends it straight back. Its
 * return shows the whole path (browser, bridge, server) still works: a WebSocket that silently died never closes by
 * itself, and nothing else comes in while nobody moves. The bridge also closes connections whose browser stopped
 * sending these. Other clients never see them.
 */

void Anchor::SendPacket_Ping() {
#ifdef __EMSCRIPTEN__
    nlohmann::json payload;
    payload["type"] = PING;
    payload["clientId"] = ownClientId;
    payload["targetClientId"] = ownClientId;
    payload["quiet"] = true;
    payload["sentAt"] = emscripten_get_now();
    // Its return confirms the save changes sent before it (Anchor::unackedPackets)
    if (!unackedPackets.empty()) {
        payload["ack"] = unackedPackets.back().first;
    }

    lastPingTime = emscripten_get_now();
    Network::SendJsonToRemote(payload);
#endif
}

void Anchor::HandlePacket_Ping(nlohmann::json payload) {
#ifdef __EMSCRIPTEN__
    if (payload.contains("sentAt")) {
        pingMs = (int)(emscripten_get_now() - payload["sentAt"].get<double>());
    }
    if (payload.contains("ack")) {
        uint32_t ack = payload["ack"].get<uint32_t>();
        while (!unackedPackets.empty() && unackedPackets.front().first <= ack) {
            unackedPackets.pop_front();
        }
    }
#endif
}
