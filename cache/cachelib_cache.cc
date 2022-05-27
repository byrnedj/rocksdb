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
#include "rocksdb/utilities/options_type.h"

#include "util/mutexlock.h"
#include "cachelib/allocator/CacheAllocator.h"

namespace ROCKSDB_NAMESPACE {
namespace {
static std::unordered_map<std::string, OptionTypeInfo> memory_tier_option_map = {
};
  
static std::unordered_map<std::string, OptionTypeInfo> cachelib_option_map = {
     {"capacity",
      {0, OptionType::kUnknown, OptionVerificationType::kNormal, OptionTypeFlags::kDontCompare}.
         .SetParseFunc([](const ConfigOptions& opts,
                          const std::string& /*name*/, const std::string& value,
                          void* addr) {
           auto config = static_cast<CacheConfig*>(addr);
	   auto capacity = ParseSizeT(value);
	   config->setCacheSize(capacity);
	   return Status::OK();
	 })
         .SetSerializeFunc([](const ConfigOptions& opts,
                          const std::string& /*name*/, const std::string& value,
                          void* addr) {
           const auto config = static_cast<CacheConfig*>(addr);
	   auto capacity = config->GetCacheSize();
	   *value = std::to_string(capacity);
	   return Status::OK();
	 })
	 },
};
} // namespace 
  
namespace facebook {
namespace cachelib {
CacheLibCache::CacheLibCache(size_t capacity)
                   id(0) {
  config_
    .setCacheSize(capacity) // 1GB
    .setCacheName("CacheLibCache")
    .setAccessConfig(
		     {25 /* bucket power */, 10 /* lock power */}) // assuming caching 20
    // million items
  //params for multi-tier
  //tier 1 file path + ratio
  //tier n file path + ratio 5
  //.configureMemoryTiers( {
  //        MemoryTierCacheConfig::fromFile("/dev/shm/file1").setRatio(1),
  //        MemoryTierCacheConfig::fromFile("/mnt/pmem1/file1").setRatio(2)) })
    RegisterOptions("", &config_, &cachelib_option_map);
}

Status CacheLibCache::PrepareOptions(const ConfigOptions& /*options*/) {
  if (cache == nullptr) {
    try {
      config_.validate(); // Will throw if bad config
      cache = std::make_unique<CacheLibAllocator>(config_);
      defaultPool =
	cache->addPool("default", cache->getCacheMemoryStats().cacheSize);
    } catch (const std::exception& e) {
      return Status::InvalidArgument(e.what());
    }
  }
  return Status::OK();
}


CacheLibCache::~CacheLibCache() {
}


bool CacheLibCache::Ref(Handle* handle) 
{
  CacheLibHandle* e = reinterpret_cast<CacheLibHandle*>(handle);
  // e->handle->incRef();
  return true;
}


void CacheLibCache::Erase(const Slice& key) {
    folly::StringPiece k(key.data(), key.size());
    cache->remove(k);
}

void* CacheLibCache::Value(Handle* handle) {
  return reinterpret_cast<CacheLibHandle*>(handle)->handle->getMemory();
}


//TODO: what is charge (size of data + header)
size_t CacheLibCache::GetCharge(Handle* handle) const {
  return reinterpret_cast<const CacheLibHandle*>(handle)->handle->getSize();
}

Cache::DeleterFn CacheLibCache::GetDeleter(Handle* handle) const {
  throw std::runtime_error("NOT SUPPORTED");
}

void CacheLibCache::DisownData() {
  // Leak data only if that won't generate an ASAN/valgrind warning.
}

void CacheLibCache::EraseUnRefEntries()
{
  // XXX
}

Status CacheLibCache::Insert(const Slice& key, void* value, size_t charge,
                        DeleterFn deleter, Handle** handle, Priority priority)
{
  // XXX: store deleter inside item.

  folly::StringPiece k(key.data(), key.size());
  auto c_handle = cache->allocate(defaultPool, k, charge);
  if (!c_handle) return Status::NoSpace();

  std::memcpy(c_handle->getMemory(), value, charge);

  cache->insertOrReplace(c_handle);

  auto h = new CacheLibHandle;
  h->handle = std::move(c_handle);

  *handle = reinterpret_cast<Handle*>(h);
  if (!handle) return Status::NoSpace();

  return Status::OK();
}

Cache::Handle* CacheLibCache::Lookup(const Slice& key, Statistics* stats)
{
  // XXX: stats

  folly::StringPiece k(key.data(), key.size());
  auto c_handle = cache->findToWrite(k);

  auto h = new CacheLibHandle;
  h->handle = std::move(c_handle);

  return reinterpret_cast<Handle*>(h);
}

uint64_t CacheLibCache::NewId() { return id.fetch_add(1); }

bool CacheLibCache::Release(Handle* handle, bool erase_if_last_ref)
{
  // reinterpret_cast<CacheLibHandle*>(handle)->handle->decRef();
  return true;
}

}  // 
}  // namespace facebook


std::shared_ptr<Cache> NewCacheLibCache(const LRUCacheOptions& cache_opts) {
  return std::make_shared<facebook::cachelib::CacheLibCache(
      cache_opts.capacity, cache_opts.high_pri_pool_ratio,
      cache_opts.metadata_charge_policy);
}

std::shared_ptr<Cache> NewCacheLibCache(
    size_t capacity, 
    double high_pri_pool_ratio,
    CacheMetadataChargePolicy metadata_charge_policy) {
  return std::make_shared<facebook::cachelib::CacheLibCache(
      capacity, high_pri_pool_ratio,
      metadata_charge_policy);
}

}  // namespace ROCKSDB_NAMESPACE
