// Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "rocksdb/utilities/object_registry.h"

#include <ctype.h>

#include "logging/logging.h"
#include "rocksdb/customizable.h"
#include "rocksdb/env.h"
#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {
#ifndef ROCKSDB_LITE
size_t ObjectLibrary::PatternEntry::MatchPatternAt(
    size_t start, Quantifier mode, const std::string &target, size_t tlen,
    const std::string &pattern) const {
  size_t plen = pattern.size();
  // See if there is enough space.  If so, find the pattern
  if (tlen < start + plen) {
    return std::string::npos;  // not enough space left
  } else if (mode == kMatchExact) {
    // Exact mode means the next thing we are looking for is the pattern
    if (target.compare(start, plen, pattern) != 0) {
      return std::string::npos;
    } else {
      return start + plen;  // Found the pattern, return where we found it
    }
  } else {
    auto pos = start + 1;
    if (!pattern.empty()) {
      pos = target.find(pattern, pos);
    }
    if (pos == std::string::npos) {
      return pos;
    } else if (mode == kMatchNumeric) {
      // If it is numeric, everything up to the match must be a number
      while (start < pos) {
        if (!isdigit(target[start++])) {
          return std::string::npos;
        }
      }
    }
    return pos + plen;
  }
}

bool ObjectLibrary::PatternEntry::MatchesPattern(const std::string &name,
                                                 size_t nlen,
                                                 const std::string &target,
                                                 size_t tlen) const {
  if (patterns_.empty()) {
    assert(pattern_optional_);  // If there are no patterns, it must be only a
                                // nmae
    return nlen == tlen && name == target;
  } else if (nlen == tlen) {  // The lengths are the same
    return pattern_optional_ && name == target;
  } else if (tlen < nlen + plength_) {
    // The target is not long enough
    return false;
  } else if (target.compare(0, nlen, name) != 0) {
    return false;  // Target does not start with name
  } else {
    // Loop through all of the patterns one at a time matching them.
    // Note that we first match the pattern and then its quantifiers.
    // Since we expect the pattern first, we start with an exact match
    // Subsequent matches will use the quantifier of the previous pattern
    size_t start = nlen;
    auto mode = kMatchExact;
    for (size_t idx = 0; idx < patterns_.size(); ++idx) {
      const auto &pattern = patterns_[idx];
      start = MatchPatternAt(start, mode, target, tlen, pattern.first);
      if (start == std::string::npos) {
        return false;
      } else {
        mode = pattern.second;
      }
    }
    // We have matched all of the pattern entries.  Now check that what is left
    // unmatched in the target is acceptable.
    if (mode == kMatchExact) {
      return (start == tlen);
    } else if (start >= tlen) {
      return false;
    } else if (mode == kMatchNumeric) {
      while (start < tlen) {
        if (!isdigit(target[start++])) {
          return false;
        }
      }
    }
  }
  return true;
}

bool ObjectLibrary::PatternEntry::Matches(const std::string &target) const {
  auto tlen = target.size();
  if (MatchesPattern(name_, nlength_, target, tlen)) {
    return true;
  } else if (!names_.empty()) {
    for (const auto &alt : names_) {
      if (MatchesPattern(alt, alt.size(), target, tlen)) {
        return true;
      }
    }
  }
  return false;
}

size_t ObjectLibrary::GetFactoryCount(size_t *types) const {
  std::unique_lock<std::mutex> lock(mu_);
  *types = factories_.size();
  size_t factories = 0;
  for (const auto &e : factories_) {
    factories += e.second.size();
  }
  return factories;
}

void ObjectLibrary::Dump(Logger *logger) const {
  std::unique_lock<std::mutex> lock(mu_);
  for (const auto &iter : factories_) {
    ROCKS_LOG_HEADER(logger, "    Registered factories for type[%s] ",
                     iter.first.c_str());
    bool printed_one = false;
    for (const auto &e : iter.second) {
      ROCKS_LOG_HEADER(logger, "%c %s", (printed_one) ? ',' : ':', e->Name());
      printed_one = true;
    }
  }
  ROCKS_LOG_HEADER(logger, "\n");
}

