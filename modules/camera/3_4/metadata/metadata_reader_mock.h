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

// Mock for metadata readers.

#ifndef DEFAULT_CAMERA_HAL_METADATA_METADATA_READER_MOCK_H_
#define DEFAULT_CAMERA_HAL_METADATA_METADATA_READER_MOCK_H_

#include "metadata_reader.h"

#include <gmock/gmock.h>

namespace default_camera_hal {

class MetadataReaderMock : public MetadataReader {
 public:
  MetadataReaderMock() : MetadataReader(nullptr){};
  MOCK_CONST_METHOD0(raw_metadata, const camera_metadata_t*());
  MOCK_CONST_METHOD1(Facing, int(int*));
  MOCK_CONST_METHOD1(Orientation, int(int*));
  MOCK_CONST_METHOD1(MaxInputStreams, int(int32_t*));
  MOCK_CONST_METHOD3(MaxOutputStreams, int(int32_t*, int32_t*, int32_t*));
  MOCK_CONST_METHOD1(RequestCapabilities, int(std::set<uint8_t>*));
  MOCK_CONST_METHOD1(StreamConfigurations,
                     int(std::vector<StreamConfiguration>*));
  MOCK_CONST_METHOD1(StreamStallDurations,
                     int(std::vector<StreamStallDuration>*));
  MOCK_CONST_METHOD1(ReprocessFormats, int(ReprocessFormatMap*));
};

}  // namespace default_camera_hal

#endif  // DEFAULT_CAMERA_HAL_METADATA_METADATA_READER_MOCK_H_
