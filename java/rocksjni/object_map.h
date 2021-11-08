//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//

#pragma once

#include <jni.h>

#include <atomic>
#include <memory>
#include <unordered_map>

#include "port/port.h"
#include "rocksdb/rocksdb_namespace.h"
#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {
namespace jni {

class JniObjectMap {
 public:
  template <typename T>
  static jlong AddObject(T *object) {
    std::shared_ptr<T> shared(object);
    auto next = ++counter_;
    MutexLock l(&mutex_);
    objects_[next] = std::static_pointer_cast<void>(shared);
    return static_cast<jlong>(next);
  }

  template <typename T>
  static jlong AddObject() {
    return RegisterNewObject(new T());
  }

  template <typename T>
  static std::shared_ptr<T> GetObject(jlong id) {
    MutexLock l(&mutex_);
    const auto it = objects_.find(id);
    if (it != objects_.end()) {
      return std::static_pointer_cast<T>(it->second);
    } else {
      return nullptr;
    }
  }

  template <typename T>
  static bool FreeObject(jlong id) {
    MutexLock l(&mutex_);
    auto it = objects_.find(id);
    if (it != objects_.end()) {
      auto shared = std::static_pointer_cast<T>(it->second);
      objects_.erase(it);
      return true;
    } else {
      return false;
    }
  }

 private:
  static std::unordered_map<jlong, std::shared_ptr<void>> objects_;
  static std::atomic_ulong counter_;
  static port::Mutex mutex_;
};

}  // namespace jni
}  // namespace ROCKSDB_NAMESPACE
