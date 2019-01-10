/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "arc/jpeg_compressor.h"

#include <memory>

#include "arc/common.h"

namespace arc {

// The destination manager that can access |result_buffer_| in JpegCompressor.
struct destination_mgr {
 public:
  struct jpeg_destination_mgr mgr;
  JpegCompressor* compressor;
};

JpegCompressor::JpegCompressor() {}

JpegCompressor::~JpegCompressor() {}

bool JpegCompressor::CompressImage(const void* image, int width, int height,
                                   int quality, const void* app1Buffer,
                                   unsigned int app1Size) {
  if (width % 8 != 0 || height % 2 != 0) {
    LOGF(ERROR) << "Image size can not be handled: " << width << "x" << height;
    return false;
  }

  result_buffer_.clear();
  if (!Encode(image, width, height, quality, app1Buffer, app1Size)) {
    return false;
  }
  LOGF(INFO) << "Compressed JPEG: " << (width * height * 12) / 8 << "[" << width
             << "x" << height << "] -> " << result_buffer_.size() << " bytes";
  return true;
}

const void* JpegCompressor::GetCompressedImagePtr() {
  return result_buffer_.data();
}

size_t JpegCompressor::GetCompressedImageSize() {
  return result_buffer_.size();
}

void JpegCompressor::InitDestination(j_compress_ptr cinfo) {
  destination_mgr* dest = reinterpret_cast<destination_mgr*>(cinfo->dest);
  std::vector<JOCTET>& buffer = dest->compressor->result_buffer_;
  buffer.resize(kBlockSize);
  dest->mgr.next_output_byte = &buffer[0];
  dest->mgr.free_in_buffer = buffer.size();
}

boolean JpegCompressor::EmptyOutputBuffer(j_compress_ptr cinfo) {
  destination_mgr* dest = reinterpret_cast<destination_mgr*>(cinfo->dest);
  std::vector<JOCTET>& buffer = dest->compressor->result_buffer_;
  size_t oldsize = buffer.size();
  buffer.resize(oldsize + kBlockSize);
  dest->mgr.next_output_byte = &buffer[oldsize];
  dest->mgr.free_in_buffer = kBlockSize;
  return true;
}

void JpegCompressor::TerminateDestination(j_compress_ptr cinfo) {
  destination_mgr* dest = reinterpret_cast<destination_mgr*>(cinfo->dest);
  std::vector<JOCTET>& buffer = dest->compressor->result_buffer_;
  buffer.resize(buffer.size() - dest->mgr.free_in_buffer);
}

void JpegCompressor::OutputErrorMessage(j_common_ptr cinfo) {
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message)(cinfo, buffer);
  LOGF(ERROR) << buffer;
}

bool JpegCompressor::Encode(const void* inYuv, int width, int height,
                            int jpegQuality, const void* app1Buffer,
                            unsigned int app1Size) {
  jpeg_compress_struct cinfo;
  jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr);
  // Override output_message() to print error log with ALOGE().
  cinfo.err->output_message = &OutputErrorMessage;
  jpeg_create_compress(&cinfo);
  SetJpegDestination(&cinfo);

  SetJpegCompressStruct(width, height, jpegQuality, &cinfo);
  jpeg_start_compress(&cinfo, TRUE);

  if (app1Buffer != nullptr && app1Size > 0) {
    jpeg_write_marker(&cinfo, JPEG_APP0 + 1,
                      static_cast<const JOCTET*>(app1Buffer), app1Size);
  }

  if (!Compress(&cinfo, static_cast<const uint8_t*>(inYuv))) {
    return false;
  }
  jpeg_finish_compress(&cinfo);
  return true;
}

void JpegCompressor::SetJpegDestination(jpeg_compress_struct* cinfo) {
  destination_mgr* dest =
      static_cast<struct destination_mgr*>((*cinfo->mem->alloc_small)(
          (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(destination_mgr)));
  dest->compressor = this;
  dest->mgr.init_destination = &InitDestination;
  dest->mgr.empty_output_buffer = &EmptyOutputBuffer;
  dest->mgr.term_destination = &TerminateDestination;
  cinfo->dest = reinterpret_cast<struct jpeg_destination_mgr*>(dest);
}

void JpegCompressor::SetJpegCompressStruct(int width, int height, int quality,
                                           jpeg_compress_struct* cinfo) {
  cinfo->image_width = width;
  cinfo->image_height = height;
  cinfo->input_components = 3;
  cinfo->in_color_space = JCS_YCbCr;
  jpeg_set_defaults(cinfo);

  jpeg_set_quality(cinfo, quality, TRUE);
  jpeg_set_colorspace(cinfo, JCS_YCbCr);
  cinfo->raw_data_in = TRUE;
  cinfo->dct_method = JDCT_IFAST;

  // Configure sampling factors. The sampling factor is JPEG subsampling 420
  // because the source format is YUV420.
  cinfo->comp_info[0].h_samp_factor = 2;
  cinfo->comp_info[0].v_samp_factor = 2;
  cinfo->comp_info[1].h_samp_factor = 1;
  cinfo->comp_info[1].v_samp_factor = 1;
  cinfo->comp_info[2].h_samp_factor = 1;
  cinfo->comp_info[2].v_samp_factor = 1;
}

bool JpegCompressor::Compress(jpeg_compress_struct* cinfo, const uint8_t* yuv) {
  JSAMPROW y[kCompressBatchSize];
  JSAMPROW cb[kCompressBatchSize / 2];
  JSAMPROW cr[kCompressBatchSize / 2];
  JSAMPARRAY planes[3]{y, cb, cr};

  size_t y_plane_size = cinfo->image_width * cinfo->image_height;
  size_t uv_plane_size = y_plane_size / 4;
  uint8_t* y_plane = const_cast<uint8_t*>(yuv);
  uint8_t* u_plane = const_cast<uint8_t*>(yuv + y_plane_size);
  uint8_t* v_plane = const_cast<uint8_t*>(yuv + y_plane_size + uv_plane_size);
  std::unique_ptr<uint8_t[]> empty(new uint8_t[cinfo->image_width]);
  memset(empty.get(), 0, cinfo->image_width);

  while (cinfo->next_scanline < cinfo->image_height) {
    for (int i = 0; i < kCompressBatchSize; ++i) {
      size_t scanline = cinfo->next_scanline + i;
      if (scanline < cinfo->image_height) {
        y[i] = y_plane + scanline * cinfo->image_width;
      } else {
        y[i] = empty.get();
      }
    }
    // cb, cr only have half scanlines
    for (int i = 0; i < kCompressBatchSize / 2; ++i) {
      size_t scanline = cinfo->next_scanline / 2 + i;
      if (scanline < cinfo->image_height / 2) {
        int offset = scanline * (cinfo->image_width / 2);
        cb[i] = u_plane + offset;
        cr[i] = v_plane + offset;
      } else {
        cb[i] = cr[i] = empty.get();
      }
    }

    int processed = jpeg_write_raw_data(cinfo, planes, kCompressBatchSize);
    if (processed != kCompressBatchSize) {
      LOGF(ERROR) << "Number of processed lines does not equal input lines.";
      return false;
    }
  }
  return true;
}

}  // namespace arc
