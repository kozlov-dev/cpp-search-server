#pragma once

#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "log_duration.h"
// #include "test_framework.h"

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap
{
private:
    struct Bucket
    {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access
    {
        std::lock_guard<std::mutex> guard;
        Value &ref_to_value;

        Access(const Key &key, Bucket &bucket)
            : guard(bucket.mutex), ref_to_value(bucket.map[key])
        {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count)
    {
    }

    Access operator[](const Key &key)
    {
        auto &bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {key, bucket};
    }

    void Erase(const Key &key)
    {
        auto &bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        std::lock_guard<std::mutex> lock(bucket.mutex);
        bucket.map.erase(key);
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> result;
        for (auto &[mutex, map] : buckets_)
        {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

private:
    std::vector<Bucket> buckets_;
};