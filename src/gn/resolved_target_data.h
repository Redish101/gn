// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RESOLVED_TARGET_DATA_H_
#define TOOLS_GN_RESOLVED_TARGET_DATA_H_

#include <memory>

#include "base/containers/span.h"
#include "gn/lib_file.h"
#include "gn/source_dir.h"
#include "gn/target.h"
#include "gn/target_public_pair.h"

class Target;

// A list of (target_ptr, is_public_flag) pairs as returned by methods
// of ResolvedTargetData.
using TargetPublicPairList = base::span<const TargetPublicPair>;

// A class used to compute target-specific data by collecting information
// from its tree of dependencies.
//
// For example, linkable targets can call all_libs() and all_lib_dirs() to
// find the library files and library search paths to add to their final
// linker command string, based on the definitions of the `libs` and `lib_dirs`
// config values of their transitive dependencies.
//
// Values are computed on demand, but memorized by the class instance in order
// to speed up multiple queries for targets that share dependencies.
//
// Usage is:
//
//  1) Create instance.
//
//  2) Call any of the methods to retrieve the value of the corresponding
//     data. For all methods, the input Target instance passed as argument
//     must have been fully resolved (meaning that Target::OnResolved()
//     must have been called and completed). Input target pointers are
//     const and thus are never modified. This allows using multiple
//     ResolvedTargetData instances from the same input graph in multiple
//     threads.
//
class ResolvedTargetData {
 public:
  ResolvedTargetData();
  ~ResolvedTargetData();

  // Move operations
  ResolvedTargetData(ResolvedTargetData&&) noexcept;
  ResolvedTargetData& operator=(ResolvedTargetData&&);

  // Retrieve information about link-time libraries needed by this target.
  struct LibInfo {
    base::span<const SourceDir> all_lib_dirs;
    base::span<const LibFile> all_libs;
  };
  LibInfo GetLibInfo(const Target*) const;

  // The list of all library directory search path to add to the final link
  // command of linkable binary. For example, if this returns ['dir1', 'dir2']
  // a command for a C++ linker would typically use `-Ldir1 -Ldir2`.
  base::span<const SourceDir> all_lib_dirs(const Target* target) const;

  // The list of all library files to add to the final link command of linkable
  // binaries. For example, if this return s['foo', '/path/to/bar'], the command
  // for a C++ linker would typically use '-lfoo /path/to/bar'.
  base::span<const LibFile> all_libs(const Target* target) const;

  // Retrieve information about link-time OS X frameworks needed by this target.
  struct FrameworkInfo {
    base::span<const SourceDir> all_framework_dirs;
    base::span<const std::string> all_frameworks;
    base::span<const std::string> all_weak_frameworks;
  };
  FrameworkInfo GetFrameworkInfo(const Target* target) const;

  // The list of framework directories search paths to use at link time
  // when generating MacOS or iOS linkable binaries.
  base::span<const SourceDir> all_framework_dirs(const Target* target) const;

  // The list of framework names to use at link time when generating MacOS
  // or iOS linkable binaries.
  base::span<const std::string> all_frameworks(const Target* target) const;

  // The list of weak framework names to use at link time when generating MacOS
  // or iOS linkable binaries.
  base::span<const std::string> all_weak_frameworks(const Target* target) const;

  // Retrieve a set of hard dependencies for this target.
  // These dependencies require the generation of a Ninja in-order input,
  // see Target::hard_dep() for details.
  TargetSet recursive_hard_deps(const Target* target) const;

  // Retrieve an ordered list of (target, is_public) pairs for all link-time
  // libraries inherited by this target.
  TargetPublicPairList inherited_libraries(const Target* target) const;

  // Retrieves an ordered list of (target, is_public) paris for all link-time
  // libraries for Rust-specific binary targets.
  TargetPublicPairList rust_transitive_inherited_libs(
      const Target* target) const;

 private:
  class Impl;

  Impl* GetImpl() const;

  mutable std::unique_ptr<Impl> impl_;
};

#endif  // TOOLS_GN_RESOLVED_TARGET_DATA_H_