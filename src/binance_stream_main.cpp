#include "binance_client.hpp"
#include "order_book.hpp"
#include "latency_stats.hpp"
#include "strategy.hpp"
#include <iostream>
#include <iomanip>
#include <csignal>
#include <atomic>
#include <deque>

using namespace hft;

std::atomic<bool> g_running{true};

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;
}

// Store recent signals for display
struct SignalLog {
    std::deque<std::pair<std::string, Signal>> recent_signals;
    std::mutex mutex;
    static constexpr size_t kMaxSignals = 5;
    
    void Add(const std::string& strategy_name, const Signal& signal) {
        std::lock_guard<std::mutex> lock(mutex);
        recent_signals.push_back({strategy_name, signal});
        while (recent_signals.size() > kMaxSignals) {
            recent_signals.pop_front();
        }
    }
};

void PrintOrderBook(const OrderBook& book, 
                    const LatencyStats& stats,
                    const SpreadMonitorStrategy& spread_strategy,
                    const ImbalanceStrategy& imbalance_strategy,
                    const SignalLog& signal_log) {
    std::cout << "\033[2J\033[H";  // Clear screen
    std::cout << "=== " << book.GetSymbol() << " Order Book + Strategy ===\n";
    std::cout << std::string(60, '-') << "\n";
    
    auto asks = book.GetTopLevels(Side::kSell, 10);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  ASK  " 
                  << std::setw(14) << SymbolConfig::FixedToString(it->price, 2)
                  << "  |  " 
                  << std::setw(14) << SymbolConfig::FixedToString(it->quantity, 8) 
                  << "\n";
    }
    
    std::cout << std::string(60, '=') << "\n";
    
    auto bids = book.GetTopLevels(Side::kBuy, 10);
    for (const auto& level : bids) {
        std::cout << "  BID  " 
                  << std::setw(14) << SymbolConfig::FixedToString(level.price, 2)
                  << "  |  " 
                  << std::setw(14) << SymbolConfig::FixedToString(level.quantity, 8) 
                  << "\n";
    }
    
    std::cout << std::string(60, '-') << "\n";
    
    // Market data
    auto spread = book.GetSpread();
    auto mid = book.GetMidPrice();
    
    if (spread) {
        std::cout << "Spread: " << SymbolConfig::FixedToString(*spread, 2) << " USDT";
    }
    if (mid) {
        std::cout << "  |  Mid: " << SymbolConfig::FixedToString(*mid, 2) << " USDT";
    }
    std::cout << "\n";
    
    std::cout << "Updates: " << book.GetUpdateCount() 
              << " | Levels: " << book.GetLevelCount(Side::kBuy) << "B / "
              << book.GetLevelCount(Side::kSell) << "A\n";
    
    // Strategy indicators
    std::cout << std::string(60, '-') << "\n";
    std::cout << "STRATEGY INDICATORS:\n";
    std::cout << std::fixed << std::setprecision(4);
    
    // Spread monitor
    std::cout << "  Spread: " << spread_strategy.GetCurrentSpreadPct() << "% "
              << "(avg: " << spread_strategy.GetAverageSpreadPct() << "%)";
    if (spread_strategy.IsAlertActive()) {
        std::cout << " [!!! WIDE !!!]";
    }
    std::cout << "\n";
    
    // Imbalance
    double imbalance = imbalance_strategy.GetCurrentImbalance();
    std::cout << "  Imbalance: " << std::setprecision(1) << (imbalance * 100) << "% ";
    if (imbalance > 0.1) {
        std::cout << "[BUY PRESSURE ↑]";
    } else if (imbalance < -0.1) {
        std::cout << "[SELL PRESSURE ↓]";
    } else {
        std::cout << "[NEUTRAL]";
    }
    std::cout << "\n";
    
    // Recent signals
    std::cout << std::string(60, '-') << "\n";
    std::cout << "RECENT SIGNALS:\n";
    {
        std::lock_guard<std::mutex> lock(const_cast<SignalLog&>(signal_log).mutex);
        if (signal_log.recent_signals.empty()) {
            std::cout << "  (none)\n";
        } else {
            for (const auto& [name, sig] : signal_log.recent_signals) {
                const char* type_str = "";
                switch (sig.type) {
                    case SignalType::kBuy: type_str = "[BUY]"; break;
                    case SignalType::kSell: type_str = "[SELL]"; break;
                    case SignalType::kWarning: type_str = "[WARN]"; break;
                    default: type_str = "[INFO]"; break;
                }
                std::cout << "  " << type_str << " " << name << ": " << sig.reason << "\n";
            }
        }
    }
    
    // Latency stats
    std::cout << std::string(60, '-') << "\n";
    auto latency = stats.Calculate();
    if (latency.count > 0) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Latency: " 
                  << "Mean=" << latency.mean_ns / 1000.0 << "μs"
                  << " | P99=" << latency.p99_ns / 1000.0 << "μs"
                  << " | Max=" << latency.max_ns / 1000.0 << "μs\n";
    }
    
    std::cout << "\nPress Ctrl+C to exit...\n";
}

