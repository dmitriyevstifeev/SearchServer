#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <type_traits>
#include <cstddef>

template<typename Key, typename Value>
class ConcurrentMap {
 private:
  struct Bucket {
    std::mutex mx_;
    std::map<Key, Value> map_;
  };

  std::vector<Bucket> buckets_;

  Bucket& GetBucket(const Key &key) {
    return buckets_[static_cast<uint64_t>(key) % buckets_.size()];
  }

 public:
  static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

  struct Access {
    Access(const Key &key, Bucket &bucket)
        : lock_(bucket.mx_)
        , ref_to_value(bucket.map_[key]) {
    }

    std::lock_guard<std::mutex> lock_;
    Value &ref_to_value;
  };

  explicit ConcurrentMap(size_t bucket_count)
      : buckets_(bucket_count) {
  }

  Access operator[](const Key &key) {
    return Access{key, GetBucket(key)};
  };

  void Erase(const Key &key){
    auto &bucket = GetBucket(key);
    std::lock_guard lock(bucket.mx_);
    bucket.map_.erase(key);
  }

  std::map<Key, Value> BuildOrdinaryMap() {
    std::map<Key, Value> full_map;
    for (auto &[mx_, map_]: buckets_) {
      {
        std::lock_guard lock_(mx_);
        full_map.insert(map_.begin(), map_.end());
      }
    }
    return full_map;
  }
};