#ifndef NETWORK_WEBSOCKET_H
#define NETWORK_WEBSOCKET_H
#ifdef __cplusplus

#include <string>
#include <functional>
#include <queue>
#include <mutex>

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#else
// For native builds, we'll use a lightweight websocket implementation
// For now, declare the interface that can be backed by different implementations
#endif

/**
 * Cross-platform WebSocket wrapper.
 * On Emscripten, uses the browser's native WebSocket via emscripten/websocket.h.
 * On native platforms, can be backed by any C++ WebSocket library.
 */
class WebSocketClient {
  public:
    using MessageCallback = std::function<void(const std::string&)>;
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void()>;

    WebSocketClient();
    ~WebSocketClient();

    bool Connect(const std::string& url);
    void Disconnect();
    bool IsConnected() const;
    // Opening: created but neither open nor closed yet
    bool IsConnecting() const;
    void Send(const std::string& data);
    // emscripten_get_now() of the last sign of life: a message, or the start of the connection
    double GetLastActivityTime() const;
    // Bytes sent but not yet handed to the network
    size_t GetBufferedAmount();

    // Poll for received messages (call from main thread)
    bool Poll(std::string& outMessage);

    void SetOnConnect(ConnectCallback cb) { mOnConnect = cb; }
    void SetOnDisconnect(DisconnectCallback cb) { mOnDisconnect = cb; }

  private:
    bool mConnected = false;
    bool mConnecting = false;
    double mLastActivityTime = 0;
    std::queue<std::string> mIncomingQueue;
    std::mutex mQueueMutex;

    ConnectCallback mOnConnect;
    DisconnectCallback mOnDisconnect;

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_WEBSOCKET_T mSocket = 0;

    static EM_BOOL OnOpen(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
    static EM_BOOL OnMessage(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
    static EM_BOOL OnClose(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);
    static EM_BOOL OnError(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
#endif
};

#endif // __cplusplus
#endif // NETWORK_WEBSOCKET_H
