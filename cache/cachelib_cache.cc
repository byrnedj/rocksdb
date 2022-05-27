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

#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {
namespace {
static std::unordered_map<std::string, OptionTypeInfo> memory_tier_option_map = {
};
  
static std::unordered_map<std::string, OptionTypeInfo> cachelib_option_map = {
     {"capacity",
      OptionTypeInfo{0, OptionType::kUnknown, OptionVerificationType::kNormal, OptionTypeFlags::kCompareNever}
         .SetParseFunc([](const ConfigOptions& opts,
                          const std::string& /*name*/, const std::string& value,
                          void* addr) {
           auto config = static_cast<facebook::cachelib::CacheConfig*>(addr);
	   auto capacity = ParseSizeT(value);
	   config->setCacheSize(capacity);
	   return Status::OK();
	 })
         .SetSerializeFunc([](const ConfigOptions& opts,
                          const std::string& /*name*/, const void* addr, std::string* value) {
           const auto config = static_cast<const facebook::cachelib::CacheConfig*>(addr);
	   auto capacity = config->getCacheSize();
	   *value = std::to_string(capacity);
	   return Status::OK();
	 })
	 }
};
} // namespace 
  
namespace facebook {
namespace cachelib {
CacheLibCache::CacheLibCache(size_t capacity):
                   id(0) {
  config_
    .setCacheSize(capacity) // 1GB
    .setCacheName("CacheLibCache")
    .setAccessConfig(
		     {25 /* bucket power */, 10 /* lock power */}) // assuming caching 20
    .enableCachePersistence("/tmp")
    .usePosixForShm()
  .configureMemoryTiers({
         ::facebook::cachelib::MemoryTierCacheConfig::fromShm().setRatio(1),
         ::facebook::cachelib::MemoryTierCacheConfig::fromFile("/mnt/pmem/file1").setRatio(1)});
    RegisterOptions("", &config_, &cachelib_option_map);
}

Status CacheLibCache::PrepareOptions(const ConfigOptions& /*options*/) {
  if (cache == nullptr) {
    try {
      config_.validate(); // Will throw if bad config
      cache = std::make_unique<CacheLibAllocator>(CacheLibAllocator::SharedMemNew, config_);
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
  auto& h = reinterpret_cast<const CacheLibHandle*>(handle)->handle;
  if (!h)
    return nullptr; 

  char *mem = reinterpret_cast<char*>(h->getMemory());
  return mem + sizeof(DeleterFn);
}


//TODO: what is charge (size of data + header)
size_t CacheLibCache::GetCharge(Handle* handle) const {
  auto &h = reinterpret_cast<const CacheLibHandle*>(handle)->handle;
  if (!h)
    return 0;  

  return h->getSize() - sizeof(DeleterFn);
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
  if (!h) return Status::NoSpace();

  h->handle = std::move(c_handle);

  if (handle)
    *handle = reinterpret_cast<Handle*>(h);

  return Status::OK();
}

Cache::Handle* CacheLibCache::Lookup(const Slice& key, Statistics* stats)
{
  // XXX: stats

  folly::StringPiece k(key.data(), key.size());
  auto c_handle = cache->findToWrite(k);

  if(!c_handle)
    return nullptr;

  auto h = new CacheLibHandle;
  h->handle = std::move(c_handle);

  return reinterpret_cast<Handle*>(h);
}

uint64_t CacheLibCache::NewId() { return id.fetch_add(1); }

bool CacheLibCache::Release(Handle* handle, bool erase_if_last_ref)
{
  delete reinterpret_cast<CacheLibHandle*>(handle);
  return true;
}

bool CacheLibCache::IsReady(Handle* handle)
{
  auto &h = reinterpret_cast<const CacheLibHandle*>(handle)->handle;
  if (!h)
    return false;

  return reinterpret_cast<CacheLibHandle*>(handle)->handle.isReady();
}

void CacheLibCache::Wait(Handle* handle)
{
  auto &h = reinterpret_cast<const CacheLibHandle*>(handle)->handle;
  if (!h)
    return; 
  reinterpret_cast<CacheLibHandle*>(handle)->handle.wait();
}

void CacheLibCache::WaitAll(std::vector<Handle*>& handles)
{
  // XXX: optimize
  for (auto &h : handles)
    Wait(h);
}

}  // 
}  // namespace facebook

}  // namespace ROCKSDB_NAMESPACE