// Returns the Default singleton instance of the ObjectLibrary
// This instance will contain most of the "standard" registered objects
std::shared_ptr<ObjectLibrary> &ObjectLibrary::Default() {
  static std::shared_ptr<ObjectLibrary> instance =
      std::make_shared<ObjectLibrary>("default");
  return instance;
}

std::shared_ptr<ObjectRegistry> ObjectRegistry::Default() {
  static std::shared_ptr<ObjectRegistry> instance(
      new ObjectRegistry(ObjectLibrary::Default()));
  return instance;
}

std::shared_ptr<ObjectRegistry> ObjectRegistry::NewInstance() {
  return std::make_shared<ObjectRegistry>(Default());
}

std::shared_ptr<ObjectRegistry> ObjectRegistry::NewInstance(
    const std::shared_ptr<ObjectRegistry> &parent) {
  return std::make_shared<ObjectRegistry>(parent);
}

Status ObjectRegistry::SetManagedObject(
    const std::string &type, const std::string &id,
    const std::shared_ptr<Customizable> &object) {
  std::string object_key = ToManagedObjectKey(type, id);
  std::shared_ptr<Customizable> curr;
  if (parent_ != nullptr) {
    curr = parent_->GetManagedObject(type, id);
  }
  if (curr == nullptr) {
    // We did not find the object in any parent.  Update in the current
    std::unique_lock<std::mutex> lock(objects_mutex_);
    auto iter = managed_objects_.find(object_key);
    if (iter != managed_objects_.end()) {  // The object exists
      curr = iter->second.lock();
      if (curr != nullptr && curr != object) {
        return Status::InvalidArgument("Object already exists: ", object_key);
      } else {
        iter->second = object;
      }
    } else {
      // The object does not exist.  Add it
      managed_objects_[object_key] = object;
    }
  } else if (curr != object) {
    return Status::InvalidArgument("Object already exists: ", object_key);
  }
  return Status::OK();
}

std::shared_ptr<Customizable> ObjectRegistry::GetManagedObject(
    const std::string &type, const std::string &id) const {
  {
    std::unique_lock<std::mutex> lock(objects_mutex_);
    auto iter = managed_objects_.find(ToManagedObjectKey(type, id));
    if (iter != managed_objects_.end()) {
      return iter->second.lock();
    }
  }
  if (parent_ != nullptr) {
    return parent_->GetManagedObject(type, id);
  } else {
    return nullptr;
  }
}

Status ObjectRegistry::ListManagedObjects(
    const std::string &type, const std::string &name,
    std::vector<std::shared_ptr<Customizable>> *results) const {
  {
    std::string key = ToManagedObjectKey(type, name);
    std::unique_lock<std::mutex> lock(objects_mutex_);
    for (auto iter = managed_objects_.lower_bound(key);
         iter != managed_objects_.end() && StartsWith(iter->first, key);
         ++iter) {
      auto shared = iter->second.lock();
      if (shared != nullptr) {
        if (name.empty() || shared->IsInstanceOf(name)) {
          results->emplace_back(shared);
        }
      }
    }
  }
  if (parent_ != nullptr) {
    return parent_->ListManagedObjects(type, name, results);
  } else {
    return Status::OK();
  }
}

void ObjectRegistry::Dump(Logger *logger) const {
  {
    std::unique_lock<std::mutex> lock(library_mutex_);
    for (auto iter = libraries_.crbegin(); iter != libraries_.crend(); ++iter) {
      iter->get()->Dump(logger);
    }
  }
  if (parent_ != nullptr) {
    parent_->Dump(logger);
  }
}

#endif  // ROCKSDB_LITE
}  // namespace ROCKSDB_NAMESPACE
