#include "binance_client.hpp"
#include "order_book.hpp"
#include "latency_stats.hpp"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <atomic>

using namespace hft;

std::atomic<bool> g_running{true};

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;
}

void PrintOrderBook(const OrderBook& book, const LatencyStats& stats) {
    std::cout << "\033[2J\033[H";  // Clear screen
    std::cout << "=== " << book.GetSymbol() << " Order Book (Live) ===\n";
    std::cout << std::string(55, '-') << "\n";
    
    auto asks = book.GetTopLevels(Side::kSell, 10);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  ASK  " 
                  << std::setw(14) << SymbolConfig::FixedToString(it->price, 2)
                  << "  |  " 
                  << std::setw(14) << SymbolConfig::FixedToString(it->quantity, 8) 
                  << "\n";
    }
    
    std::cout << std::string(55, '=') << "\n";
    
    auto bids = book.GetTopLevels(Side::kBuy, 10);
    for (const auto& level : bids) {
        std::cout << "  BID  " 
                  << std::setw(14) << SymbolConfig::FixedToString(level.price, 2)
                  << "  |  " 
                  << std::setw(14) << SymbolConfig::FixedToString(level.quantity, 8) 
                  << "\n";
    }
    
    std::cout << std::string(55, '-') << "\n";
    
    auto spread = book.GetSpread();
    auto mid = book.GetMidPrice();
    
    if (spread) {
        std::cout << "Spread: " << SymbolConfig::FixedToString(*spread, 2) << " USDT\n";
    }
    if (mid) {
        std::cout << "Mid:    " << SymbolConfig::FixedToString(*mid, 2) << " USDT\n";
    }
    
    std::cout << "Updates: " << book.GetUpdateCount() 
              << " | Levels: " << book.GetLevelCount(Side::kBuy) << " bids, "
              << book.GetLevelCount(Side::kSell) << " asks\n";
    
    // Show latency stats
    auto latency = stats.Calculate();
    if (latency.count > 0) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\nProcessing Latency (per update batch):\n";
        std::cout << "  Mean: " << latency.mean_ns / 1000.0 << " μs"
                  << "  |  P99: " << latency.p99_ns / 1000.0 << " μs"
                  << "  |  Max: " << latency.max_ns / 1000.0 << " μs\n";
    }
    
    std::cout << "\nPress Ctrl+C to exit...\n";
}

int main(int argc, char* argv[]) {
    std::string symbol = "btcusdt";
    if (argc > 1) {
        symbol = argv[1];
    }
    
    std::cout << "Starting Binance stream for " << symbol << "...\n";
    
    // Set up signal handler
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    // Create order book
    OrderBook book(symbol, 2, 8);
    
    // Create latency stats collector
    LatencyStats latency_stats("Processing");
    
    // Track last update ID for synchronization
    int64_t last_update_id = 0;
    std::atomic<bool> synchronized{false};
    std::atomic<bool> connected{false};
    
    // Create client using make_shared (required for shared_from_this)
    auto client = std::make_shared<BinanceClient>();
    client->SetSymbol(symbol);
    
    // Set up callbacks
    client->SetOnConnected([&]() {
        std::cout << "Connected to Binance WebSocket\n";
        connected = true;
    });
    
    client->SetOnDepthUpdate([&](const DepthUpdate& update) {
        // First update: fetch snapshot
        if (!synchronized) {
            std::cout << "First update received, fetching snapshot...\n";
            try {
                auto snapshot = client->FetchDepthSnapshot(1000);
                last_update_id = snapshot.last_update_id;
                
                // Apply snapshot to order book
                book.Clear();
                for (const auto& [price, qty] : snapshot.bids) {
                    book.UpdateFromStrings(Side::kBuy, price, qty);
                }
                for (const auto& [price, qty] : snapshot.asks) {
                    book.UpdateFromStrings(Side::kSell, price, qty);
                }
                
                synchronized = true;
                std::cout << "Synchronized! Last update ID: " << last_update_id << "\n";
                std::cout << "Order book has " << book.GetLevelCount(Side::kBuy) 
                          << " bids, " << book.GetLevelCount(Side::kSell) << " asks\n\n";
            } catch (const std::exception& e) {
                std::cerr << "Failed to fetch snapshot: " << e.what() << "\n";
            }
            return;
        }
        
        // Skip old updates
        if (update.final_update_id <= last_update_id) return;
        
        // ====== START LATENCY MEASUREMENT ======
        auto start_time = NowNanos();
        
        // Apply updates
        for (const auto& [price, qty] : update.bids) {
            book.UpdateFromStrings(Side::kBuy, price, qty);
        }
        for (const auto& [price, qty] : update.asks) {
            book.UpdateFromStrings(Side::kSell, price, qty);
        }
        
        // ====== END LATENCY MEASUREMENT ======
        auto end_time = NowNanos();
        latency_stats.Record(end_time - start_time);
        
        last_update_id = update.final_update_id;
        
        // Print order book every 50 updates
        if (book.GetUpdateCount() % 50 == 0) {
            PrintOrderBook(book, latency_stats);
        }
    });
    
    client->SetOnError([](const std::string& error) {
        std::cerr << "Error: " << error << "\n";
    });
    
    client->SetOnDisconnected([&]() {
        std::cout << "Disconnected from Binance\n";
        connected = false;
    });
    
    // Connect
    client->Connect();
    
    // Wait for connection or timeout
    std::cout << "Connecting...\n";
    int timeout_count = 0;
    while (!connected && g_running && timeout_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout_count++;
    }
    
    if (!connected) {
        std::cerr << "Failed to connect within 10 seconds\n";
        client->Disconnect();
        return 1;
    }
    
    // Main loop - keep running while connected
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup
    client->Disconnect();
    
    // Print final statistics
    std::cout << "\n" << std::string(55, '=') << "\n";
    std::cout << "Final Statistics\n";
    std::cout << std::string(55, '=') << "\n\n";
    
    std::cout << "Connection:\n";
    std::cout << "  Messages received: " << client->GetMessagesReceived() << "\n";
    std::cout << "  Bytes received: " << client->GetBytesReceived() << "\n";
    std::cout << "  Order book updates: " << book.GetUpdateCount() << "\n\n";
    
    std::cout << latency_stats.ToString() << "\n";
    
    return 0;
}