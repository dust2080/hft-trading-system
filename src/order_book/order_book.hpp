#pragma once

#include "types.hpp"
#include <unordered_map>
#include <set>
#include <vector>
#include <optional>
#include <functional>

namespace hft {

/**
 * High-performance order book for market data tracking.
 * 
 * Design goals:
 * - update(): O(log N) worst case
 * - getBestBid/Ask(): O(1) with caching
 * - getQuantityAt(): O(1) hash lookup
 * 
 * This is a "market data" order book that tracks aggregate quantities
 * at each price level, as received from exchange feeds (e.g., Binance).
 * It does NOT perform order matching - that happens on the exchange.
 * 
 * Future optimizations:
 * - Memory pool to avoid dynamic allocation
 * - Cache-friendly data layout
 * - Lock-free updates for multi-threaded access
 */
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol, 
                       int price_decimals = 2, 
                       int quantity_decimals = 8);
    
    // === Core Operations ===

    /**
     * Update quantity at a price level.
     * If quantity is 0, the price level is removed.
     * This is the hot path - must be as fast as possible.
     */
    void Update(Side side, Price price, Quantity quantity);

    /**
     * Update from string format (for Binance API compatibility).
     * Converts strings to fixed-point integers internally.
     */
    void UpdateFromStrings(Side side, 
                           const std::string& price_str,
                           const std::string& quantity_str);

    /**
     * Clear all price levels (e.g., when receiving a new snapshot).
     */
    void Clear();

    /**
     * Clear only one side of the book.
     */
    void Clear(Side side);

    // === Query Operations ===

    std::optional<Price> GetBestBid() const;
    std::optional<Price> GetBestAsk() const;

    /**
     * Get spread = best_ask - best_bid.
     * Returns nullopt if either side is empty.
     */
    std::optional<Price> GetSpread() const;

    /**
     * Get mid price = (best_bid + best_ask) / 2.
     */
    std::optional<Price> GetMidPrice() const;

    /**
     * Get quantity at a specific price level.
     * Returns 0 if price level doesn't exist.
     */
    Quantity GetQuantityAt(Side side, Price price) const;

    /**
     * Get top N price levels.
     * Bids are returned highest price first.
     * Asks are returned lowest price first.
     */
    std::vector<PriceLevel> GetTopLevels(Side side, size_t n) const;

    /**
     * Get number of active price levels on one side.
     */
    size_t GetLevelCount(Side side) const;

    // === Statistics ===

    uint64_t GetUpdateCount() const { return update_count_; }
    const std::string& GetSymbol() const { return symbol_; }
    int GetPriceDecimals() const { return price_decimals_; }
    int GetQuantityDecimals() const { return quantity_decimals_; }

private:
    void UpdateBid(Price price, Quantity quantity);
    void UpdateAsk(Price price, Quantity quantity);
    void InvalidateCache();
    void RefreshCache() const;

    std::string symbol_;
    int price_decimals_;
    int quantity_decimals_;

    // Bid side: price -> quantity, with sorted price set (descending)
    std::unordered_map<Price, Quantity> bids_;
    std::set<Price, std::greater<Price>> bid_prices_;

    // Ask side: price -> quantity, with sorted price set (ascending)
    std::unordered_map<Price, Quantity> asks_;
    std::set<Price> ask_prices_;

    // Cached best prices for O(1) access
    mutable std::optional<Price> cached_best_bid_;
    mutable std::optional<Price> cached_best_ask_;
    mutable bool cache_valid_ = false;

    uint64_t update_count_ = 0;
};

}  // namespace hft