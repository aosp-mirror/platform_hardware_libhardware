/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_JPEG_COMPRESSOR_H_
#define INCLUDE_ARC_JPEG_COMPRESSOR_H_

// We must include cstdio before jpeglib.h. It is a requirement of libjpeg.
#include <cstdio>
#include <vector>

extern "C" {
#include <jerror.h>
#include <jpeglib.h>
}

namespace arc {

// Encapsulates a converter from YU12 to JPEG format. This class is not
// thread-safe.
class JpegCompressor {
 public:
  JpegCompressor();
  ~JpegCompressor();

  // Compresses YU12 image to JPEG format. After calling this method, call
  // GetCompressedImagePtr() to get the image. |quality| is the resulted jpeg
  // image quality. It ranges from 1 (poorest quality) to 100 (highest quality).
  // |app1Buffer| is the buffer of APP1 segment (exif) which will be added to
  // the compressed image. Returns false if errors occur during compression.
  bool CompressImage(const void* image, int width, int height, int quality,
                     const void* app1Buffer, unsigned int app1Size);

  // Returns the compressed JPEG buffer pointer. This method must be called only
  // after calling CompressImage().
  const void* GetCompressedImagePtr();

  // Returns the compressed JPEG buffer size. This method must be called only
  // after calling CompressImage().
  size_t GetCompressedImageSize();

 private:
  // InitDestination(), EmptyOutputBuffer() and TerminateDestination() are
  // callback functions to be passed into jpeg library.
  static void InitDestination(j_compress_ptr cinfo);
  static boolean EmptyOutputBuffer(j_compress_ptr cinfo);
  static void TerminateDestination(j_compress_ptr cinfo);
  static void OutputErrorMessage(j_common_ptr cinfo);

  // Returns false if errors occur.
  bool Encode(const void* inYuv, int width, int height, int jpegQuality,
              const void* app1Buffer, unsigned int app1Size);
  void SetJpegDestination(jpeg_compress_struct* cinfo);
  void SetJpegCompressStruct(int width, int height, int quality,
                             jpeg_compress_struct* cinfo);
  // Returns false if errors occur.
  bool Compress(jpeg_compress_struct* cinfo, const uint8_t* yuv);

  // The block size for encoded jpeg image buffer.
  static const int kBlockSize = 16384;
  // Process 16 lines of Y and 16 lines of U/V each time.
  // We must pass at least 16 scanlines according to libjpeg documentation.
  static const int kCompressBatchSize = 16;

  // The buffer that holds the compressed result.
  std::vector<JOCTET> result_buffer_;
};

}  // namespace arc

#endif  // INCLUDE_ARC_JPEG_COMPRESSOR_H_