int main(int argc, char* argv[]) {
    std::string symbol = "btcusdt";
    if (argc > 1) {
        symbol = argv[1];
    }
    
    std::cout << "Starting Binance stream with strategies for " << symbol << "...\n";
    
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    // Create components
    OrderBook book(symbol, 2, 8);
    LatencyStats latency_stats("Processing");
    SignalLog signal_log;
    
    // Create strategies
    SpreadMonitorStrategy spread_strategy(0.5);  // Alert if spread > 50% above average
    ImbalanceStrategy imbalance_strategy(0.3, 10);  // Alert if imbalance > 30%
    
    // Set up signal callbacks
    spread_strategy.SetOnSignal([&](const Signal& sig) {
        signal_log.Add(spread_strategy.GetName(), sig);
    });
    
    imbalance_strategy.SetOnSignal([&](const Signal& sig) {
        signal_log.Add(imbalance_strategy.GetName(), sig);
    });
    
    // Track synchronization
    int64_t last_update_id = 0;
    std::atomic<bool> synchronized{false};
    std::atomic<bool> connected{false};
    
    // Create client
    auto client = std::make_shared<BinanceClient>();
    client->SetSymbol(symbol);
    
    client->SetOnConnected([&]() {
        std::cout << "Connected to Binance WebSocket\n";
        connected = true;
    });
    
    client->SetOnDepthUpdate([&](const DepthUpdate& update) {
        if (!synchronized) {
            std::cout << "First update received, fetching snapshot...\n";
            try {
                auto snapshot = client->FetchDepthSnapshot(1000);
                last_update_id = snapshot.last_update_id;
                
                book.Clear();
                for (const auto& [price, qty] : snapshot.bids) {
                    book.UpdateFromStrings(Side::kBuy, price, qty);
                }
                for (const auto& [price, qty] : snapshot.asks) {
                    book.UpdateFromStrings(Side::kSell, price, qty);
                }
                
                synchronized = true;
                std::cout << "Synchronized! Starting strategies...\n\n";
            } catch (const std::exception& e) {
                std::cerr << "Failed to fetch snapshot: " << e.what() << "\n";
            }
            return;
        }
        
        if (update.final_update_id <= last_update_id) return;
        
        // Measure latency
        auto start_time = NowNanos();
        
        // Update order book
        for (const auto& [price, qty] : update.bids) {
            book.UpdateFromStrings(Side::kBuy, price, qty);
        }
        for (const auto& [price, qty] : update.asks) {
            book.UpdateFromStrings(Side::kSell, price, qty);
        }
        
        // Run strategies
        spread_strategy.OnOrderBookUpdate(book);
        imbalance_strategy.OnOrderBookUpdate(book);
        
        auto end_time = NowNanos();
        latency_stats.Record(end_time - start_time);
        
        last_update_id = update.final_update_id;
        
        // Print every 50 updates
        if (book.GetUpdateCount() % 50 == 0) {
            PrintOrderBook(book, latency_stats, spread_strategy, imbalance_strategy, signal_log);
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
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup
    client->Disconnect();
    
    // Print final statistics
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Final Statistics\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    std::cout << "Connection:\n";
    std::cout << "  Messages received: " << client->GetMessagesReceived() << "\n";
    std::cout << "  Bytes received: " << client->GetBytesReceived() << "\n";
    std::cout << "  Order book updates: " << book.GetUpdateCount() << "\n\n";
    
    std::cout << latency_stats.ToString() << "\n";
    
    return 0;
}