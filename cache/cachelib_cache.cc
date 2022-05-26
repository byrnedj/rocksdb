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
namespace cachelib {

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
  cache = std::make_unique<CacheLibAllocator>(config);
}

CacheLibCache::~CacheLibCache() {
}


bool CacheLibCache::Ref(Handle* handle) 
{
    return false;
}


void CacheLibCache::Erase(const Slice& key) {
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

void CacheLibCache::DisownData() {
  // Leak data only if that won't generate an ASAN/valgrind warning.
}

void CacheLibCache::EraseUnRefEntries()
{

}

Status CacheLibCache::Insert(const Slice& key, void* value, size_t charge,
                        DeleterFn deleter, Handle** handle = nullptr,
                        Priority priority = Priority::LOW)
{
  return Status::OK;
}

Cache::Handle* CacheLibCache::Lookup(const Slice& key, Statistics* stats)
{
  return nullptr;
}

bool CacheLibCache::Release(Handle* handle, bool erase_if_last_ref)
{
  return false;
}

}  // 
}  // namespace facebook

std::shared_ptr<Cache> CacheLibCache(
    size_t capacity, int num_shard_bits, bool strict_capacity_limit,
    CacheMetadataChargePolicy metadata_charge_policy) {
  return std::make_shared<facebook::cachelib::CacheLibCache>(
      capacity, num_shard_bits, strict_capacity_limit, metadata_charge_policy);
}

}  // namespace ROCKSDB_NAMESPACE
