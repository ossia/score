#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Zero-copy DMA-BUF → QRhi decoder for AVFrames in the
// AV_PIX_FMT_DRM_PRIME transport convention.
//
// Backend dispatch (chosen at init based on live QRhi backend):
//   - QRhi::Vulkan      → DMABufPlaneImporter (VK_KHR_external_memory_fd
//                         + VK_EXT_image_drm_format_modifier)
//   - QRhi::OpenGLES2   → EglDmaBufImporter (EGL_EXT_image_dma_buf_import_modifiers
//                         + GL_OES_EGL_image). EGL-backed contexts only;
//                         GLX falls back to no-op.
//   - everything else   → no-op shader (black). Producer-side memcpy
//                         in PipewireInputDevice covers those.
//
// Format dispatch (chosen at init based on `format.hwaccel_sw_format`):
//   - Packed RGB (BGRA8 / RGBA8 / RGB10A2 / RGBA16F) → single
//     sampler, RGBA pass-through shader. Producer-side fourcc maps
//     to the matching native format (VK_FORMAT_B8G8R8A8_UNORM etc.
//     for Vulkan; DRM_FORMAT_* fourcc for EGL).
//   - NV12 / P010 → 2 samplers (Y as R8/R16, UV as RG8/RG16),
//     standard YUV→RGB shader (reused from NV12.hpp).
//   - I420 / YUV420P → 3 samplers (Y/U/V all R8), YUV420 shader
//     (reused from YUV420.hpp).
//
// The decoder uses a 2-slot import ring per plane so the GPU has
// finished sampling slot N before the importer reuses it.

#if defined(__linux__)
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/interop/DMABufImport.hpp>
#include <Gfx/Graph/interop/EglDmaBufImport.hpp>

#include <Video/VideoInterface.hpp>

#include <score/gfx/Vulkan.hpp>

extern "C" {
#include <libavformat/avformat.h>
#if __has_include(<libavutil/hwcontext_drm.h>)
#include <libavutil/hwcontext_drm.h>
#define SCORE_DRMPRIME_HAS_HWCONTEXT_DRM 1
#endif
}

#if defined(SCORE_DRMPRIME_HAS_HWCONTEXT_DRM) \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

#if QT_HAS_VULKAN
#include <QtGui/private/qrhivulkan_p.h>
#include <vulkan/vulkan.h>
#endif

namespace score::gfx
{

/** Zero-copy decoder for DMA-BUF frames delivered as
 *  AV_PIX_FMT_DRM_PRIME AVFrames. Dispatches between Vulkan/EGL
 *  importers and between packed/planar shader variants. */
struct DRMPrimeDecoder : GPUVideoDecoder
{
  Video::ImageFormat& m_decoder;

  enum class Backend
  {
    None,
    Vulkan,
    OpenGL,
  };
  Backend m_backend{Backend::None};

  // SW format family — determines shader + plane count.
  enum class Family
  {
    PackedRGB, // 1 plane: BGRA/RGBA/RGB10A2/RGBA16F
    NV12,      // 2 planes: R8 Y + RG8 UV (or R16/RG16 for P010)
    I420,      // 3 planes: R8 Y + R8 U + R8 V
  };
  Family m_family{Family::PackedRGB};
  bool m_is_10bit{false}; // P010 vs NV12

#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
  DMABufPlaneImporter m_vk_importer;
  static constexpr int kNumVkSlots = 2;
  struct VkSlot
  {
    DMABufPlaneImporter::PlaneImport planes[3]{};
  };
  VkSlot m_vk_slots[kNumVkSlots]{};
  int m_vk_slotIdx{0};
  VkFormat m_vk_plane_fmt[3]{}; // per-plane VkFormat
#endif

  EglDmaBufImporter m_gl_importer;
  static constexpr int kNumGlSlots = 2;
  struct GlSlot
  {
    EglDmaBufImporter::PlaneImport planes[3]{};
  };
  GlSlot m_gl_slots[kNumGlSlots]{};
  int m_gl_slotIdx{0};
  unsigned int m_gl_textures[3]{0, 0, 0}; // persistent GL ids per plane
  uint32_t m_gl_plane_fourcc[3]{};        // per-plane DRM fourcc

  /** True if any backend is available against this QRhi. */
  static bool isAvailable(QRhi& rhi) noexcept
  {
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
    if(DMABufPlaneImporter::isAvailable(rhi))
      return true;
#endif
    if(EglDmaBufImporter::isAvailable(rhi))
      return true;
    return false;
  }

