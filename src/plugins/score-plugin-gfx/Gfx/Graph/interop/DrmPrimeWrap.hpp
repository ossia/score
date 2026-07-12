#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Protocol-agnostic AVDRMFrameDescriptor construction for DMA-BUF video
// buffers, hoisted from Gfx/Pipewire/PipewireInputDevice.cpp. Any transport
// that hands us dmabuf fds + per-plane chunk metadata (PipeWire today;
// TODO: videoio vendors and GStreamer can reuse this for their DRM-prime
// capture paths) can build an AV_PIX_FMT_DRM_PRIME AVFrame from it.
//
// The caller owns frame-release semantics: buildDrmDescriptor() returns a
// calloc'd descriptor the caller must either free() or hand to
// av_buffer_create with a release callback (see wrapDescriptorAsDrmPrime).

#if defined(__linux__)

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
}

#include <Gfx/Graph/interop/DrmFourcc.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace score::gfx::interop
{

/** One transport buffer block: a dmabuf fd plus its chunk metadata.
 *  Mirrors pipewire's spa_data/spa_chunk fields without the dependency. */
struct DrmPlaneSpan
{
  int fd{-1};
  uint32_t maxsize{0};
  int32_t stride{0};
  uint32_t offset{0};
};

/** Build an AVDRMFrameDescriptor from N transport blocks.
 *
 * Handles the three producer layout shapes:
 *   - nBlocks == 1, packed fourcc  -> 1 object, 1 layer, 1 plane
 *   - nBlocks == 1, planar fourcc  -> 1 object, 1 layer, N planes laid out
 *     sequentially inside the object using the block's stride (NV12/P010
 *     two-plane, YUV420/YVU420 three-plane) — the common pipewire shape
 *     (screencast portal, libcamera).
 *   - nBlocks > 1                  -> N objects, 1 layer, one plane per
 *     object (split-buffer hardware).
 *
 * Returns nullptr for fourcc == 0 or nBlocks < 1. Caller frees. */
inline AVDRMFrameDescriptor* buildDrmDescriptor(
    const DrmPlaneSpan* blocks, int nBlocks, uint32_t fourcc,
    uint64_t modifier, int width, int height) noexcept
{
  if(!blocks || nBlocks < 1 || fourcc == 0)
    return nullptr;

  auto* desc = static_cast<AVDRMFrameDescriptor*>(
      std::calloc(1, sizeof(AVDRMFrameDescriptor)));
  if(!desc)
    return nullptr;

  if(nBlocks == 1)
  {
    desc->nb_objects = 1;
    desc->objects[0].fd = blocks[0].fd;
    desc->objects[0].size = blocks[0].maxsize;
    desc->objects[0].format_modifier = modifier;

    desc->nb_layers = 1;
    desc->layers[0].format = fourcc;

    const int srcStride = blocks[0].stride;
    const std::size_t srcOffset = blocks[0].offset;

    switch(fourcc)
    {
      case DRM_NV12:
      case DRM_P010:
      {
        // 2 planes: Y at offset 0, UV interleaved at offset Y_size.
        const int pitch = srcStride > 0
                              ? srcStride
                              : (fourcc == DRM_P010 ? width * 2 : width);
        const std::size_t y_size = std::size_t(pitch) * height;
        desc->layers[0].nb_planes = 2;
        desc->layers[0].planes[0].object_index = 0;
        desc->layers[0].planes[0].offset = srcOffset;
        desc->layers[0].planes[0].pitch = pitch;
        desc->layers[0].planes[1].object_index = 0;
        desc->layers[0].planes[1].offset = srcOffset + y_size;
        desc->layers[0].planes[1].pitch = pitch; // UV interleaved: same pitch
        break;
      }
      case DRM_YUV420:
      case DRM_YVU420:
      {
        // 3 planes at offsets 0, y_size, y_size + uv_size. For YVU420 the
        // middle plane is V — the fourcc already says so; no swap here.
        const int yPitch = srcStride > 0 ? srcStride : width;
        const int uvPitch = yPitch / 2;
        const std::size_t y_size = std::size_t(yPitch) * height;
        const std::size_t uv_size = std::size_t(uvPitch) * (height / 2);
        desc->layers[0].nb_planes = 3;
        desc->layers[0].planes[0].object_index = 0;
        desc->layers[0].planes[0].offset = srcOffset;
        desc->layers[0].planes[0].pitch = yPitch;
        desc->layers[0].planes[1].object_index = 0;
        desc->layers[0].planes[1].offset = srcOffset + y_size;
        desc->layers[0].planes[1].pitch = uvPitch;
        desc->layers[0].planes[2].object_index = 0;
        desc->layers[0].planes[2].offset = srcOffset + y_size + uv_size;
        desc->layers[0].planes[2].pitch = uvPitch;
        break;
      }
      default:
      {
        // Single-plane packed.
        desc->layers[0].nb_planes = 1;
        desc->layers[0].planes[0].object_index = 0;
        desc->layers[0].planes[0].offset = srcOffset;
        desc->layers[0].planes[0].pitch = srcStride;
        break;
      }
    }
  }
  else
  {
    // Split planes across separate DMA-BUFs: N objects + 1 layer with one
    // plane per object. Vulkan (VK_EXT_image_drm_format_modifier) and EGL
    // (EGL_EXT_image_dma_buf_import_modifiers) both accept this shape.
    const int n = std::min(nBlocks, int(AV_DRM_MAX_PLANES));
    desc->nb_objects = n;
    desc->nb_layers = 1;
    desc->layers[0].format = fourcc;
    desc->layers[0].nb_planes = n;
    for(int i = 0; i < n; ++i)
    {
      desc->objects[i].fd = blocks[i].fd;
      desc->objects[i].size = blocks[i].maxsize;
      desc->objects[i].format_modifier = modifier;
      desc->layers[0].planes[i].object_index = i;
      desc->layers[0].planes[i].offset = blocks[i].offset;
      desc->layers[0].planes[i].pitch = blocks[i].stride;
    }
  }
  return desc;
}

/** Wrap a descriptor as an AV_PIX_FMT_DRM_PRIME AVFrame. On success the
 *  frame's buf[0] owns `desc` via `release(opaque, ...)`; on failure the
 *  descriptor is freed and nullptr returned (the caller's opaque is NOT
 *  cleaned up — check the return before transferring ownership). */
inline AVFrame* wrapDescriptorAsDrmPrime(
    AVDRMFrameDescriptor* desc, int width, int height,
    void (*release)(void*, uint8_t*), void* opaque) noexcept
{
  AVFrame* f = av_frame_alloc();
  if(!f)
  {
    std::free(desc);
    return nullptr;
  }
  f->format = AV_PIX_FMT_DRM_PRIME;
  f->width = width;
  f->height = height;
  f->data[0] = reinterpret_cast<uint8_t*>(desc);
  f->buf[0] = av_buffer_create(
      reinterpret_cast<uint8_t*>(desc), sizeof(*desc), release, opaque,
      AV_BUFFER_FLAG_READONLY);
  if(!f->buf[0])
  {
    std::free(desc);
    av_frame_free(&f);
    return nullptr;
  }
  return f;
}

} // namespace score::gfx::interop

#endif // __linux__
