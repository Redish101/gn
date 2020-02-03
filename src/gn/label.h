// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_LABEL_H_
#define TOOLS_GN_LABEL_H_

#include <tuple>

#include <stddef.h>

#include "gn/source_dir.h"
#include "gn/string_atom.h"

class Err;
class Value;

// A label represents the name of a target or some other named thing in
// the source path. The label is always absolute and always includes a name
// part, so it starts with a slash, and has one colon.
class Label {
 public:
  Label() = default;

  // Makes a label given an already-separated out path and name.
  // See also Resolve().
  Label(const SourceDir& dir,
        const std::string_view& name,
        const SourceDir& toolchain_dir,
        const std::string_view& toolchain_name);

  // Makes a label with an empty toolchain.
  Label(const SourceDir& dir, const std::string_view& name);

  // Resolves a string from a build file that may be relative to the
  // current directory into a fully qualified label. On failure returns an
  // is_null() label and sets the error.
  static Label Resolve(const SourceDir& current_dir,
                       const Label& current_toolchain,
                       const Value& input,
                       Err* err);

  bool is_null() const { return dir_.is_null(); }

  const SourceDir& dir() const { return dir_; }
  const std::string& name() const { return name_.str(); }
  StringAtom name_atom() const { return name_; }

  const SourceDir& toolchain_dir() const { return toolchain_dir_; }
  const std::string& toolchain_name() const { return toolchain_name_.str(); }
  StringAtom toolchain_name_atom() const { return toolchain_name_; }

  // Returns the current label's toolchain as its own Label.
  Label GetToolchainLabel() const;

  // Returns a copy of this label but with an empty toolchain.
  Label GetWithNoToolchain() const;

  // Formats this label in a way that we can present to the user or expose to
  // other parts of the system. SourceDirs end in slashes, but the user
  // expects names like "//chrome/renderer:renderer_config" when printed. The
  // toolchain is optionally included.
  std::string GetUserVisibleName(bool include_toolchain) const;

  // Like the above version, but automatically includes the toolchain if it's
  // not the default one. Normally the user only cares about the toolchain for
  // non-default ones, so this can make certain output more clear.
  std::string GetUserVisibleName(const Label& default_toolchain) const;

  bool operator==(const Label& other) const {
    return name_.SameAs(other.name_) && dir_ == other.dir_ &&
           toolchain_dir_ == other.toolchain_dir_ &&
           toolchain_name_.SameAs(other.toolchain_name_);
  }
  bool operator!=(const Label& other) const { return !operator==(other); }
  bool operator<(const Label& other) const {
    return std::tie(dir_, name_, toolchain_dir_, toolchain_name_) <
           std::tie(other.dir_, other.name_, other.toolchain_dir_,
                    other.toolchain_name_);
  }

  // Returns true if the toolchain dir/name of this object matches some
  // other object.
  bool ToolchainsEqual(const Label& other) const {
    return toolchain_dir_ == other.toolchain_dir_ &&
           toolchain_name_.SameAs(other.toolchain_name_);
  }

  size_t hash() const { return hash_; }

 private:
  Label(SourceDir dir, StringAtom name) : dir_(dir), name_(name) {}

  Label(SourceDir dir,
        StringAtom name,
        SourceDir toolchain_dir,
        StringAtom toolchain_name)
      : dir_(dir),
        name_(name),
        toolchain_dir_(toolchain_dir),
        toolchain_name_(toolchain_name) {}

  static size_t ComputeHash(SourceDir dir,
                            StringAtom name,
                            SourceDir toolchain_dir,
                            StringAtom toolchain_name) {
    return ((dir.hash() * 131 + name.hash()) * 131 + toolchain_dir.hash()) *
               131 +
           toolchain_name.hash();
  }

  SourceDir dir_;
  StringAtom name_;

  SourceDir toolchain_dir_;
  StringAtom toolchain_name_;

  size_t hash_ = ComputeHash(dir_, name_, toolchain_dir_, toolchain_name_);
};

namespace std {

template <>
struct hash<Label> {
  std::size_t operator()(const Label& v) const { return v.hash(); }
};

}  // namespace std

extern const char kLabels_Help[];

#endif  // TOOLS_GN_LABEL_H_