  explicit DRMPrimeDecoder(Video::ImageFormat& d)
      : m_decoder{d}
  {
    // Determine format family from hwaccel_sw_format. Falls back to
    // PackedRGB if not set; the descriptor inspection at exec time
    // can still detect mismatch.
    switch(d.hwaccel_sw_format)
    {
      case AV_PIX_FMT_NV12:
        m_family = Family::NV12;
        m_is_10bit = false;
        break;
      case AV_PIX_FMT_P010LE:
        m_family = Family::NV12;
        m_is_10bit = true;
        break;
      case AV_PIX_FMT_YUV420P:
        m_family = Family::I420;
        m_is_10bit = false;
        break;
      default:
        m_family = Family::PackedRGB;
        break;
    }
  }

  ~DRMPrimeDecoder() override
  {
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
    if(m_backend == Backend::Vulkan)
    {
      for(auto& slot : m_vk_slots)
        for(auto& p : slot.planes)
          m_vk_importer.cleanupPlane(p);
    }
#endif
    if(m_backend == Backend::OpenGL)
    {
      for(auto& slot : m_gl_slots)
        for(auto& p : slot.planes)
          m_gl_importer.cleanupPlane(p);
      if(auto* ctx = QOpenGLContext::currentContext())
      {
        if(auto* funcs = ctx->extraFunctions())
        {
          for(unsigned int& tex : m_gl_textures)
            if(tex != 0)
              funcs->glDeleteTextures(1, &tex);
        }
      }
      for(unsigned int& tex : m_gl_textures)
        tex = 0;
    }
  }

  static constexpr auto packed_frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

    layout(binding=3) uniform sampler2D y_tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 tex) {
      vec4 processed = tex;
      { %1 }
      return processed;
    }

    void main () {
      fragColor = processTexture(texture(y_tex, v_texcoord));
    })_";

  /** Number of planes for the current family. */
  int planeCount() const noexcept
  {
    switch(m_family)
    {
      case Family::PackedRGB: return 1;
      case Family::NV12:      return 2;
      case Family::I420:      return 3;
    }
    return 1;
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;

    // -- Pick backend ----------------------------------------------
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
    if(DMABufPlaneImporter::isAvailable(rhi))
    {
      m_vk_importer.init(rhi);
      m_backend = Backend::Vulkan;
    }
    else
#endif
        if(EglDmaBufImporter::isAvailable(rhi))
    {
      if(m_gl_importer.init(rhi))
        m_backend = Backend::OpenGL;
    }

    if(m_backend == Backend::None)
    {
      return score::gfx::makeShaders(
          r.state, vertexShader(), EmptyDecoder::hashtag_no_filter);
    }

    // -- Create per-plane sampler/texture pairs --------------------
    const auto w = m_decoder.width, h = m_decoder.height;

    auto makeSamplerTexture = [&](QRhiTexture::Format fmt, QSize size) {
      auto* tex = rhi.newTexture(fmt, size, 1, QRhiTexture::Flag{});
      tex->create();
      auto* sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    };

    switch(m_family)
    {
      case Family::PackedRGB:
        // Placeholder format; createFrom replaces the native handle.
        makeSamplerTexture(QRhiTexture::BGRA8, QSize{w, h});
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
        m_vk_plane_fmt[0] = VK_FORMAT_UNDEFINED; // chosen at first exec
#endif
        break;

      case Family::NV12:
        if(m_is_10bit)
        {
          // P010: R16 (Y) + RG16 (UV at half res).
          makeSamplerTexture(QRhiTexture::R16, QSize{w, h});
          makeSamplerTexture(QRhiTexture::RG16, QSize{w / 2, h / 2});
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
          m_vk_plane_fmt[0] = VK_FORMAT_R16_UNORM;
          m_vk_plane_fmt[1] = VK_FORMAT_R16G16_UNORM;
#endif
          // DRM fourccs (R16, GR1616) — Mesa-defined.
          m_gl_plane_fourcc[0] = 0x20363152u; // DRM_FORMAT_R16   'R16 '
          m_gl_plane_fourcc[1] = 0x36315247u; // DRM_FORMAT_GR1616 'GR16'
        }
        else
        {
          // NV12: R8 (Y) + RG8 (UV at half res).
          makeSamplerTexture(QRhiTexture::R8, QSize{w, h});
          makeSamplerTexture(QRhiTexture::RG8, QSize{w / 2, h / 2});
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
          m_vk_plane_fmt[0] = VK_FORMAT_R8_UNORM;
          m_vk_plane_fmt[1] = VK_FORMAT_R8G8_UNORM;
#endif
          m_gl_plane_fourcc[0] = 0x20203852u; // DRM_FORMAT_R8    'R8  '
          m_gl_plane_fourcc[1] = 0x38385247u; // DRM_FORMAT_GR88  'GR88'
        }
        break;

      case Family::I420:
        // Three R8 planes: Y full-res, U/V at half-res.
        makeSamplerTexture(QRhiTexture::R8, QSize{w, h});
        makeSamplerTexture(QRhiTexture::R8, QSize{w / 2, h / 2});
        makeSamplerTexture(QRhiTexture::R8, QSize{w / 2, h / 2});
#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
        m_vk_plane_fmt[0] = VK_FORMAT_R8_UNORM;
        m_vk_plane_fmt[1] = VK_FORMAT_R8_UNORM;
        m_vk_plane_fmt[2] = VK_FORMAT_R8_UNORM;
#endif
        m_gl_plane_fourcc[0] = 0x20203852u;
        m_gl_plane_fourcc[1] = 0x20203852u;
        m_gl_plane_fourcc[2] = 0x20203852u;
        break;
    }

    // For the OpenGL path: pre-create persistent GL texture ids and
    // wire each QRhiTexture to wrap one. The EGL importer re-targets
    // their storage each frame via glEGLImageTargetTexture2DOES.
    if(m_backend == Backend::OpenGL)
    {
      const int n = planeCount();
      for(int i = 0; i < n; ++i)
      {
        m_gl_textures[i] = score::gfx::createLinearClampGlTexture2D();
        if(m_gl_textures[i])
          samplers[i].texture->createFrom(
              QRhiTexture::NativeTexture{quint64(m_gl_textures[i]), 0});
      }
    }

    // -- Build the right shader for the family --------------------
    switch(m_family)
    {
      case Family::PackedRGB:
        return score::gfx::makeShaders(
            r.state, vertexShader(), QString(packed_frag).arg(""));

      case Family::NV12:
      {
        // Reuse NV12Decoder's filter epilogue. Builds
        //   y from t0.r, uv from t1.rg, yuv → rgb conversion.
        QString frag = NV12Decoder::nv12_filter_prologue;
        if(m_is_10bit)
          frag += "    vec3 yuv = vec3(y * 64.0, u * 64.0, v * 64.0);\n";
        else
          frag += "    vec3 yuv = vec3(y, u, v);\n";
        frag += NV12Decoder::nv12_filter_epilogue;
        return score::gfx::makeShaders(
            r.state, vertexShader(), frag.arg("").arg(colorMatrix(m_decoder)));
      }

      case Family::I420:
        return score::gfx::makeShaders(
            r.state, vertexShader(),
            QString(YUV420Decoder::frag).arg("").arg(colorMatrix(m_decoder)));
    }
    return score::gfx::makeShaders(
        r.state, vertexShader(), EmptyDecoder::hashtag_no_filter);
  }

