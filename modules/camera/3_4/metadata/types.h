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

#ifndef DEFAULT_CAMERA_HAL_METADATA_TYPES_H_
#define DEFAULT_CAMERA_HAL_METADATA_TYPES_H_

#include <array>
#include <map>
#include <set>

#include <hardware/camera3.h>

namespace default_camera_hal {

// A variety of Structs describing more complex metadata entries.

// StreamSpec describe the attributes of a single stream.
struct StreamSpec {
  int32_t format;
  int32_t width;
  int32_t height;

  StreamSpec(int32_t f, int32_t w, int32_t h)
      : format(f), width(w), height(h) {}
  StreamSpec(const camera3_stream_t* stream)
      : format(stream->format), width(stream->width), height(stream->height) {}

  struct Compare {
    bool operator()(const StreamSpec& left, const StreamSpec& right) const {
      // Base equality/comparison first on format, then on width, then height.
      return left.format < right.format ||
             (left.format == right.format &&
              (left.width < right.width ||
               (left.width == right.width && left.height < right.height)));
    }
  };
};

// StreamConfigurations indicate a possible direction configuration for
// a given set of stream specifications.
typedef std::array<int32_t, 4> RawStreamConfiguration;
struct StreamConfiguration {
  StreamSpec spec;
  int32_t direction;

  StreamConfiguration(const RawStreamConfiguration& raw)
      : spec({raw[0], raw[1], raw[2]}), direction(raw[3]) {}
};

// StreamStallDurations indicate the stall duration (in ns) for
// when a stream with a given set of specifications is used as output.
typedef std::array<int64_t, 4> RawStreamStallDuration;
struct StreamStallDuration {
  StreamSpec spec;
  int64_t duration;

  StreamStallDuration(const RawStreamStallDuration& raw)
      : spec({static_cast<int32_t>(raw[0]),
              static_cast<int32_t>(raw[1]),
              static_cast<int32_t>(raw[2])}),
        duration(raw[3]) {}
};

// Map input formats to their supported reprocess output formats.
typedef std::map<int32_t, std::set<int32_t>> ReprocessFormatMap;

}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_METADATA_TYPES_H_
