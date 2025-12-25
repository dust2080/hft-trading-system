#include "binance_client.hpp"
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>  // Keep for REST API (simpler)

namespace hft {

namespace http = beast::http;

BinanceClient::BinanceClient()
    : resolver_(net::make_strand(ioc_)) {
    ssl_ctx_.set_default_verify_paths();
    ssl_ctx_.set_verify_mode(ssl::verify_peer);
}

BinanceClient::~BinanceClient() {
    Disconnect();
}

void BinanceClient::SetSymbol(const std::string& symbol) {
    symbol_ = symbol;
    for (char& c : symbol_) {
        c = std::tolower(c);
    }
}

void BinanceClient::Connect() {
    if (running_) return;
    
    running_ = true;
    
    ws_ = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(
        net::make_strand(ioc_), ssl_ctx_);
    
    DoConnect();
    
    io_thread_ = std::thread([this]() { RunIoContext(); });
}

void BinanceClient::Disconnect() {
    if (!running_) return;
    
    running_ = false;
    
    if (connected_) {
        DoClose();
    }
    
    ioc_.stop();
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    ioc_.restart();
}

void BinanceClient::RunIoContext() {
    try {
        ioc_.run();
    } catch (const std::exception& e) {
        if (on_error_) {
            on_error_(std::string("I/O error: ") + e.what());
        }
    }
}

void BinanceClient::DoConnect() {
    resolver_.async_resolve(
        host_,
        port_,
        beast::bind_front_handler(&BinanceClient::OnResolve, shared_from_this())
    );
}

void BinanceClient::OnResolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        if (on_error_) on_error_("Resolve failed: " + ec.message());
        return;
    }
    
    net::async_connect(
        beast::get_lowest_layer(*ws_),
        results,
        beast::bind_front_handler(&BinanceClient::OnConnect, shared_from_this())
    );
}

void BinanceClient::OnConnect(beast::error_code ec, 
                               [[maybe_unused]] tcp::resolver::results_type::endpoint_type ep) {
    if (ec) {
        if (on_error_) on_error_("Connect failed: " + ec.message());
        return;
    }
    
    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
        if (on_error_) on_error_("SSL SNI failed");
        return;
    }
    
    ws_->next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&BinanceClient::OnSslHandshake, shared_from_this())
    );
}

void BinanceClient::OnSslHandshake(beast::error_code ec) {
    if (ec) {
        if (on_error_) on_error_("SSL handshake failed: " + ec.message());
        return;
    }
    
    ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_->set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(http::field::user_agent, "hft-trading-system/1.0");
    }));
    
    std::string path = "/ws/" + symbol_ + "@depth@100ms";
    
    ws_->async_handshake(
        host_,
        path,
        beast::bind_front_handler(&BinanceClient::OnHandshake, shared_from_this())
    );
}

void BinanceClient::OnHandshake(beast::error_code ec) {
    if (ec) {
        if (on_error_) on_error_("WebSocket handshake failed: " + ec.message());
        return;
    }
    
    connected_ = true;
    
    if (on_connected_) {
        on_connected_();
    }
    
    DoRead();
}

void BinanceClient::DoRead() {
    if (!running_) return;
    
    ws_->async_read(
        buffer_,
        beast::bind_front_handler(&BinanceClient::OnRead, shared_from_this())
    );
}

void BinanceClient::OnRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            connected_ = false;
            if (on_disconnected_) on_disconnected_();
        } else if (running_) {
            if (on_error_) on_error_("Read error: " + ec.message());
        }
        return;
    }
    
    bytes_received_ += bytes_transferred;
    messages_received_++;
    
    std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    
    HandleMessage(message);
    
    DoRead();
}

void BinanceClient::HandleMessage(const std::string& message) {
    // Use simdjson for fast parsing
    DepthUpdate update;
    if (json_parser_.ParseDepthUpdate(message, update)) {
        if (on_depth_update_) {
            on_depth_update_(update);
        }
    }
}

void BinanceClient::DoClose() {
    if (!ws_) return;
    
    beast::error_code ec;
    ws_->close(websocket::close_code::normal, ec);
}

void BinanceClient::OnClose([[maybe_unused]] beast::error_code ec) {
    connected_ = false;
    if (on_disconnected_) {
        on_disconnected_();
    }
}

DepthSnapshot BinanceClient::FetchDepthSnapshot(int limit) {
    // Use nlohmann/json for REST (simpler, not performance critical)
    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    
    tcp::resolver resolver(ioc);
    beast::ssl_stream<tcp::socket> stream(ioc, ctx);
    
    SSL_set_tlsext_host_name(stream.native_handle(), "api.binance.com");
    
    auto const results = resolver.resolve("api.binance.com", "443");
    net::connect(beast::get_lowest_layer(stream), results);
    stream.handshake(ssl::stream_base::client);
    
    std::string upper_symbol = symbol_;
    for (char& c : upper_symbol) {
        c = std::toupper(c);
    }
    std::string target = "/api/v3/depth?symbol=" + upper_symbol + "&limit=" + std::to_string(limit);
    
    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, "api.binance.com");
    req.set(http::field::user_agent, "hft-trading-system/1.0");
    
    http::write(stream, req);
    
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);
    
    // Parse with simdjson
    DepthSnapshot snapshot;
    json_parser_.ParseDepthSnapshot(res.body(), snapshot);
    
    beast::error_code ec;
    stream.shutdown(ec);
    
    return snapshot;
}

}  // namespace hft