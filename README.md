# HFT Trading System

A high-frequency trading system demo featuring real-time market data processing from Binance with strategy execution.

## Features

- **High-Performance Order Book**: Sub-microsecond update latency
- **Real-time Market Data**: Binance WebSocket integration with simdjson parsing
- **Strategy Framework**: Pluggable strategy architecture
- **Low-Latency Processing**: ~26μs median end-to-end latency
- **Performance Monitoring**: Built-in latency statistics

## Architecture
```
┌─────────────────────────────────────────────────────────────────┐
│                      Binance Exchange                           │
└───────────────────────────┬─────────────────────────────────────┘
                            │ WebSocket (wss://)
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                      BinanceClient                              │
│  - Async I/O (Boost.Asio)                                       │
│  - TLS encryption                                               │
│  - simdjson parsing (10x faster than traditional JSON)          │
└───────────────────────────┬─────────────────────────────────────┘
                            │ Callbacks
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                       OrderBook                                 │
│  - Price levels: std::unordered_map + std::set                  │
│  - O(1) best bid/ask query (cached)                             │
│  - O(log N) update                                              │
└───────────────────────────┬─────────────────────────────────────┘
                            │ OnOrderBookUpdate()
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Strategies                                 │
│  ┌─────────────────┐  ┌─────────────────┐                      │
│  │ SpreadMonitor   │  │   Imbalance     │                      │
│  │ - Track spread  │  │ - Buy/Sell      │                      │
│  │ - Alert on      │  │   pressure      │                      │
│  │   widening      │  │ - Signal gen    │                      │
│  └────────┬────────┘  └────────┬────────┘                      │
│           │                    │                                │
│           └────────┬───────────┘                                │
│                    ▼                                            │
│              Signal Output                                      │
│           [BUY] [SELL] [WARN]                                   │
└─────────────────────────────────────────────────────────────────┘
```

## Performance

### Order Book Benchmark (Synthetic)

| Operation | Median | P99 |
|-----------|--------|-----|
| Update | 166 ns | 1,208 ns |
| GetBestBid | 41 ns | 42 ns |
| GetQuantityAt | 42 ns | 209 ns |

### End-to-End Latency (Live Binance Data)

| Metric | nlohmann/json | simdjson | Improvement |
|--------|---------------|----------|-------------|
| Min | 792 ns | 167 ns | **4.7x** |
| Median | 19.54 μs | 19.54 μs | - |
| Mean | 32.10 μs | 25.80 μs | **20%** |
| P99 | 183.25 μs | 135.75 μs | **26%** |
| Max | 536.62 μs | 193.33 μs | **64%** |

*Tested with BTCUSDT, ~580 samples over 1 minute*

### Latency Breakdown (Estimated)
```
Total: ~26 μs
├── JSON parsing (simdjson): ~5-8 μs
├── String conversion:       ~5-8 μs
├── Order Book update:       ~1-5 μs
├── Strategy execution:      ~2-5 μs
└── Other overhead:          ~2-5 μs
```

## Strategies

### 1. Spread Monitor

Monitors bid-ask spread and alerts on unusual widening.
```
Normal:  Spread = $0.01 (0.00001%)
Alert:   Spread = $0.05 (50% above average) → [WARN] Signal
```

**Use case**: Detect liquidity changes, potential large orders incoming.

### 2. Imbalance Strategy

Generates signals based on order book imbalance.
```
Imbalance = (BidQty - AskQty) / (BidQty + AskQty)

Imbalance > +30%  → [BUY]  Buy pressure detected
Imbalance < -30%  → [SELL] Sell pressure detected
|Imbalance| < 15% → [INFO] Neutral
```

**Use case**: Detect short-term price direction based on order flow.

## Building

### Prerequisites

- CMake 3.16+
- C++20 compiler (GCC 10+, Clang 12+, Apple Clang 14+)
- Boost 1.75+ (Beast, Asio)
- OpenSSL
- nlohmann/json
- simdjson

### macOS
```bash
brew install boost openssl nlohmann-json simdjson
```

### Ubuntu/Debian
```bash
sudo apt install libboost-all-dev libssl-dev nlohmann-json3-dev libsimdjson-dev
```

### Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

## Usage

### Live Market Data Stream with Strategies
```bash
# Stream BTCUSDT order book with strategy signals
./bin/binance_stream btcusdt

# Stream other symbols
./bin/binance_stream ethusdt
./bin/binance_stream solusdt
```

### Sample Output
```
=== btcusdt Order Book + Strategy ===
------------------------------------------------------------
  ASK        88241.81  |      2.23322000
  ASK        88241.80  |      0.39077000
============================================================
  BID        88241.79  |      4.48388000
  BID        88241.78  |      0.11133000
------------------------------------------------------------
Spread: 0.01 USDT  |  Mid: 88241.79 USDT
Updates: 2250 | Levels: 1006B / 1011A
------------------------------------------------------------
STRATEGY INDICATORS:
  Spread: 0.0000% (avg: 0.0000%)
  Imbalance: 79.5% [BUY PRESSURE ↑]
------------------------------------------------------------
RECENT SIGNALS:
  [BUY] Imbalance: Bid imbalance: 79.3% (buy pressure)
------------------------------------------------------------
Latency: Mean=42.80μs | P99=311.04μs | Max=1039.54μs
```

Press `Ctrl+C` to stop and see final statistics.

### Order Book Benchmark
```bash
./bin/order_book_benchmark
```

## Project Structure
```
hft-trading-system/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── common/
│   │   ├── types.hpp           # Core types (Price, Quantity, Side)
│   │   └── latency_stats.hpp   # Latency measurement utilities
│   ├── order_book/
│   │   ├── order_book.hpp      # Order book interface
│   │   └── order_book.cpp      # Order book implementation
│   ├── market_data/
│   │   ├── binance_client.hpp  # WebSocket client interface
│   │   ├── binance_client.cpp  # WebSocket client implementation
│   │   └── binance_messages.hpp # simdjson message parsing
│   ├── strategy/
│   │   └── strategy.hpp        # Strategy framework and implementations
│   ├── main.cpp                # Basic demo
│   └── binance_stream_main.cpp # Full demo with strategies
└── benchmark/
    └── order_book_benchmark.cpp
```

## Technical Details

### Order Book Design

- **Price Storage**: Fixed-point integers to avoid floating-point precision issues
  - Price: `int64_t` (e.g., $89358.13 → 8935813)
  - Quantity: `int64_t` with 8 decimal places (satoshi precision)

- **Data Structures**:
  - `std::unordered_map<Price, Quantity>`: O(1) price level lookup
  - `std::set<Price>`: Maintains sorted prices for best bid/ask

- **Caching**: Best bid/ask cached and invalidated on updates

### JSON Parsing Optimization

Using **simdjson** for high-performance JSON parsing:
- SIMD instructions for parallel character processing
- On-demand parsing (only parse accessed fields)
- Reused parser instance (zero allocation per message)

### Network Architecture

- **Async I/O**: Non-blocking operations using Boost.Asio
- **Connection Flow**: DNS → TCP → TLS → WebSocket handshake
- **Synchronization**: REST API snapshot + WebSocket incremental updates

### Strategy Framework

- **Base Class**: `Strategy` with virtual `OnOrderBookUpdate()`
- **Signal System**: Decoupled signal generation from handling
- **State Machine**: Prevents duplicate signals

### Thread Model
```
Main Thread:   Signal handling, shutdown coordination
I/O Thread:    Network I/O, message parsing, order book updates, strategy execution
```

## Future Optimizations

1. **Memory Pool**: Pre-allocated memory to avoid dynamic allocation
2. **Lock-free Structures**: For multi-threaded access
3. **Binary Protocol**: Use Binance's binary WebSocket format
4. **Kernel Bypass**: DPDK or Solarflare for ultra-low latency
5. **More Strategies**: VWAP, TWAP, Market Making
6. **Backtesting Engine**: Test strategies on historical data

## Disclaimer

This is a **demonstration project** for educational purposes. The strategies implemented are simplified examples and are **not intended for actual trading**. Real trading systems require:
- Extensive backtesting
- Risk management
- Transaction cost analysis
- Regulatory compliance
- Much lower latency

## License

MIT License