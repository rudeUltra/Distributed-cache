// Author: Rudhra Deep Biswas | github: rudeUltra
// File: GatewayRouter.cpp
#include <grpcpp/grpcpp.h>
#include <map>
#include <string>
#include <vector>
#include <future>
#include <algorithm>
#include "grid.grpc.pb.h"

using namespace grpc;
using namespace semanticgrid;

class ConsistentHashRing {
private:
    std::map<size_t, std::string> ring;
    int virtual_nodes = 100;

public:
    void add_master(const std::string& ip) {
        for (int i = 0; i < virtual_nodes; ++i) {
            size_t hash_val = std::hash<std::string>{}(ip + "#VNODE#" + std::to_string(i));
            ring[hash_val] = ip;
        }
    }

    std::string get_master(const std::string& key) {
        if (ring.empty()) return "";
        auto it = ring.lower_bound(std::hash<std::string>{}(key));
        if (it == ring.end()) it = ring.begin();
        return it->second;
    }
};

class Gateway {
private:
    ConsistentHashRing hash_ring;
    std::unordered_map<std::string, std::vector<std::unique_ptr<SemanticCache::Stub>>> shard_groups;

public:
    void RoutePut(const std::string& key, const std::vector<float>& vec) {
        std::string target_master_ip = hash_ring.get_master(key);
        // unary RPC to target_master_ip handling execution
    }

    std::vector<SearchResponse> ScatterGatherSearch(const std::vector<float>& query_vector, int top_k) {
        std::vector<std::future<std::vector<SearchResponse>>> futures;

        for (const auto& group : shard_groups) {
            const auto& replica_stub = group.second[0]; 

            futures.push_back(std::async(std::launch::async, [&replica_stub, &query_vector, top_k]() {
                ClientContext context;
                SearchRequest request;
                for (float v : query_vector) request.add_query_vector(v);
                request.set_top_k(top_k);

                auto reader = replica_stub->SemanticSearch(&context, request);
                std::vector<SearchResponse> local_results;
                SearchResponse response;
                
                while (reader->Read(&response)) {
                    local_results.push_back(response);
                }
                return local_results;
            }));
        }

        std::vector<SearchResponse> global_results;
        for (auto& fut : futures) {
            std::vector<SearchResponse> local_results = fut.get();
            global_results.insert(global_results.end(), local_results.begin(), local_results.end());
        }

        std::sort(global_results.begin(), global_results.end(), 
            [](const SearchResponse& a, const SearchResponse& b) { return a.score() > b.score(); });

        if (global_results.size() > top_k) global_results.resize(top_k);
        return global_results;
    }
};