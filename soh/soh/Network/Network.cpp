#include "Network.h"
#include <spdlog/spdlog.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/eventloop.h>
#endif

// MARK: - Public

void Network::Enable(const char* host, uint16_t port) {
#ifdef __EMSCRIPTEN__
    if (isEnabled) {
        return;
    }

    // Build WebSocket URL from host:port.
    // If host already contains a scheme (ws:// or wss://), use as-is with port.
    // Otherwise, default to wss:// for security (browsers block mixed content).
    std::string hostStr(host);
    std::string url;
    if (hostStr.find("ws://") == 0 || hostStr.find("wss://") == 0) {
        url = hostStr + ":" + std::to_string(port);
    } else {
        url = std::string("wss://") + host + ":" + std::to_string(port);
    }
    EnableWebSocket(url);
#else
    if (isEnabled) {
        return;
    }

    if (SDLNet_ResolveHost(&networkAddress, host, port) == -1) {
        SPDLOG_ERROR("[Network] SDLNet_ResolveHost: {}", SDLNet_GetError());
    }

    isEnabled = true;

    // First check if there is a thread running, if so, join it
    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    receiveThread = std::thread(&Network::ReceiveFromServer, this);
#endif
}

void Network::Disable() {
    if (!isEnabled) {
        return;
    }

    isEnabled = false;

#ifdef __EMSCRIPTEN__
    if (reconnectTimer != 0) {
        emscripten_clear_timeout(reconnectTimer);
        reconnectTimer = 0;
    }
    wsClient.Disconnect();
    isConnected = false;
    OnDisconnected();
#else
    receiveThread.join();
#endif
}

void Network::OnIncomingData(char payload[512]) {
}

void Network::OnIncomingJson(nlohmann::json payload) {
}

void Network::OnConnected() {
}

void Network::OnDisconnected() {
}

void Network::ProcessOutgoingPackets() {
}

void Network::SendDataToRemote(const char* payload) {
#ifdef __EMSCRIPTEN__
    if (isConnected) {
        wsClient.Send(std::string(payload));
    }
#else
    SPDLOG_DEBUG("[Network] Sending data: {}", payload);
    SDLNet_TCP_Send(networkSocket, payload, static_cast<int>(strlen(payload) + 1));
#endif
}

void Network::SendJsonToRemote(nlohmann::json payload) {
#ifdef __EMSCRIPTEN__
    if (isConnected) {
        wsClient.Send(payload.dump());
    }
#else
    SendDataToRemote(payload.dump().c_str());
#endif
}

#ifdef __EMSCRIPTEN__
void Network::EnableWebSocket(const std::string& url) {
    isEnabled = true;
    wsUrl = url;
    reconnectAttempts = 0;

    wsClient.SetOnConnect([this]() {
        isConnected = true;
        OnConnected();
    });

    wsClient.SetOnDisconnect([this]() {
        isConnected = false;
        if (isEnabled) {
            OnDisconnected();
            ScheduleReconnect();
        }
    });

    wsClient.Connect(url);
}

void Network::ScheduleReconnect() {
    if (reconnectTimer != 0 || !isEnabled || wsUrl.empty()) {
        return;
    }

    // 1s, 2s, 4s, 8s, then every 15s. Attempts only reset once the server accepts us (ResetReconnectBackoff), so a
    // bridge that opens and closes again straight away doesn't retry every second. The +-25% jitter keeps players
    // who dropped together (bridge or server restart) from all coming back in the same instant.
    static const int delays[] = { 1000, 2000, 4000, 8000, 15000 };
    reconnectAttempts++;
    double delayMs = delays[std::min(reconnectAttempts, 5) - 1] * (0.75 + 0.5 * emscripten_random());
    reconnectTime = emscripten_get_now() + delayMs;
    SPDLOG_INFO("[Network] WebSocket disconnected. Reconnecting in {}ms (attempt {})", (int)delayMs,
                reconnectAttempts);

    reconnectTimer = emscripten_set_timeout(
        [](void* userData) {
            Network* self = static_cast<Network*>(userData);
            self->reconnectTimer = 0;
            self->ReconnectNow();
        },
        delayMs, this);
}

