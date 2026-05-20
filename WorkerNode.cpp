// Author: Rudhra Deep Biswas | github: rudeUltra
// File: WorkerNode.cpp
#include <grpcpp/grpcpp.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "grid.grpc.pb.h"
#include "SemanticMemoryGrid.h"

using namespace grpc;
using namespace semanticgrid;

enum class NodeRole { MASTER, REPLICA };

class SubscriberQueue {
public:
    std::queue<PutRequest> updates;
    std::mutex mtx; 
    std::condition_variable cv;

    void push(const PutRequest& req) {
        std::lock_guard<std::mutex> lock(mtx); 
        updates.push(req);
        cv.notify_one(); 
    }
};

class CacheServiceImpl final : public SemanticCache::Service {
private:
    SemanticMemoryGrid<std::string> grid;
    NodeRole role;
    
    std::vector<std::shared_ptr<SubscriberQueue>> active_replicas;
    std::mutex replica_mutex; 

public:
    CacheServiceImpl(NodeRole assigned_role) : grid(1000000, 2), role(assigned_role) {}

    Status Put(ServerContext* context, const PutRequest* request, google::protobuf::Empty* reply) override {
        if (role == NodeRole::REPLICA) {
            return Status(StatusCode::PERMISSION_DENIED, "Read-only replica. Send writes to Master.");
        }

        std::vector<float> vec(request->vector_data().begin(), request->vector_data().end());
        grid.put(request->key(), vec);

        std::lock_guard<std::mutex> lock(replica_mutex); 
        for (auto& queue : active_replicas) {
            queue->push(*request); 
        }
        return Status::OK;
    }

    Status SemanticSearch(ServerContext* context, const SearchRequest* request, ServerWriter<SearchResponse>* writer) override {
        std::vector<float> query(request->query_vector().begin(), request->query_vector().end());
        auto top_results = grid.semantic_search(query, request->top_k());

        for (const auto& result : top_results) {
            SearchResponse response;
            response.set_key(result.key);
            response.set_score(result.score);
            writer->Write(response); 
        }
        return Status::OK;
    }

    Status Subscribe(ServerContext* context, const google::protobuf::Empty* request, ServerWriter<PutRequest>* writer) override {
        auto my_queue = std::make_shared<SubscriberQueue>();
        {
            std::lock_guard<std::mutex> lock(replica_mutex); 
            active_replicas.push_back(my_queue);
        } 

        while (!context->IsCancelled()) {
            std::unique_lock<std::mutex> lock(my_queue->mtx); 
            my_queue->cv.wait(lock, [&]{ return !my_queue->updates.empty(); });
            
            PutRequest update = my_queue->updates.front();
            my_queue->updates.pop();
            
            lock.unlock(); 
            writer->Write(update);
        }
        return Status::OK;
    }
};