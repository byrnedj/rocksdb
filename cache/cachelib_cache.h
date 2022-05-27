//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#pragma once

#include <atomic>
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
namespace cachelib {

using CacheLibAllocator = ::facebook::cachelib::LruAllocator; // or Lru2QAllocator, or TinyLFUAllocator
using CacheConfig = typename CacheLibAllocator::Config;
using CacheKey = typename CacheLibAllocator::Key;
using CacheItemHandle = typename CacheLibAllocator::WriteHandle;

struct CacheLibHandle {
  CacheItemHandle handle;
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
  DeleterFn GetDeleter(Handle* handle) const override;
  void DisownData() override;

  //  virtual const char* Name() const = 0;
  // virtual Status Insert(const Slice& key, void* value, size_t charge,
  bool Ref(Handle* handle);
  void Erase(const Slice& key);

  uint64_t NewId();

  void SetCapacity(size_t capacity) {
    // XXX
  }

  void SetStrictCapacityLimit(bool strict_capacity_limit) { // not supported by cachelib?
   } 

  bool HasStrictCapacityLimit() const { return false; }

  size_t GetCapacity() const {return 0;};

  using Cache::Insert;
  Status Insert(const Slice& key, void* value, size_t charge,
                        DeleterFn deleter, Handle** handle = nullptr,
                        Priority priority = Priority::LOW);
  using Cache::Lookup;
  Handle* Lookup(const Slice& key, Statistics* stats = nullptr);
  
  using Cache::Release;
  bool Release(Handle* handle, bool erase_if_last_ref = false);

  void ApplyToAllEntries(
      const std::function<void(const Slice& key, void* value, size_t charge,
                               DeleterFn deleter)>& callback,
      const ApplyToAllEntriesOptions& opts) {};
  size_t GetUsage() const { return 0; }

  // Returns the memory size for a specific entry in the cache.
  size_t GetUsage(Handle* handle) const { return 0; }

  // Returns the memory size for the entries in use by the system
  size_t GetPinnedUsage() const { return 0; }

  void EraseUnRefEntries();

 private:
  std::atomic<size_t> id;
  std::unique_ptr<CacheLibAllocator> cache;
  ::facebook::cachelib::PoolId defaultPool;
};

}  // namespace facebook 
}

std::shared_ptr<Cache> CacheLibCache(
    size_t capacity, int num_shard_bits = -1,
    bool strict_capacity_limit = false,
    CacheMetadataChargePolicy metadata_charge_policy =
        kDefaultCacheMetadataChargePolicy);

}  // namespace ROCKSDB_NAMESPACE
