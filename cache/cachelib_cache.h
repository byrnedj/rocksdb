//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#pragma once

#include <memory>
#include <string>

#include "cachelib/allocator/CacheAllocator.h"
#include "cache/sharded_cache.h"
#include "port/lang.h"
#include "port/malloc.h"
#include "port/port.h"
#include "rocksdb/secondary_cache.h"
#include "util/autovector.h"

namespace ROCKSDB_NAMESPACE {
namespace facebook {
namespace cachelib_cache {

using CacheLibCache = rocksdb::facebook::cachelib::LruAllocator; // or Lru2QAllocator, or TinyLFUAllocator
using CacheConfig = typename CacheLibCache::Config;
using CacheKey = typename CacheLibCache::Key;
using CacheItemHandle = typename CacheLibCache::ReadHandle;


struct CacheLibHandle {
  CacheItemHandle handle;

  Cache::DeleterFn deleter;
  size_t charge;  // TODO(opt): Only allow uint32_t?
  void* value;
/*
  LRUHandle* next_hash;
  LRUHandle* next;
  LRUHandle* prev;
  size_t key_length;
  // The hash of key(). Used for fast sharding and comparisons.
  uint32_t hash;
  // The number of external refs to this entry. The cache itself is not counted.
  uint32_t refs;

  enum Flags : uint8_t {
    // Whether this entry is referenced by the hash table.
    IN_CACHE = (1 << 0),
  };
  uint8_t flags;

  // Beginning of the key (MUST BE THE LAST FIELD IN THIS STRUCT!)
  char key_data[1];

  Slice key() const { return Slice(key_data, key_length); }

  // Increase the reference count by 1.
  void Ref() { refs++; }

  // Just reduce the reference count by 1. Return true if it was last reference.
  bool Unref() {
    assert(refs > 0);
    refs--;
    return refs == 0;
  }

  // Return true if there are external refs, false otherwise.
  bool HasRefs() const { return refs > 0; }

  bool InCache() const { return flags & IN_CACHE; }

  void SetInCache(bool in_cache) {
    if (in_cache) {
      flags |= IN_CACHE;
    } else {
      flags &= ~IN_CACHE;
    }
  }

  void Free() {
    assert(refs == 0);
    if (deleter) {
      (*deleter)(key(), value);
    }
    delete[] reinterpret_cast<char*>(this);
  }

  // Calculate the memory usage by metadata.
  inline size_t CalcTotalCharge(
      CacheMetadataChargePolicy metadata_charge_policy) {
    size_t meta_charge = 0;
    if (metadata_charge_policy == kFullChargeCacheMetadata) {
#ifdef ROCKSDB_MALLOC_USABLE_SIZE
      meta_charge += malloc_usable_size(static_cast<void*>(this));
#else
      // This is the size that is used when a new handle is created.
      meta_charge += sizeof(LRUHandle) - 1 + key_length;
#endif
    }
    return charge + meta_charge;
  }
  */
};

class CacheLibCache : public Cache {

 public:
  CacheLibCache(size_t capacity, int num_shard_bits, bool strict_capacity_limit,
           CacheMetadataChargePolicy metadata_charge_policy =
               kDontChargeCacheMetadata);
  ~CacheLibCache() override;
  const char* Name() const override { return "CacheLibCache"; }
  void* Value(Handle* handle) override;
  size_t GetCharge(Handle* handle) const override;
  uint32_t GetHash(Handle* handle) const override;
  DeleterFn GetDeleter(Handle* handle) const override;
  void DisownData() override;

 private:
  Cache *cache;
  int num_shards_ = 0;
};

}  // namespace cachelib_cache
}  // namespace facebook 

std::shared_ptr<Cache> CacheLibCache(
    size_t capacity, int num_shard_bits = -1,
    bool strict_capacity_limit = false,
    CacheMetadataChargePolicy metadata_charge_policy =
        kDefaultCacheMetadataChargePolicy);

}  // namespace ROCKSDB_NAMESPACE
