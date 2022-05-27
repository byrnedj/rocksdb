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

CacheLibCache::CacheLibCache(size_t capacity, double high_pri_pool_ratio,
                   CacheMetadataChargePolicy metadata_charge_policy):
                   id(0) {

  //params for multi-tier
  //tier 1 file path + ratio
  //tier n file path + ratio 5
  //.configureMemoryTiers( {
  //        MemoryTierCacheConfig::fromFile("/dev/shm/file1").setRatio(1),
  //        MemoryTierCacheConfig::fromFile("/mnt/pmem1/file1").setRatio(2)) })
  // params in general
  // capacity - uint in bytes
  //
  CacheConfig config;
  config
      .setCacheSize(capacity) // 1GB
      .setCacheName("CacheLibCache")
      .setAccessConfig(
          {25 /* bucket power */, 10 /* lock power */}) // assuming caching 20
                                                        // million items
      .validate(); // will throw if bad config
  cache = std::make_unique<CacheLibAllocator>(config);
  defaultPool =
      cache->addPool("default", cache->getCacheMemoryStats().cacheSize);
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
  char *mem = reinterpret_cast<char*>(reinterpret_cast<CacheLibHandle*>(handle)->handle->getMemory());
  return mem + sizeof(DeleterFn);
}


//TODO: what is charge (size of data + header)
size_t CacheLibCache::GetCharge(Handle* handle) const {
  return reinterpret_cast<const CacheLibHandle*>(handle)->handle->getSize() - sizeof(DeleterFn);
}

Cache::DeleterFn CacheLibCache::GetDeleter(Handle* handle) const {
  char *mem = reinterpret_cast<char*>(reinterpret_cast<CacheLibHandle*>(handle)->handle->getMemory());
  return *reinterpret_cast<DeleterFn*>(mem);
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

  // XXX: handle null value with size by reducing capacity (shrinkPool maybe?)
  if (!value)
    return Status::OK();

  auto size = charge + sizeof(deleter);

  folly::StringPiece k(key.data(), key.size());
  auto c_handle = cache->allocate(defaultPool, k, size);
  if (!c_handle) return Status::NoSpace();

  new (c_handle->getMemory()) DeleterFn(deleter);
  char *mem = reinterpret_cast<char*>(c_handle->getMemory());
  std::memcpy(mem + sizeof(deleter), value, charge);

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

  return true;
}

}  // 
}  // namespace facebook


std::shared_ptr<Cache> NewCacheLibCache(const LRUCacheOptions& cache_opts) {
  return std::make_shared<facebook::cachelib::CacheLibCache>(
      cache_opts.capacity, cache_opts.high_pri_pool_ratio,
      cache_opts.metadata_charge_policy);
}

}  // namespace ROCKSDB_NAMESPACE
