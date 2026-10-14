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
    bool reconnectScheduled = false;
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
#endif
};

#endif // __cplusplus
