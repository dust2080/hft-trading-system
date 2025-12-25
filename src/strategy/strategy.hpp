#pragma once

#include "order_book.hpp"
#include "types.hpp"
#include <functional>
#include <string>
#include <vector>

namespace hft {

// Signal types
enum class SignalType {
    kNone,
    kBuy,
    kSell,
    kWarning
};

// Signal with details
struct Signal {
    SignalType type;
    std::string reason;
    double strength;      // 0.0 to 1.0
    Timestamp timestamp;
    
    Signal() : type(SignalType::kNone), strength(0.0), timestamp(0) {}
    Signal(SignalType t, const std::string& r, double s)
        : type(t), reason(r), strength(s), timestamp(NowNanos()) {}
};

// Callback for signals
using OnSignal = std::function<void(const Signal&)>;

/**
 * Base strategy interface.
 * All strategies inherit from this.
 */
class Strategy {
public:
    virtual ~Strategy() = default;
    
    // Called on every order book update
    virtual void OnOrderBookUpdate(const OrderBook& book) = 0;
    
    // Get strategy name
    virtual const std::string& GetName() const = 0;
    
    // Set signal callback
    void SetOnSignal(OnSignal callback) { on_signal_ = callback; }

protected:
    void EmitSignal(const Signal& signal) {
        if (on_signal_) {
            on_signal_(signal);
        }
    }
    
    OnSignal on_signal_;
};

/**
 * Spread Monitor Strategy
 * Monitors bid-ask spread and alerts on unusual widening.
 */
class SpreadMonitorStrategy : public Strategy {
public:
    explicit SpreadMonitorStrategy(double alert_threshold_pct = 0.05)
        : alert_threshold_pct_(alert_threshold_pct)
        , name_("SpreadMonitor") {}
    
    void OnOrderBookUpdate(const OrderBook& book) override {
        auto best_bid = book.GetBestBid();
        auto best_ask = book.GetBestAsk();
        
        if (!best_bid || !best_ask) return;
        
        Price spread = *best_ask - *best_bid;
        Price mid_price = (*best_bid + *best_ask) / 2;
        
        // Calculate spread as percentage of mid price
        // Both are fixed-point with 2 decimals, so this works
        double spread_pct = (static_cast<double>(spread) / static_cast<double>(mid_price)) * 100.0;
        
        // Update moving average
        UpdateSpreadAverage(spread_pct);
        
        // Check for unusual spread
        if (spread_avg_count_ >= 10) {  // Need enough samples
            double ratio = spread_pct / spread_avg_;
            
            if (ratio > (1.0 + alert_threshold_pct_) && !alert_active_) {
                alert_active_ = true;
                char buf[128];
                snprintf(buf, sizeof(buf), "Spread widened: %.4f%% (avg: %.4f%%)", 
                        spread_pct, spread_avg_);
                EmitSignal(Signal(SignalType::kWarning, buf, std::min(1.0, ratio - 1.0)));
            } else if (ratio < (1.0 + alert_threshold_pct_ / 2) && alert_active_) {
                alert_active_ = false;
                char buf[128];
                snprintf(buf, sizeof(buf), "Spread normalized: %.4f%%", spread_pct);
                EmitSignal(Signal(SignalType::kNone, buf, 0.0));
            }
        }
        
        last_spread_pct_ = spread_pct;
    }
    
    const std::string& GetName() const override { return name_; }
    
    // Getters for current state
    double GetCurrentSpreadPct() const { return last_spread_pct_; }
    double GetAverageSpreadPct() const { return spread_avg_; }
    bool IsAlertActive() const { return alert_active_; }

private:
    void UpdateSpreadAverage(double spread_pct) {
        // Exponential moving average
        if (spread_avg_count_ == 0) {
            spread_avg_ = spread_pct;
        } else {
            const double alpha = 0.1;  // Smoothing factor
            spread_avg_ = alpha * spread_pct + (1.0 - alpha) * spread_avg_;
        }
        spread_avg_count_++;
    }
    
    double alert_threshold_pct_;  // Alert when spread exceeds avg by this %
    std::string name_;
    
    double spread_avg_ = 0.0;
    int spread_avg_count_ = 0;
    double last_spread_pct_ = 0.0;
    bool alert_active_ = false;
};

/**
 * Imbalance Strategy
 * Generates signals based on order book imbalance.
 */
class ImbalanceStrategy : public Strategy {
public:
    explicit ImbalanceStrategy(double imbalance_threshold = 0.3, int depth = 5)
        : imbalance_threshold_(imbalance_threshold)
        , depth_(depth)
        , name_("Imbalance") {}
    
    void OnOrderBookUpdate(const OrderBook& book) override {
        auto bids = book.GetTopLevels(Side::kBuy, depth_);
        auto asks = book.GetTopLevels(Side::kSell, depth_);
        
        if (bids.empty() || asks.empty()) return;
        
        // Calculate total quantity on each side
        Quantity bid_qty = 0;
        Quantity ask_qty = 0;
        
        for (const auto& level : bids) {
            bid_qty += level.quantity;
        }
        for (const auto& level : asks) {
            ask_qty += level.quantity;
        }
        
        if (bid_qty == 0 && ask_qty == 0) return;
        
        // Calculate imbalance: positive = more bids, negative = more asks
        double total = static_cast<double>(bid_qty + ask_qty);
        double imbalance = static_cast<double>(bid_qty - ask_qty) / total;
        
        // Update state
        last_imbalance_ = imbalance;
        
        // Generate signals on significant imbalance
        if (imbalance > imbalance_threshold_ && last_signal_type_ != SignalType::kBuy) {
            last_signal_type_ = SignalType::kBuy;
            EmitSignal(Signal(
                SignalType::kBuy,
                "Bid imbalance: " + FormatPercent(imbalance) + " (buy pressure)",
                imbalance
            ));
        } else if (imbalance < -imbalance_threshold_ && last_signal_type_ != SignalType::kSell) {
            last_signal_type_ = SignalType::kSell;
            EmitSignal(Signal(
                SignalType::kSell,
                "Ask imbalance: " + FormatPercent(-imbalance) + " (sell pressure)",
                -imbalance
            ));
        } else if (std::abs(imbalance) < imbalance_threshold_ / 2 && last_signal_type_ != SignalType::kNone) {
            last_signal_type_ = SignalType::kNone;
            EmitSignal(Signal(
                SignalType::kNone,
                "Imbalance neutralized",
                0.0
            ));
        }
    }
    
    const std::string& GetName() const override { return name_; }
    
    double GetCurrentImbalance() const { return last_imbalance_; }

private:
    static std::string FormatPercent(double value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f%%", value * 100.0);
        return buf;
    }
    
    double imbalance_threshold_;
    int depth_;
    std::string name_;
    
    double last_imbalance_ = 0.0;
    SignalType last_signal_type_ = SignalType::kNone;
};

}  // namespace hft