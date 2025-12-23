#include "order_book.hpp"

namespace hft {

OrderBook::OrderBook(const std::string& symbol, 
                     int price_decimals, 
                     int quantity_decimals)
    : symbol_(symbol)
    , price_decimals_(price_decimals)
    , quantity_decimals_(quantity_decimals) {}

void OrderBook::Update(Side side, Price price, Quantity quantity) {
    ++update_count_;
    InvalidateCache();

    if (side == Side::kBuy) {
        UpdateBid(price, quantity);
    } else {
        UpdateAsk(price, quantity);
    }
}

void OrderBook::UpdateFromStrings(Side side,
                                  const std::string& price_str,
                                  const std::string& quantity_str) {
    Price price = SymbolConfig::StringToFixed(price_str, price_decimals_);
    Quantity qty = SymbolConfig::StringToFixed(quantity_str, quantity_decimals_);
    Update(side, price, qty);
}

void OrderBook::UpdateBid(Price price, Quantity quantity) {
    if (quantity == 0) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            bids_.erase(it);
            bid_prices_.erase(price);
        }
    } else {
        auto [it, inserted] = bids_.insert_or_assign(price, quantity);
        if (inserted) {
            bid_prices_.insert(price);
        }
    }
}

void OrderBook::UpdateAsk(Price price, Quantity quantity) {
    if (quantity == 0) {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            asks_.erase(it);
            ask_prices_.erase(price);
        }
    } else {
        auto [it, inserted] = asks_.insert_or_assign(price, quantity);
        if (inserted) {
            ask_prices_.insert(price);
        }
    }
}

void OrderBook::Clear() {
    bids_.clear();
    asks_.clear();
    bid_prices_.clear();
    ask_prices_.clear();
    InvalidateCache();
}

void OrderBook::Clear(Side side) {
    if (side == Side::kBuy) {
        bids_.clear();
        bid_prices_.clear();
    } else {
        asks_.clear();
        ask_prices_.clear();
    }
    InvalidateCache();
}

void OrderBook::InvalidateCache() {
    cache_valid_ = false;
}

void OrderBook::RefreshCache() const {
    if (cache_valid_) return;

    cached_best_bid_ = bid_prices_.empty() 
        ? std::nullopt 
        : std::optional<Price>(*bid_prices_.begin());

    cached_best_ask_ = ask_prices_.empty() 
        ? std::nullopt 
        : std::optional<Price>(*ask_prices_.begin());

    cache_valid_ = true;
}

std::optional<Price> OrderBook::GetBestBid() const {
    RefreshCache();
    return cached_best_bid_;
}

std::optional<Price> OrderBook::GetBestAsk() const {
    RefreshCache();
    return cached_best_ask_;
}

std::optional<Price> OrderBook::GetSpread() const {
    auto bid = GetBestBid();
    auto ask = GetBestAsk();
    if (bid && ask) {
        return *ask - *bid;
    }
    return std::nullopt;
}

std::optional<Price> OrderBook::GetMidPrice() const {
    auto bid = GetBestBid();
    auto ask = GetBestAsk();
    if (bid && ask) {
        return (*bid + *ask) / 2;
    }
    return std::nullopt;
}

Quantity OrderBook::GetQuantityAt(Side side, Price price) const {
    const auto& map = (side == Side::kBuy) ? bids_ : asks_;
    auto it = map.find(price);
    return (it != map.end()) ? it->second : 0;
}

std::vector<PriceLevel> OrderBook::GetTopLevels(Side side, size_t n) const {
    std::vector<PriceLevel> result;
    result.reserve(n);

    if (side == Side::kBuy) {
        for (Price price : bid_prices_) {
            if (result.size() >= n) break;
            result.emplace_back(price, bids_.at(price));
        }
    } else {
        for (Price price : ask_prices_) {
            if (result.size() >= n) break;
            result.emplace_back(price, asks_.at(price));
        }
    }

    return result;
}

size_t OrderBook::GetLevelCount(Side side) const {
    return (side == Side::kBuy) ? bid_prices_.size() : ask_prices_.size();
}

}  // namespace hft