/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEFAULT_CAMERA_HAL_STATIC_PROPERTIES_H_
#define DEFAULT_CAMERA_HAL_STATIC_PROPERTIES_H_

#include <memory>

#include "common.h"
#include "metadata/metadata_reader.h"

namespace default_camera_hal {

// StaticProperties provides a wrapper around useful static metadata entries.
class StaticProperties {
 public:
  // Use this method to create StaticProperties objects.
  // Functionally equivalent to "new StaticProperties",
  // except that it may return nullptr in case of failure (missing entries).
  static StaticProperties* NewStaticProperties(
      std::unique_ptr<const MetadataReader> metadata_reader);
  virtual ~StaticProperties(){};

  int facing() const { return facing_; };
  int orientation() const { return orientation_; };
  // Carrying on the promise of the underlying reader,
  // the returned pointer is valid only as long as this object is alive.
  const camera_metadata_t* raw_metadata() const {
    return metadata_reader_->raw_metadata();
  };

 private:
  // Constructor private to allow failing on bad input.
  // Use NewStaticProperties instead.
  StaticProperties(std::unique_ptr<const MetadataReader> metadata_reader,
                   int facing,
                   int orientation);

  const std::unique_ptr<const MetadataReader> metadata_reader_;
  const int facing_;
  const int orientation_;

  DISALLOW_COPY_AND_ASSIGN(StaticProperties);
};

}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_STATIC_PROPERTIES_H_
