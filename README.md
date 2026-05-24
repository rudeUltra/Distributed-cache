# Semantic AI Memory Grid

A high-performance, distributed vector database and semantic cache built from scratch in C++17. 

This project bridges low-level CPU optimization with high-concurrency distributed systems engineering. It utilizes SIMD hardware intrinsics for blazing-fast vector math, Lock Striping for thread-safe memory management, and HTTP/2 multiplexing via gRPC for infinitely scalable read throughput.

## System Architecture

### The Topology

```text
                              [ External Client ]
                                       │
                                       ▼
===================================================================================
                             [ GATEWAY ROUTER ]
                     (Layer 7 Load Balancer / Envoy)
   * Holds the Consistent Hash Ring for targeted writes.
   * Executes Scatter-Gather parallel network requests for global searches.
===================================================================================
                  │                                         │
       [ Route: Put() Request ]                  [ Route: Search() Request ]
       (Hashes Key to find Master)             (Scatter to ALL Shard Groups)
                  │                                         │
                  ▼                                         ▼
+-----------------------------------+     +-----------------------------------+
|      MASTER WORKER NODE           |     |      REPLICA WORKER NODE          |
|      (Shard Group A)              |     |      (Shard Group A)              |
+-----------------------------------+     +-----------------------------------+
| 1. gRPC Server (Accepts Writes)   |     | 1. gRPC Server (Accepts Reads)    |
|                                   |     |                                   |
| 2. SemanticMemoryGrid             |     | 2. SemanticMemoryGrid             |
|    - Shard 0: [LRU-K Buffers]     |     |    - Shard 0: [LRU-K Buffers]     |
|    - Shard 1: [LRU-K Buffers]     |     |    - Shard 1: [LRU-K Buffers]     |
|    - SIMD Math Engine (AVX2)      |     |    - SIMD Math Engine (AVX2)      |
+-----------------------------------+     +-----------------------------------+
                  │                                         ▲
                  └─────────────► [ gRPC Sync Stream ] ─────┘
                                  (Asynchronous replication)
```
## Benchmarks

| Benchmark | Time | CPU | Iterations |
|-----------|------:|------:|-----------:|
| `BM_SemanticSearch` | **7.495 µs** | **7.491 µs** | `100000` |

![Search Metrics](https://github.com/rudeUltra/Distributed-cache/blob/7f7977cdf9594e45ac46e03baf61f34bcee990f8/SearchBenchmark.png)

## Network Gateway Performance Summary

| Metric | Value |
|---|---|
| Count | 10000 |
| Total Time | 0.62 s |
| Slowest Request | 68.42 ms |
| Fastest Request | 2.15 ms |
| Average Request | 31.00 ms |
| Requests/sec | 16129.03 |

---

## Response Time Histogram

```text
  2.150  [1]     |
  8.777  [640]   |∎∎∎∎
  15.404 [1850]  |∎∎∎∎∎∎∎∎∎∎∎
  22.031 [3105]  |∎∎∎∎∎∎∎∎∎∎∎∎∎∎∎∎∎∎∎
  28.658 [2400]  |∎∎∎∎∎∎∎∎∎∎∎∎∎∎∎
  35.285 [1200]  |∎∎∎∎∎∎∎
  41.912 [550]   |∎∎∎
  48.539 [150]   |∎
  55.166 [80]    |
  61.793 [20]    |
  68.420 [4]     |
```

---

## Latency Distribution

| Percentile | Latency |
|---|---|
| 10% | 15.20 ms |
| 25% | 21.45 ms |
| 50% | 28.40 ms |
| 75% | 34.10 ms |
| 90% | 45.10 ms |
| 95% | 52.80 ms |
| 99% | 58.70 ms |

---

## Status Code Distribution

```text
[OK] 10000 responses
```

---

# Core Features

## Data Partitioning (Lock Striping)

Eliminated global database locks by splitting the memory grid into 16 independent mutex-protected shards, allowing parallel I/O without thread contention.

## LRU-K Cache Eviction

Built a dual-buffer (FIFO + LRU) eviction policy to prevent cache pollution from anomalous one-off queries.

## SIMD Auto-Vectorization

Optimized mathematical dot-product loops using contiguous memory and `__restrict` pointers, forcing the compiler to generate high-throughput hardware instructions.

**Compute time:** ~7µs

## Asymmetric Scaling

Decoupled write-heavy and read-heavy workloads using a Master-Replica topology.

## Real-Time Replication

Implemented a thread-safe Producer-Consumer queue tied to an infinite gRPC HTTP/2 stream for near-zero-latency data syncing.

## Consistent Hashing

Engineered a deterministic routing ring with virtual nodes at the Gateway to distribute writes evenly without cascading cache misses.

## Scatter-Gather Parallel I/O

Utilized `std::async` thread pools to execute global semantic searches across all replica shards concurrently.

---

# Benchmarks & Performance

- **Compute Latency:** ~7.5 µs per internal search (measured via Google Benchmark).
- **Network Throughput:** ~16,000+ Requests Per Second (RPS) under heavy concurrent load (measured via ghz).
- **Tail Latency (p99):** Sub-60ms response times at maximum load, proving the efficiency of the Scatter-Gather routing and lock-free reads.

---

# Build Instructions

## Prerequisites

- C++17 Compiler (GCC/Clang)
- gRPC & Protocol Buffers
- Google Benchmark (for testing)
