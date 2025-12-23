#include "order_book.hpp"
#include <iostream>
#include <iomanip>

using namespace hft;

void PrintOrderBook(const OrderBook& book) {
    std::cout << "\n=== " << book.GetSymbol() << " Order Book ===\n";
    std::cout << std::string(45, '-') << "\n";

    // Print asks (reversed so lowest price at bottom)
    std::cout << "ASKS:\n";
    auto asks = book.GetTopLevels(Side::kSell, 5);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  " 
                  << std::setw(12) << SymbolConfig::FixedToString(it->price, 2)
                  << "  |  " 
                  << SymbolConfig::FixedToString(it->quantity, 8) << "\n";
    }

    std::cout << std::string(45, '-') << "\n";

    // Print bids (highest price first)
    std::cout << "BIDS:\n";
    auto bids = book.GetTopLevels(Side::kBuy, 5);
    for (const auto& level : bids) {
        std::cout << "  " 
                  << std::setw(12) << SymbolConfig::FixedToString(level.price, 2)
                  << "  |  " 
                  << SymbolConfig::FixedToString(level.quantity, 8) << "\n";
    }

    std::cout << std::string(45, '-') << "\n\n";

    // Print summary
    auto best_bid = book.GetBestBid();
    auto best_ask = book.GetBestAsk();
    auto spread = book.GetSpread();
    auto mid = book.GetMidPrice();

    if (best_bid) {
        std::cout << "Best Bid:  " << SymbolConfig::FixedToString(*best_bid, 2) << "\n";
    }
    if (best_ask) {
        std::cout << "Best Ask:  " << SymbolConfig::FixedToString(*best_ask, 2) << "\n";
    }
    if (spread) {
        std::cout << "Spread:    " << SymbolConfig::FixedToString(*spread, 2) << "\n";
    }
    if (mid) {
        std::cout << "Mid Price: " << SymbolConfig::FixedToString(*mid, 2) << "\n";
    }

    std::cout << "\nLevels: " << book.GetLevelCount(Side::kBuy) << " bids, "
              << book.GetLevelCount(Side::kSell) << " asks\n";
    std::cout << "Updates: " << book.GetUpdateCount() << "\n";
}

int main() {
    std::cout << "=== HFT Trading System Demo ===\n";

    // Create order book for BTCUSDT
    // Price: 2 decimal places (e.g., 30000.50)
    // Quantity: 8 decimal places (satoshi precision)
    OrderBook book("BTCUSDT", 2, 8);

    // Simulate Binance depth snapshot
    // Bids
    book.UpdateFromStrings(Side::kBuy, "30000.00", "1.50000000");
    book.UpdateFromStrings(Side::kBuy, "29999.50", "2.30000000");
    book.UpdateFromStrings(Side::kBuy, "29999.00", "0.80000000");
    book.UpdateFromStrings(Side::kBuy, "29998.00", "5.00000000");
    book.UpdateFromStrings(Side::kBuy, "29997.50", "3.20000000");

    // Asks
    book.UpdateFromStrings(Side::kSell, "30001.00", "0.75000000");
    book.UpdateFromStrings(Side::kSell, "30001.50", "1.20000000");
    book.UpdateFromStrings(Side::kSell, "30002.00", "3.50000000");
    book.UpdateFromStrings(Side::kSell, "30003.00", "2.00000000");
    book.UpdateFromStrings(Side::kSell, "30005.00", "4.10000000");

    PrintOrderBook(book);

    // Simulate market update
    std::cout << "\n=== Simulating Market Update ===\n";
    std::cout << "Best bid (30000.00) gets filled...\n";
    book.UpdateFromStrings(Side::kBuy, "30000.00", "0");

    auto best_bid = book.GetBestBid();
    if (best_bid) {
        std::cout << "New Best Bid: " << SymbolConfig::FixedToString(*best_bid, 2) << "\n";
    }

    std::cout << "\nNew aggressive bid at 30000.75...\n";
    book.UpdateFromStrings(Side::kBuy, "30000.75", "2.00000000");

    PrintOrderBook(book);

    return 0;
}