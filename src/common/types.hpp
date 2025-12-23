#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace hft {

// Price stored as integer to avoid floating point issues
// e.g., 30000.50 USDT -> 3000050 (multiplied by 100)
using Price = int64_t;

// Quantity stored as integer
// e.g., 1.5 BTC -> 150000000 (multiplied by 10^8, satoshi precision)
using Quantity = int64_t;

using Timestamp = int64_t;

enum class Side : uint8_t {
    kBuy = 0,
    kSell = 1
};

// Single price level in market depth
struct PriceLevel {
    Price price;
    Quantity quantity;

    PriceLevel() : price(0), quantity(0) {}
    PriceLevel(Price p, Quantity q) : price(p), quantity(q) {}
};

// Configuration for price/quantity conversion
struct SymbolConfig {
    int price_decimals;
    int quantity_decimals;

    // Convert string to integer price
    // "30000.50" with decimals=2 -> 3000050
    static int64_t StringToFixed(const std::string& s, int decimals) {
        int64_t result = 0;
        bool found_dot = false;
        int decimal_count = 0;

        for (char c : s) {
            if (c == '.') {
                found_dot = true;
            } else if (c >= '0' && c <= '9') {
                result = result * 10 + (c - '0');
                if (found_dot) {
                    ++decimal_count;
                    if (decimal_count >= decimals) break;
                }
            }
        }

        while (decimal_count < decimals) {
            result *= 10;
            ++decimal_count;
        }

        return result;
    }

    // Convert integer price back to string
    // 3000050 with decimals=2 -> "30000.50"
    static std::string FixedToString(int64_t value, int decimals) {
        if (value == 0) return "0." + std::string(decimals, '0');

        std::string s = std::to_string(value);
        
        while (static_cast<int>(s.length()) <= decimals) {
            s = "0" + s;
        }

        s.insert(s.length() - decimals, ".");
        return s;
    }
};

// Get current timestamp in nanoseconds
inline Timestamp NowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

inline const char* SideToString(Side side) {
    return side == Side::kBuy ? "BUY" : "SELL";
}

}  // namespace hft