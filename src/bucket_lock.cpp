#include <array>
#include <mutex>

// степень двойки
const size_t NUM_BUCKETS = 64;

struct BucketLocker {
    
    // alignas(64) нужен, чтобы мьютексы не лежали в одной кэш-линии процессора 
    struct alignas(64) Shard {
        std::mutex mtx;
    };
    
    std::array<Shard, NUM_BUCKETS> shards;

    // Получить мьютекс для конкретного FD
    std::mutex& get_mutex(int fd) {
        return shards[fd % NUM_BUCKETS].mtx;
    }
};

