// Author: Rudhra Deep Biswas | github: rudeUltra
// File: SemanticMemoryGrid.h
#ifndef SEMANTIC_MEMORY_GRID_H
#define SEMANTIC_MEMORY_GRID_H

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <list>
#include <set>
#include <mutex>
#include <chrono>
#include <deque>
#include <optional>
#include <memory>
#include <queue>
#include <algorithm>

using namespace std;
using namespace std::chrono;

struct SearchResult {
    string key;
    float score;
    bool operator<(const SearchResult& other) const { return score > other.score; } 
};

struct AccessRecord {
    vector<float> value;
    deque<steady_clock::time_point> history;
    bool is_in_cache_buffer = false;
    list<string>::iterator history_iterator;

    void add_hit(steady_clock::time_point time, size_t k) {
        history.push_back(time);
        if (history.size() > k) history.pop_front();
    }

    steady_clock::time_point get_kth_timestamp() const {
        return history.front(); 
    }
};

template<typename Key = string>
class CacheShard {
private:
    size_t capacity;
    size_t k_value;
    mutex shard_mutex;

    unordered_map<Key, AccessRecord> storage;
    list<Key> history_buffer; 
    set<pair<steady_clock::time_point, Key>> cache_buffer; 

    void evict() {
        if (!history_buffer.empty()) {
            Key to_evict = history_buffer.back();
            storage.erase(to_evict);
            history_buffer.pop_back();
        } else if (!cache_buffer.empty()) {
            Key to_evict = cache_buffer.begin()->second;
            storage.erase(to_evict);
            cache_buffer.erase(cache_buffer.begin());
        }
    }

public:
    CacheShard(size_t cap, size_t k) : capacity(cap), k_value(k) {}

    void put(const Key& key, const vector<float>& value) {
        lock_guard<mutex> lock(shard_mutex);
        auto now = steady_clock::now();

        if (storage.find(key) != storage.end()) {
            storage[key].value = value;
            return; 
        }

        if (storage.size() >= capacity) evict();

        AccessRecord record;
        record.value = value;
        record.add_hit(now, k_value);
        
        storage[key] = record;
        storage[key].history_iterator = history_buffer.insert(history_buffer.begin(), key);
    }

    vector<SearchResult> search_local(const vector<float>& query_vec, int top_k) {
        lock_guard<mutex> lock(shard_mutex);
        priority_queue<SearchResult> local_heap;
        
        const float* __restrict query_ptr = query_vec.data();
        size_t vec_size = query_vec.size();

        for (const auto& [key, record] : storage) {
            const float* __restrict target_ptr = record.value.data();
            float score = 0.0f;
            
            for (size_t i = 0; i < vec_size; ++i) {
                score += query_ptr[i] * target_ptr[i];
            }

            if (local_heap.size() < top_k) {
                local_heap.push({key, score});
            } else if (score > local_heap.top().score) {
                local_heap.pop();
                local_heap.push({key, score});
            }
        }
        
        vector<SearchResult> res;
        while (!local_heap.empty()) { 
            res.push_back(local_heap.top()); 
            local_heap.pop(); 
        }
        return res;
    }
};

template<typename Key = string>
class SemanticMemoryGrid {
private:
    vector<unique_ptr<CacheShard<Key>>> shards;
    size_t num_shards;

public:
    SemanticMemoryGrid(size_t total_cap, size_t k, size_t shard_count = 16) : num_shards(shard_count) {
        for (size_t i = 0; i < num_shards; i++) {
            shards.push_back(make_unique<CacheShard<Key>>(total_cap / shard_count, k));
        }
    }

    void put(const Key& key, const vector<float>& value) {
        size_t hash_val = hash<Key>{}(key);
        shards[hash_val % num_shards]->put(key, value);
    }

    vector<SearchResult> semantic_search(const vector<float>& query_vec, int top_k) {
        priority_queue<SearchResult> global_heap;

        for (auto& shard : shards) {
            auto local_results = shard->search_local(query_vec, top_k);
            for (const auto& res : local_results) {
                if (global_heap.size() < top_k) {
                    global_heap.push(res);
                } else if (res.score > global_heap.top().score) {
                    global_heap.pop();
                    global_heap.push(res);
                }
            }
        }

        vector<SearchResult> final_results;
        while (!global_heap.empty()) {
            final_results.push_back(global_heap.top());
            global_heap.pop();
        }
        reverse(final_results.begin(), final_results.end());
        return final_results;
    }
};

#endif