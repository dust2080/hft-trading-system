#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "types.hpp"

namespace hft {

// Depth update from WebSocket stream
struct DepthUpdate {
    std::string symbol;
    int64_t first_update_id;
    int64_t final_update_id;
    std::vector<std::pair<std::string, std::string>> bids;  // [price, qty]
    std::vector<std::pair<std::string, std::string>> asks;
    
    static DepthUpdate Parse(const nlohmann::json& j) {
        DepthUpdate update;
        update.symbol = j.value("s", "");
        update.first_update_id = j.value("U", 0L);
        update.final_update_id = j.value("u", 0L);
        
        for (const auto& bid : j["b"]) {
            update.bids.emplace_back(bid[0].get<std::string>(), 
                                      bid[1].get<std::string>());
        }
        for (const auto& ask : j["a"]) {
            update.asks.emplace_back(ask[0].get<std::string>(), 
                                      ask[1].get<std::string>());
        }
        return update;
    }
};

// Depth snapshot from REST API
struct DepthSnapshot {
    int64_t last_update_id;
    std::vector<std::pair<std::string, std::string>> bids;
    std::vector<std::pair<std::string, std::string>> asks;
    
    static DepthSnapshot Parse(const nlohmann::json& j) {
        DepthSnapshot snapshot;
        snapshot.last_update_id = j.value("lastUpdateId", 0L);
        
        for (const auto& bid : j["bids"]) {
            snapshot.bids.emplace_back(bid[0].get<std::string>(), 
                                        bid[1].get<std::string>());
        }
        for (const auto& ask : j["asks"]) {
            snapshot.asks.emplace_back(ask[0].get<std::string>(), 
                                        ask[1].get<std::string>());
        }
        return snapshot;
    }
};

// Trade event from WebSocket
struct TradeEvent {
    std::string symbol;
    int64_t trade_id;
    std::string price;
    std::string quantity;
    int64_t trade_time;
    bool is_buyer_maker;
    
    static TradeEvent Parse(const nlohmann::json& j) {
        TradeEvent trade;
        trade.symbol = j.value("s", "");
        trade.trade_id = j.value("t", 0L);
        trade.price = j.value("p", "0");
        trade.quantity = j.value("q", "0");
        trade.trade_time = j.value("T", 0L);
        trade.is_buyer_maker = j.value("m", false);
        return trade;
    }
};

}  // namespace hft