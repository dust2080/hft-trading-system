# HFT Trading System

A high-frequency trading system demo featuring real-time market data processing from Binance.

## Features

- **High-Performance Order Book**: Sub-microsecond update latency
- **Real-time Market Data**: Binance WebSocket integration
- **Low-Latency Processing**: ~20μs median end-to-end latency
- **Performance Monitoring**: Built-in latency statistics

## Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                     Binance Exchange                        │
└─────────────────────────────┬───────────────────────────────┘
                              │ WebSocket (wss://)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     BinanceClient                           │
│  - Async I/O (Boost.Asio)                                   │
│  - TLS encryption                                           │
│  - JSON parsing                                             │
└─────────────────────────────┬───────────────────────────────┘
                              │ Callbacks
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      OrderBook                              │
│  - Price levels: std::unordered_map + std::set              │
│  - O(1) best bid/ask query                                  │
│  - O(log N) update                                          │
└─────────────────────────────────────────────────────────────┘
```

## Performance

### Order Book Benchmark (Synthetic)

| Operation | Median | P99 |
|-----------|--------|-----|
| Update | 166 ns | 1,208 ns |
| GetBestBid | 41 ns | 42 ns |
| GetQuantityAt | 42 ns | 209 ns |

### End-to-End Latency (Live Binance Data)

| Metric | Value |
|--------|-------|
| Min | 0.79 μs |
| Median | 19.54 μs |
| Mean | 32.10 μs |
| P99 | 183.25 μs |
| Max | 536.62 μs |

*Tested with BTCUSDT, ~580 samples over 1 minute*

### Latency Breakdown (Estimated)
```
Total: ~32 μs
├── JSON parsing:        ~15-20 μs
├── String conversion:   ~5-10 μs
├── Order Book update:   ~1-5 μs
└── Other overhead:      ~2-5 μs
```

## Building

### Prerequisites

- CMake 3.16+
- C++20 compiler (GCC 10+, Clang 12+, Apple Clang 14+)
- Boost 1.75+ (Beast, Asio)
- OpenSSL
- nlohmann/json

### macOS
```bash
brew install boost openssl nlohmann-json
```

### Ubuntu/Debian
```bash
sudo apt install libboost-all-dev libssl-dev nlohmann-json3-dev
```

### Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

## Usage

### Live Market Data Stream
```bash
# Stream BTCUSDT order book
./bin/binance_stream btcusdt

# Stream other symbols
./bin/binance_stream ethusdt
./bin/binance_stream solusdt
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
│   │   └── binance_messages.hpp # Binance message parsing
│   ├── main.cpp                # Demo application
│   └── binance_stream_main.cpp # Live streaming application
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

### Network Architecture

- **Async I/O**: Non-blocking operations using Boost.Asio
- **Connection Flow**: DNS → TCP → TLS → WebSocket handshake
- **Synchronization**: REST API snapshot + WebSocket incremental updates

### Thread Model
```
Main Thread:        Signal handling, shutdown coordination
I/O Thread:         Network I/O, message parsing, order book updates
```

## Future Optimizations

1. **Faster JSON Parsing**: Replace nlohmann/json with simdjson
2. **Memory Pool**: Pre-allocated memory to avoid dynamic allocation
3. **Lock-free Structures**: For multi-threaded access
4. **Binary Protocol**: Use Binance's binary WebSocket format
5. **Kernel Bypass**: DPDK or Solarflare for ultra-low latency

## License

MIT License