#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
  /** Packed RGB DRM fourcc → Vulkan packed RGB format. */
  static VkFormat vkPackedFmtFromDrmFourcc(uint32_t fourcc) noexcept
  {
    switch(fourcc)
    {
      case 0x34325241: return VK_FORMAT_B8G8R8A8_UNORM; // ARGB8888
      case 0x34325258: return VK_FORMAT_B8G8R8A8_UNORM; // XRGB8888
      case 0x34324241: return VK_FORMAT_R8G8B8A8_UNORM; // ABGR8888
      case 0x34324258: return VK_FORMAT_R8G8B8A8_UNORM; // XBGR8888
      case 0x30335241: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
      case 0x30335258: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
      case 0x30334241: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      case 0x30334258: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      case 0x48344241: return VK_FORMAT_R16G16B16A16_SFLOAT;
      case 0x48344258: return VK_FORMAT_R16G16B16A16_SFLOAT;
      default:         return VK_FORMAT_UNDEFINED;
    }
  }
#endif

  void exec(
      RenderList&, QRhiResourceUpdateBatch& /*res*/, AVFrame& frame) override
  {
    if(m_backend == Backend::None || frame.format != AV_PIX_FMT_DRM_PRIME)
      return;

    const auto* desc
        = reinterpret_cast<const AVDRMFrameDescriptor*>(frame.data[0]);
    if(!desc || desc->nb_objects < 1 || desc->nb_layers < 1)
      return;

    // Resolve per-plane (object_index, offset, pitch, width, height).
    struct PlaneRef
    {
      int obj_idx;
      ptrdiff_t offset;
      ptrdiff_t pitch;
      int w, h;
    };
    PlaneRef p[3]{};
    const int np = planeCount();

    // Two layouts: single-layer-multi-plane (typical) or
    // multi-layer-single-plane (split-buffer producer).
    if(desc->nb_layers == 1 && desc->layers[0].nb_planes >= np)
    {
      const auto& layer = desc->layers[0];
      for(int i = 0; i < np; ++i)
      {
        const auto& plane = layer.planes[i];
        p[i].obj_idx = plane.object_index;
        p[i].offset = plane.offset;
        p[i].pitch = plane.pitch;
      }
    }
    else if(desc->nb_layers >= np)
    {
      for(int i = 0; i < np; ++i)
      {
        const auto& plane = desc->layers[i].planes[0];
        p[i].obj_idx = plane.object_index;
        p[i].offset = plane.offset;
        p[i].pitch = plane.pitch;
      }
    }
    else
    {
      qDebug() << "DRMPrimeDecoder: unexpected DRM layout — layers"
               << desc->nb_layers << "but need" << np << "planes";
      return;
    }

    // Per-plane dimensions: Y plane full size; chroma planes half-res
    // for NV12/I420 (semi or 4:2:0).
    p[0].w = m_decoder.width;
    p[0].h = m_decoder.height;
    if(m_family == Family::NV12)
    {
      p[1].w = m_decoder.width / 2;
      p[1].h = m_decoder.height / 2;
    }
    else if(m_family == Family::I420)
    {
      p[1].w = p[2].w = m_decoder.width / 2;
      p[1].h = p[2].h = m_decoder.height / 2;
    }

#if QT_HAS_VULKAN && defined(VK_EXT_image_drm_format_modifier) \
    && defined(VK_KHR_external_memory_fd)
    if(m_backend == Backend::Vulkan)
    {
      // For packed RGB, derive the VkFormat from the descriptor's
      // fourcc on first exec.
      if(m_family == Family::PackedRGB
         && m_vk_plane_fmt[0] == VK_FORMAT_UNDEFINED)
      {
        m_vk_plane_fmt[0] = vkPackedFmtFromDrmFourcc(desc->layers[0].format);
        if(m_vk_plane_fmt[0] == VK_FORMAT_UNDEFINED)
        {
          qDebug()
              << "DRMPrimeDecoder: unsupported packed DRM fourcc"
              << Qt::hex << desc->layers[0].format;
          return;
        }
      }

      auto& slot = m_vk_slots[m_vk_slotIdx];
      for(auto& pl : slot.planes)
        m_vk_importer.cleanupPlane(pl);
      m_vk_slotIdx = (m_vk_slotIdx + 1) % kNumVkSlots;

      for(int i = 0; i < np; ++i)
      {
        const auto& obj = desc->objects[p[i].obj_idx];
        if(!m_vk_importer.importPlane(
               slot.planes[i], obj.fd, obj.format_modifier, p[i].offset,
               p[i].pitch, m_vk_plane_fmt[i], p[i].w, p[i].h))
        {
          qDebug() << "DRMPrimeDecoder: Vulkan importPlane failed for plane"
                   << i << "fd" << obj.fd;
          return;
        }
        samplers[i].texture->createFrom(QRhiTexture::NativeTexture{
            quint64(slot.planes[i].image), VK_IMAGE_LAYOUT_UNDEFINED});
      }
      hasFrame = true;
      return;
    }
#endif

    if(m_backend == Backend::OpenGL)
    {
      // For packed RGB on GL, the EGL importer uses the descriptor's
      // fourcc directly (EGL handles format mapping).
      if(m_family == Family::PackedRGB)
        m_gl_plane_fourcc[0] = desc->layers[0].format;

      auto& slot = m_gl_slots[m_gl_slotIdx];
      for(auto& pl : slot.planes)
        m_gl_importer.cleanupPlane(pl);
      m_gl_slotIdx = (m_gl_slotIdx + 1) % kNumGlSlots;

      for(int i = 0; i < np; ++i)
      {
        const auto& obj = desc->objects[p[i].obj_idx];
        if(!m_gl_importer.importPlane(
               slot.planes[i], m_gl_textures[i], obj.fd, obj.format_modifier,
               p[i].offset, p[i].pitch, m_gl_plane_fourcc[i], p[i].w, p[i].h))
        {
          qDebug() << "DRMPrimeDecoder: EGL importPlane failed for plane"
                   << i << "fd" << obj.fd << "fourcc" << Qt::hex
                   << m_gl_plane_fourcc[i];
          return;
        }
      }
      hasFrame = true;
      return;
    }
  }
};

} // namespace score::gfx

#endif // SCORE_DRMPRIME_HAS_HWCONTEXT_DRM && Qt 6.6+
#endif // __linux__
