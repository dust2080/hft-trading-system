#pragma once

#include <string>
#include <vector>
#include <simdjson.h>
#include "types.hpp"

namespace hft {

// Depth update from WebSocket stream
struct DepthUpdate {
    std::string symbol;
    int64_t first_update_id;
    int64_t final_update_id;
    std::vector<std::pair<std::string, std::string>> bids;
    std::vector<std::pair<std::string, std::string>> asks;
};

// Depth snapshot from REST API
struct DepthSnapshot {
    int64_t last_update_id;
    std::vector<std::pair<std::string, std::string>> bids;
    std::vector<std::pair<std::string, std::string>> asks;
};

// Trade event from WebSocket
struct TradeEvent {
    std::string symbol;
    int64_t trade_id;
    std::string price;
    std::string quantity;
    int64_t trade_time;
    bool is_buyer_maker;
};

/**
 * Fast JSON parser using simdjson.
 * Reuses parser and padded_string to avoid allocation.
 */
class FastJsonParser {
public:
    FastJsonParser() = default;
    
    // Parse depth update from WebSocket
    bool ParseDepthUpdate(const std::string& json, DepthUpdate& update) {
        padded_ = simdjson::padded_string(json);
        auto doc = parser_.iterate(padded_);
        if (doc.error()) return false;
        
        // Get event type
        std::string_view event_type;
        auto event_result = doc["e"].get_string();
        if (event_result.error()) return false;
        event_type = event_result.value();
        if (event_type != "depthUpdate") return false;
        
        // Get symbol
        auto symbol_result = doc["s"].get_string();
        if (symbol_result.error()) return false;
        update.symbol = std::string(symbol_result.value());
        
        // Get update IDs
        auto first_id_result = doc["U"].get_int64();
        if (first_id_result.error()) return false;
        update.first_update_id = first_id_result.value();
        
        auto final_id_result = doc["u"].get_int64();
        if (final_id_result.error()) return false;
        update.final_update_id = final_id_result.value();
        
        // Parse bids
        update.bids.clear();
        auto bids_result = doc["b"].get_array();
        if (!bids_result.error()) {
            for (auto bid : bids_result.value()) {
                auto bid_arr = bid.get_array();
                if (bid_arr.error()) continue;
                
                auto it = bid_arr.begin();
                auto price_result = (*it).get_string();
                ++it;
                auto qty_result = (*it).get_string();
                
                if (!price_result.error() && !qty_result.error()) {
                    update.bids.emplace_back(
                        std::string(price_result.value()),
                        std::string(qty_result.value())
                    );
                }
            }
        }
        
        // Parse asks
        update.asks.clear();
        auto asks_result = doc["a"].get_array();
        if (!asks_result.error()) {
            for (auto ask : asks_result.value()) {
                auto ask_arr = ask.get_array();
                if (ask_arr.error()) continue;
                
                auto it = ask_arr.begin();
                auto price_result = (*it).get_string();
                ++it;
                auto qty_result = (*it).get_string();
                
                if (!price_result.error() && !qty_result.error()) {
                    update.asks.emplace_back(
                        std::string(price_result.value()),
                        std::string(qty_result.value())
                    );
                }
            }
        }
        
        return true;
    }
    
    // Parse depth snapshot from REST API
    bool ParseDepthSnapshot(const std::string& json, DepthSnapshot& snapshot) {
        padded_ = simdjson::padded_string(json);
        auto doc = parser_.iterate(padded_);
        if (doc.error()) return false;
        
        // Get last update ID
        auto id_result = doc["lastUpdateId"].get_int64();
        if (id_result.error()) return false;
        snapshot.last_update_id = id_result.value();
        
        // Parse bids
        snapshot.bids.clear();
        auto bids_result = doc["bids"].get_array();
        if (!bids_result.error()) {
            for (auto bid : bids_result.value()) {
                auto bid_arr = bid.get_array();
                if (bid_arr.error()) continue;
                
                auto it = bid_arr.begin();
                auto price_result = (*it).get_string();
                ++it;
                auto qty_result = (*it).get_string();
                
                if (!price_result.error() && !qty_result.error()) {
                    snapshot.bids.emplace_back(
                        std::string(price_result.value()),
                        std::string(qty_result.value())
                    );
                }
            }
        }
        
        // Parse asks
        snapshot.asks.clear();
        auto asks_result = doc["asks"].get_array();
        if (!asks_result.error()) {
            for (auto ask : asks_result.value()) {
                auto ask_arr = ask.get_array();
                if (ask_arr.error()) continue;
                
                auto it = ask_arr.begin();
                auto price_result = (*it).get_string();
                ++it;
                auto qty_result = (*it).get_string();
                
                if (!price_result.error() && !qty_result.error()) {
                    snapshot.asks.emplace_back(
                        std::string(price_result.value()),
                        std::string(qty_result.value())
                    );
                }
            }
        }
        
        return true;
    }

private:
    simdjson::ondemand::parser parser_;
    simdjson::padded_string padded_;
};

}  // namespace hft