void Network::ReconnectNow() {
    if (!isEnabled || isConnected || wsClient.IsConnecting() || wsUrl.empty()) {
        return;
    }
    if (reconnectTimer != 0) {
        emscripten_clear_timeout(reconnectTimer);
        reconnectTimer = 0;
    }

    SPDLOG_INFO("[Network] Attempting WebSocket reconnect to {}", wsUrl);
    if (!wsClient.Connect(wsUrl)) {
        ScheduleReconnect();
    }
}

void Network::ResetReconnectBackoff() {
    reconnectAttempts = 0;
}

int Network::GetReconnectAttempts() const {
    return reconnectAttempts;
}

double Network::GetReconnectDelay() const {
    return reconnectTimer != 0 ? std::max(0.0, reconnectTime - emscripten_get_now()) : 0;
}

void Network::PollIncoming() {
    if (!isEnabled) {
        return;
    }

    // Process outgoing packets
    ProcessOutgoingPackets();

    // Poll for incoming WebSocket messages
    std::string message;
    while (wsClient.Poll(message)) {
        HandleRemoteJson(message);
    }
}
#endif

// MARK: - Private

void Network::ReceiveFromServer() {
#ifndef __EMSCRIPTEN__
    while (isEnabled) {
        while (!isConnected && isEnabled) {
            SPDLOG_TRACE("[Network] Attempting to make connection to server...");
            networkSocket = SDLNet_TCP_Open(&networkAddress);

            if (networkSocket) {
                isConnected = true;
                receivedData.clear();
                SPDLOG_INFO("[Network] Connection to server established!");

                OnConnected();
                break;
            }
        }

        SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
        if (networkSocket) {
            SDLNet_TCP_AddSocket(socketSet, networkSocket);
        }

        // Listen to socket messages
        while (isConnected && networkSocket && isEnabled) {
            // we check first if socket has data, to not block in the TCP_Recv
            int socketsReady = SDLNet_CheckSockets(socketSet, 0);

            if (socketsReady == -1) {
                SPDLOG_ERROR("[Network] SDLNet_CheckSockets: {}", SDLNet_GetError());
                break;
            }

            // Always process outgoing packets
            ProcessOutgoingPackets();

            if (socketsReady == 0) {
                // No incoming data
                continue;
            }

            char remoteDataReceived[512];
            memset(remoteDataReceived, 0, sizeof(remoteDataReceived));
            int len = SDLNet_TCP_Recv(networkSocket, &remoteDataReceived, sizeof(remoteDataReceived));
            if (!len || !networkSocket || len == -1) {
                SPDLOG_ERROR("[Network] SDLNet_TCP_Recv: {}", SDLNet_GetError());
                break;
            }

            HandleRemoteData(remoteDataReceived);

            receivedData.append(remoteDataReceived, len);

            // Process all complete packets
            size_t delimiterPos = receivedData.find('\0');
            while (delimiterPos != std::string::npos) {
                std::string packet = receivedData.substr(0, delimiterPos);
                receivedData.erase(0, delimiterPos + 1);
                HandleRemoteJson(packet);
                delimiterPos = receivedData.find('\0');
            }
        }

        if (socketSet) {
            SDLNet_FreeSocketSet(socketSet);
        }

        if (isConnected) {
            SDLNet_TCP_Close(networkSocket);
            networkSocket = nullptr;
            isConnected = false;
            receivedData.clear();
            OnDisconnected();
            SPDLOG_INFO("[Network] Ending receiving thread...");
        }
    }
#endif
}

void Network::HandleRemoteData(char payload[512]) {
    OnIncomingData(payload);
}

void Network::HandleRemoteJson(std::string payload) {
    SPDLOG_DEBUG("[Network] Received json: {}", payload);
    nlohmann::json jsonPayload;
    try {
        jsonPayload = nlohmann::json::parse(payload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Failed to parse json: \n{}\n{}\n", payload, e.what());
        return;
    }

    try {
        OnIncomingJson(jsonPayload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Network] Exception handling incoming JSON: {}", e.what());
    } catch (...) { SPDLOG_ERROR("[Network] Unknown exception handling incoming JSON"); }
}
