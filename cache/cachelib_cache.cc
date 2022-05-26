//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "cache/cachelib_cache.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "monitoring/perf_context_imp.h"
#include "monitoring/statistics.h"
#include "port/lang.h"
#include "util/mutexlock.h"
#include "cachelib/allocator/CacheAllocator.h"

namespace ROCKSDB_NAMESPACE {
namespace facebook {
namespace cachelib_cache {

using Cache = cachelib::LruAllocator; // or Lru2QAllocator, or TinyLFUAllocator
using CacheConfig = typename Cache::Config;
using CacheKey = typename Cache::Key;
using CacheItemHandle = typename Cache::ReadHandle;


CacheLibCache::CacheLibCache(size_t capacity, int num_shard_bits,
                   bool strict_capacity_limit,
                   CacheMetadataChargePolicy metadata_charge_policy) {
  //num_shards_ = 1 << num_shard_bits;
  //shards_ = reinterpret_cast<CacheLibCacheShard*>(
  //    port::cacheline_aligned_alloc(sizeof(CacheLibCacheShard) * num_shards_));
  //size_t per_shard = (capacity + (num_shards_ - 1)) / num_shards_;
  
  //for (int i = 0; i < num_shards_; i++) {
    //new (&shards_[i])
    //    CacheLibCacheShard(per_shard, strict_capacity_limit, metadata_charge_policy,
    //                  /* max_upper_hash_bits */ 32 - num_shard_bits);
  //}
  CacheConfig config;
  config
      .setCacheSize(capacity) // 1GB
      .setCacheName("CacheLibCache")
      .setAccessConfig(
          {25 /* bucket power */, 10 /* lock power */}) // assuming caching 20
                                                        // million items
      .validate(); // will throw if bad config
  cache = std::make_unique<Cache>(config);
}

CacheLibCache::~CacheLibCache() {
}


void* CacheLibCache::Value(Handle* handle) {
  return reinterpret_cast<const CacheLibHandle*>(handle)->value;
}

//TODO: what is charge (size of data + header)
size_t CacheLibCache::GetCharge(Handle* handle) const {
  return reinterpret_cast<const CacheLibHandle*>(handle)->charge;
}

Cache::DeleterFn CacheLibCache::GetDeleter(Handle* handle) const {
  auto h = reinterpret_cast<const CacheLibHandle*>(handle);
  return h->deleter;
}

uint32_t CacheLibCache::GetHash(Handle* handle) const {
  return reinterpret_cast<const CacheLibHandle*>(handle)->hash;
}

void CacheLibCache::DisownData() {
  // Leak data only if that won't generate an ASAN/valgrind warning.
  if (!kMustFreeHeapAllocations) {
    shards_ = nullptr;
    num_shards_ = 0;
  }
}

}  // namespace cachelib_cache

std::shared_ptr<Cache> CacheLibCache(
    size_t capacity, int num_shard_bits, bool strict_capacity_limit,
    CacheMetadataChargePolicy metadata_charge_policy) {
  if (num_shard_bits >= 20) {
    return nullptr;  // The cache cannot be sharded into too many fine pieces.
  }
  if (num_shard_bits < 0) {
    num_shard_bits = GetDefaultCacheShardBits(capacity);
  }
  return std::make_shared<cachelib_cache::CacheLibCache>(
      capacity, num_shard_bits, strict_capacity_limit, metadata_charge_policy);
}

}  // namespace ROCKSDB_NAMESPACE
