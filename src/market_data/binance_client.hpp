#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "order_book.hpp"
#include "binance_messages.hpp"

namespace hft {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// Callback types
using OnDepthUpdate = std::function<void(const DepthUpdate&)>;
using OnTrade = std::function<void(const TradeEvent&)>;
using OnError = std::function<void(const std::string&)>;
using OnConnected = std::function<void()>;
using OnDisconnected = std::function<void()>;

/**
 * Binance WebSocket client for market data streaming.
 * Uses simdjson for fast JSON parsing.
 */
class BinanceClient : public std::enable_shared_from_this<BinanceClient> {
public:
    BinanceClient();
    ~BinanceClient();
    
    // Configuration
    void SetSymbol(const std::string& symbol);
    
    // Callbacks
    void SetOnDepthUpdate(OnDepthUpdate callback) { on_depth_update_ = callback; }
    void SetOnTrade(OnTrade callback) { on_trade_ = callback; }
    void SetOnError(OnError callback) { on_error_ = callback; }
    void SetOnConnected(OnConnected callback) { on_connected_ = callback; }
    void SetOnDisconnected(OnDisconnected callback) { on_disconnected_ = callback; }
    
    // Connection control
    void Connect();
    void Disconnect();
    bool IsConnected() const { return connected_; }
    
    // Fetch snapshot via REST API (blocking call)
    DepthSnapshot FetchDepthSnapshot(int limit = 1000);
    
    // Statistics
    uint64_t GetMessagesReceived() const { return messages_received_; }
    uint64_t GetBytesReceived() const { return bytes_received_; }

private:
    void RunIoContext();
    void DoConnect();
    void OnResolve(beast::error_code ec, tcp::resolver::results_type results);
    void OnConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void OnSslHandshake(beast::error_code ec);
    void OnHandshake(beast::error_code ec);
    void DoRead();
    void OnRead(beast::error_code ec, std::size_t bytes_transferred);
    void HandleMessage(const std::string& message);
    void DoClose();
    void OnClose(beast::error_code ec);
    
    // Configuration
    std::string symbol_ = "btcusdt";
    std::string host_ = "stream.binance.com";
    std::string port_ = "9443";
    
    // Boost.Asio components
    net::io_context ioc_;
    ssl::context ssl_ctx_{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;
    tcp::resolver resolver_;
    beast::flat_buffer buffer_;
    
    // Fast JSON parser (reused)
    FastJsonParser json_parser_;
    
    // Thread management
    std::thread io_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    
    // Callbacks
    OnDepthUpdate on_depth_update_;
    OnTrade on_trade_;
    OnError on_error_;
    OnConnected on_connected_;
    OnDisconnected on_disconnected_;
    
    // Statistics
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> bytes_received_{0};
};

}  // namespace hft