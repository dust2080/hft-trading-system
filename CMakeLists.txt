cmake_minimum_required(VERSION 3.16)
project(hft_trading_system VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags (removed -march=native for Rosetta compatibility)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find packages
cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Find simdjson
find_library(SIMDJSON_LIB simdjson REQUIRED)
find_path(SIMDJSON_INCLUDE simdjson.h REQUIRED)

# Order book library
add_library(order_book 
    src/order_book/order_book.cpp
)
target_include_directories(order_book PUBLIC 
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/order_book
)

# Market data library
add_library(market_data
    src/market_data/binance_client.cpp
)
target_include_directories(market_data PUBLIC
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/market_data
    ${CMAKE_SOURCE_DIR}/src/strategy
    ${SIMDJSON_INCLUDE}
)
target_link_libraries(market_data 
    PUBLIC 
    order_book
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${SIMDJSON_LIB}
)

# Main executable
add_executable(hft_main src/main.cpp)
target_link_libraries(hft_main PRIVATE market_data)

# Benchmark
add_executable(order_book_benchmark benchmark/order_book_benchmark.cpp)
target_link_libraries(order_book_benchmark PRIVATE order_book)

# Binance stream demo
add_executable(binance_stream src/binance_stream_main.cpp)
target_include_directories(binance_stream PRIVATE ${CMAKE_SOURCE_DIR}/src/strategy)
target_link_libraries(binance_stream PRIVATE market_data)