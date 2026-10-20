#pragma once

#ifdef __cplusplus

#include <thread>
#include <string>

#ifdef __EMSCRIPTEN__
#include "WebSocket.h"
#else
#include <SDL2/SDL_net.h>
#endif
#include <nlohmann/json.hpp>

class Network {
  private:
#ifdef __EMSCRIPTEN__
    WebSocketClient wsClient;
    void PollWebSocket();
    std::string wsUrl;
    int reconnectAttempts = 0;
    int reconnectTimer = 0;
    double reconnectTime = 0;
    void ScheduleReconnect();
#else
    IPaddress networkAddress;
    TCPsocket networkSocket;
    std::thread receiveThread;
#endif
    std::string receivedData;

    void ReceiveFromServer();
    void HandleRemoteData(char payload[512]);
    void HandleRemoteJson(std::string payload);

  public:
    bool isEnabled;
    bool isConnected;

    void Enable(const char* host, uint16_t port);
    void Disable();
    virtual void OnIncomingData(char payload[512]);
    virtual void OnIncomingJson(nlohmann::json payload);
    virtual void OnConnected();
    virtual void OnDisconnected();
    virtual void ProcessOutgoingPackets();
    void SendDataToRemote(const char* payload);
    virtual void SendJsonToRemote(nlohmann::json packet);

#ifdef __EMSCRIPTEN__
    // WebSocket-specific: connect to a WebSocket URL
    void EnableWebSocket(const std::string& url);
    // Poll for incoming WebSocket messages (call from game thread)
    void PollIncoming();
    // Connect straight away instead of waiting for the next scheduled attempt
    void ReconnectNow();
    // Call once the server has accepted the connection, so the next drop starts again from the shortest wait
    void ResetReconnectBackoff();
    int GetReconnectAttempts() const;
    // Milliseconds until the next scheduled attempt, 0 if none is scheduled
    double GetReconnectDelay() const;
#endif
};

#endif // __cplusplus
