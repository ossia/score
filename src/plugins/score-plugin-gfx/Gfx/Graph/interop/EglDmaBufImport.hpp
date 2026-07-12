#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// EGL DMA-BUF importer — sister to DMABufPlaneImporter, for the
// QRhi::OpenGLES2 backend.
//
// Imports a Linux DMA-BUF file descriptor as an EGLImage, then binds
// that EGLImage to a GL texture via glEGLImageTargetTexture2DOES.
// The resulting GL texture is what `QRhiTexture::createFrom` wraps —
// score's render graph samples from it like any other texture, no
// pixel copy.
//
// Requires:
//   - EGL_EXT_image_dma_buf_import_modifiers (the modifier-aware
//     variant of EGL_EXT_image_dma_buf_import — added in 2016)
//   - GL_OES_EGL_image / GL_EXT_EGL_image_external (the
//     glEGLImageTargetTexture2DOES entry point)
//   - An EGL-backed QOpenGLContext. On Linux/Wayland this is always
//     the case. On Linux/X11 it depends on whether Qt's XCB plugin
//     picked the EGL or GLX integration — `isAvailable()` probes
//     via QNativeInterface::QEGLContext and returns false for GLX.
//     Users can force EGL on X11 by setting
//     QT_XCB_GL_INTEGRATION=xcb_egl in their environment.
//
// Lifecycle:
//   - Construct with no args; `init(QRhi&)` opens libEGL via dlopen,
//     resolves the entry points, and grabs the current EGLDisplay.
//   - `importPlane(...)` per frame: creates an EGLImage from the
//     given DMA-BUF FD + modifier + offset + pitch + drm_fourcc + size.
//   - `cleanupPlane(...)` destroys the EGLImage. The caller manages
//     a ring of PlaneImports so the GPU has finished sampling before
//     the EGLImage is destroyed.
//   - Score's DRMPrimeDecoder owns a single persistent GL texture id
//     and re-targets it each frame via glEGLImageTargetTexture2DOES
//     — see DRMPrime.hpp.

#if defined(__linux__)
#include <ossia/detail/dylib_loader.hpp>

#include <QOpenGLContext>
#include <QtGui/qpa/qplatformnativeinterface.h>

#if __has_include(<QtGui/qopenglextrafunctions.h>)
#include <QOpenGLExtraFunctions>
#endif

#include <private/qrhi_p.h>

#include <cstdint>
#include <cstring>
#include <optional>

namespace score::gfx
{

/** Minimal EGL type / function-pointer surface. We don't include
 *  `<EGL/egl.h>` so score can build on systems without libEGL-dev.
 *  All names and signatures match the EGL 1.5 / Khronos definitions. */
struct EglDmaBufImporter
{
  // -- EGL constants (from <EGL/egl.h> + <EGL/eglext.h>) -------------
  static constexpr uint32_t EGL_NONE_ = 0x3038;
  static constexpr uint32_t EGL_WIDTH_ = 0x3057;
  static constexpr uint32_t EGL_HEIGHT_ = 0x3056;
  static constexpr uint32_t EGL_EXTENSIONS_ = 0x3055;
  static constexpr uint32_t EGL_LINUX_DMA_BUF_EXT_ = 0x3270;
  static constexpr uint32_t EGL_LINUX_DRM_FOURCC_EXT_ = 0x3271;
  static constexpr uint32_t EGL_DMA_BUF_PLANE0_FD_EXT_ = 0x3272;
  static constexpr uint32_t EGL_DMA_BUF_PLANE0_OFFSET_EXT_ = 0x3273;
  static constexpr uint32_t EGL_DMA_BUF_PLANE0_PITCH_EXT_ = 0x3274;
  static constexpr uint32_t EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT_ = 0x3443;
  static constexpr uint32_t EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT_ = 0x3444;
  // Plane 1 (for NV12/P010 UV plane, etc.)
  static constexpr uint32_t EGL_DMA_BUF_PLANE1_FD_EXT_ = 0x3275;
  static constexpr uint32_t EGL_DMA_BUF_PLANE1_OFFSET_EXT_ = 0x3276;
  static constexpr uint32_t EGL_DMA_BUF_PLANE1_PITCH_EXT_ = 0x3277;
  static constexpr uint32_t EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT_ = 0x3445;
  static constexpr uint32_t EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT_ = 0x3446;
  // Plane 2 (for I420 V plane)
  static constexpr uint32_t EGL_DMA_BUF_PLANE2_FD_EXT_ = 0x3278;
  static constexpr uint32_t EGL_DMA_BUF_PLANE2_OFFSET_EXT_ = 0x3279;
  static constexpr uint32_t EGL_DMA_BUF_PLANE2_PITCH_EXT_ = 0x327A;
  static constexpr uint32_t EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT_ = 0x3447;
  static constexpr uint32_t EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT_ = 0x3448;

  // -- EGL function pointer types -----------------------------------
  // EGL handles are all opaque void*. We use void* directly to keep
  // the header header-only.
  using FN_eglGetCurrentDisplay = void* (*)(void);
  using FN_eglGetCurrentContext = void* (*)(void);
  using FN_eglQueryString = const char* (*)(void* dpy, int name);
  using FN_eglCreateImage
      = void* (*)(void* dpy, void* ctx, uint32_t target,
                  void* clientBuffer, const intptr_t* attribs);
  using FN_eglCreateImageKHR
      = void* (*)(void* dpy, void* ctx, uint32_t target,
                  void* clientBuffer, const int32_t* attribs);
  using FN_eglDestroyImage = unsigned int (*)(void* dpy, void* image);
  using FN_eglDestroyImageKHR = unsigned int (*)(void* dpy, void* image);
  // GL extension function — resolved via QOpenGLContext::getProcAddress.
  using FN_glEGLImageTargetTexture2DOES
      = void (*)(unsigned int target, void* image);

  // -- State ---------------------------------------------------------
  // dylib_loader has no default ctor; defer construction to init().
  std::optional<ossia::dylib_loader> m_lib;
  void* m_display{nullptr};
  bool m_useKhrEntry{false}; // true → eglCreateImageKHR/Destroy variants

  FN_eglGetCurrentDisplay m_getCurrentDisplay{};
  FN_eglQueryString m_queryString{};
  FN_eglCreateImage m_createImage{};
  FN_eglCreateImageKHR m_createImageKHR{};
  FN_eglDestroyImage m_destroyImage{};
  FN_eglDestroyImageKHR m_destroyImageKHR{};
  FN_glEGLImageTargetTexture2DOES m_eglImageTargetTexture{};

  /** Returned from importPlane. The GL texture id is the public
   *  output (handed to `QRhiTexture::createFrom`). The EGLImage must
   *  be retained until the GPU is done with the texture. */
  struct PlaneImport
  {
    void* image{nullptr};
  };

  /** Probe: is EGL DMA-BUF zero-copy possible against the current
   *  QRhi instance? Returns false for non-GL backends, GLX-backed GL
   *  contexts, and EGL without the modifier extension. */
  static bool isAvailable(QRhi& rhi) noexcept
  {
    if(rhi.backend() != QRhi::OpenGLES2)
      return false;

    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx)
      return false;

    // Only EGL-backed Qt GL contexts can interop with EGL. GLX
    // contexts can't access EGL images even if libEGL is installed —
    // they're independent stacks. Qt exposes this via the
    // QEGLContext native interface, which returns non-null only when
    // EGL is the actual backend.
    if(!ctx->nativeInterface<QNativeInterface::QEGLContext>())
      return false;

    // Resolve eglGetCurrentDisplay via the GL context's procaddress.
    // If libEGL isn't present at all, this returns null.
    auto getCurDpy = reinterpret_cast<FN_eglGetCurrentDisplay>(
        ctx->getProcAddress("eglGetCurrentDisplay"));
    if(!getCurDpy)
      return false;

    void* dpy = getCurDpy();
    if(!dpy)
      return false;

    auto queryString = reinterpret_cast<FN_eglQueryString>(
        ctx->getProcAddress("eglQueryString"));
    if(!queryString)
      return false;
    const char* exts = queryString(dpy, int(EGL_EXTENSIONS_));
    if(!exts)
      return false;

    // We need the modifier-aware variant. Without it, only LINEAR
    // imports work, and the producer might serve us anything pipewire
    // negotiated (we currently advertise LINEAR + INVALID, but newer
    // producers may emit modifiers anyway).
    if(!std::strstr(exts, "EGL_EXT_image_dma_buf_import_modifiers"))
      return false;

    // GL_OES_EGL_image (the glEGLImageTargetTexture2DOES side) is
    // present on every EGL-backed GL context we've seen, but check
    // anyway — without it the import is useless.
    if(!ctx->hasExtension(QByteArrayLiteral("GL_OES_EGL_image")))
      return false;

    return true;
  }

  /** Initialise: open libEGL via dlopen, resolve entry points, grab
   *  the current EGL display. Returns true on success. */
  bool init(QRhi& rhi) noexcept
  {
    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx)
      return false;

    try
    {
      m_lib.emplace(std::vector<std::string_view>{
          "libEGL.so.1", "libEGL.so"});
    }
    catch(...)
    {
      return false;
    }

    m_getCurrentDisplay
        = m_lib->symbol<FN_eglGetCurrentDisplay>("eglGetCurrentDisplay");
    m_queryString = m_lib->symbol<FN_eglQueryString>("eglQueryString");

    // Modern EGL (1.5+) exposes eglCreateImage / eglDestroyImage as
    // core. Older drivers have only the KHR-suffixed variants. We
    // prefer core; fall back to KHR.
    m_createImage = m_lib->symbol<FN_eglCreateImage>("eglCreateImage");
    m_destroyImage = m_lib->symbol<FN_eglDestroyImage>("eglDestroyImage");
    if(!m_createImage || !m_destroyImage)
    {
      m_createImageKHR
          = m_lib->symbol<FN_eglCreateImageKHR>("eglCreateImageKHR");
      m_destroyImageKHR
          = m_lib->symbol<FN_eglDestroyImageKHR>("eglDestroyImageKHR");
      m_useKhrEntry = m_createImageKHR && m_destroyImageKHR;
      if(!m_useKhrEntry)
        return false;
    }

    // GL-side: glEGLImageTargetTexture2DOES is an extension function;
    // QOpenGLContext::getProcAddress is the portable way to resolve
    // it (delegates to eglGetProcAddress / glXGetProcAddress).
    m_eglImageTargetTexture
        = reinterpret_cast<FN_glEGLImageTargetTexture2DOES>(
            ctx->getProcAddress("glEGLImageTargetTexture2DOES"));
    if(!m_eglImageTargetTexture)
      return false;

    m_display = m_getCurrentDisplay ? m_getCurrentDisplay() : nullptr;
    if(!m_display)
      return false;

    (void)rhi;
    return true;
  }

  /** Destroy the EGLImage held by `p`. The associated GL texture
   *  remains owned by the caller — typically a single persistent
   *  GL texture id that this importer's owner re-targets each frame. */
  void cleanupPlane(PlaneImport& p) noexcept
  {
    if(!p.image)
      return;
    if(m_useKhrEntry)
    {
      if(m_destroyImageKHR && m_display)
        m_destroyImageKHR(m_display, p.image);
    }
    else
    {
      if(m_destroyImage && m_display)
        m_destroyImage(m_display, p.image);
    }
    p.image = nullptr;
  }

  /** Import a single-plane DMA-BUF (packed RGB) as an EGLImage and
   *  bind it to `glTexture` via glEGLImageTargetTexture2DOES.
   *
   *  Returns true on success; on failure `out.image` is left null
   *  and the GL texture is unchanged. */
  bool importPlane(
      PlaneImport& out, unsigned int glTexture, int fd, uint64_t modifier,
      ptrdiff_t offset, ptrdiff_t pitch, uint32_t drm_fourcc, int w, int h)
  {
    out.image = nullptr;
    if(!m_display || !m_eglImageTargetTexture || fd < 0)
      return false;

    // Pass the modifier even when it's LINEAR (0). Drivers that
    // support EGL_EXT_image_dma_buf_import_modifiers expect the
    // modifier attribs to be present; some require them for the
    // import to succeed.
    const uint32_t mod_lo = uint32_t(modifier & 0xFFFFFFFFu);
    const uint32_t mod_hi = uint32_t((modifier >> 32) & 0xFFFFFFFFu);

    // EGLAttrib (intptr_t) on the core entry point; EGLint (int)
    // on the KHR entry point. Build both flavors.
    intptr_t attribs[]
        = {(intptr_t)EGL_WIDTH_,
           w,
           (intptr_t)EGL_HEIGHT_,
           h,
           (intptr_t)EGL_LINUX_DRM_FOURCC_EXT_,
           intptr_t(drm_fourcc),
           (intptr_t)EGL_DMA_BUF_PLANE0_FD_EXT_,
           intptr_t(fd),
           (intptr_t)EGL_DMA_BUF_PLANE0_OFFSET_EXT_,
           intptr_t(offset),
           (intptr_t)EGL_DMA_BUF_PLANE0_PITCH_EXT_,
           intptr_t(pitch),
           (intptr_t)EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT_,
           intptr_t(mod_lo),
           (intptr_t)EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT_,
           intptr_t(mod_hi),
           (intptr_t)EGL_NONE_};

    void* img = nullptr;
    if(m_useKhrEntry)
    {
      // KHR variant takes EGLint (int) attribs.
      int khrAttribs[sizeof(attribs) / sizeof(attribs[0])];
      for(std::size_t i = 0; i < sizeof(attribs) / sizeof(attribs[0]); ++i)
        khrAttribs[i] = int(attribs[i]);
      img = m_createImageKHR(
          m_display, /*EGL_NO_CONTEXT=*/nullptr,
          EGL_LINUX_DMA_BUF_EXT_, /*EGLClientBuffer=*/nullptr,
          khrAttribs);
    }
    else
    {
      img = m_createImage(
          m_display, /*EGL_NO_CONTEXT=*/nullptr,
          EGL_LINUX_DMA_BUF_EXT_, /*EGLClientBuffer=*/nullptr, attribs);
    }
    if(!img)
      return false;

    // Bind to the supplied GL texture. The caller pre-created the
    // texture via glGenTextures + glBindTexture; we just re-target
    // its storage via the new EGLImage. The same texture id can be
    // used across all frames.
    constexpr unsigned int GL_TEXTURE_2D_v = 0x0DE1;
    if(auto* ctx = QOpenGLContext::currentContext())
    {
      auto* funcs = ctx->extraFunctions();
      if(funcs)
      {
        funcs->glBindTexture(GL_TEXTURE_2D_v, glTexture);
        m_eglImageTargetTexture(GL_TEXTURE_2D_v, img);
      }
    }

    out.image = img;
    return true;
  }

  // -- Modifier-aware capability queries -------------------

  /** Enumerate the DMA-BUF modifiers the EGL driver can import for
   *  `drm_fourcc`. Returns at least LINEAR (0) when the extension is
   *  available; empty list when not. Used by format_negotiation to
   *  build the consumer-side EnumFormat modifier choice list.
   *
   *  Requires the importer to have been init()'d (m_display non-null
   *  and ctx active). */
  std::vector<std::uint64_t>
  supportedModifiers(std::uint32_t drm_fourcc) noexcept
  {
    std::vector<std::uint64_t> result;
    if(!m_display)
      return result;

    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx)
      return result;

    using FN_eglQueryDmaBufModifiersEXT = unsigned int (*)(
        void* dpy, int format, int max_modifiers, std::uint64_t* modifiers,
        unsigned int* external_only, int* num_modifiers);
    auto query = reinterpret_cast<FN_eglQueryDmaBufModifiersEXT>(
        ctx->getProcAddress("eglQueryDmaBufModifiersEXT"));
    if(!query)
    {
      // Extension entry not exposed (older driver). LINEAR is the
      // only safe assumption.
      result.push_back(0);
      return result;
    }

    int count = 0;
    if(!query(m_display, int(drm_fourcc), 0, nullptr, nullptr, &count)
       || count <= 0)
    {
      result.push_back(0);
      return result;
    }

    result.resize(static_cast<std::size_t>(count));
    std::vector<unsigned int> external_only(count);
    if(!query(
           m_display, int(drm_fourcc), count, result.data(),
           external_only.data(), &count))
    {
      result.clear();
      result.push_back(0);
      return result;
    }
    result.resize(static_cast<std::size_t>(count));

    // Drop EXTERNAL-only modifiers — score binds these as
    // GL_TEXTURE_2D, which forbids EXTERNAL_OES textures.
    std::size_t w = 0;
    for(std::size_t i = 0; i < result.size(); ++i)
      if(external_only[i] == 0)
        result[w++] = result[i];
    result.resize(w);

    // Always include LINEAR — driver may list non-LINEAR only when
    // it has tiled support, but consumers expect LINEAR as a fallback.
    bool has_linear = false;
    for(auto m : result)
      if(m == 0)
      {
        has_linear = true;
        break;
      }
    if(!has_linear)
      result.push_back(0);
    return result;
  }

  /** Quick yes/no on whether a specific (drm_fourcc, modifier) is
   *  importable. Convenience wrapper around supportedModifiers. */
  bool canImportModifier(
      std::uint32_t drm_fourcc, std::uint64_t modifier) noexcept
  {
    auto mods = supportedModifiers(drm_fourcc);
    for(auto m : mods)
      if(m == modifier)
        return true;
    return modifier == 0; // LINEAR is always safe to claim
  }
};

/** Create a GL_TEXTURE_2D parameterized for EGLImage targeting: LINEAR
 *  min/mag filtering, CLAMP_TO_EDGE wrap. This is the boilerplate every
 *  EGLImage-consuming site needs before glEGLImageTargetTexture2DOES
 *  (DRMPrimeDecoder, EglDmaBufExport, PipewireProducer's EGL path).
 *  Requires a current GL context; returns 0 without one. The constants
 *  are hardcoded so callers don't need GL headers. */
inline unsigned int createLinearClampGlTexture2D() noexcept
{
  auto* ctx = QOpenGLContext::currentContext();
  if(!ctx)
    return 0;
  auto* gl = ctx->extraFunctions();
  unsigned int tex = 0;
  gl->glGenTextures(1, &tex);
  if(!tex)
    return 0;
  constexpr unsigned int GL_TEXTURE_2D_v = 0x0DE1;
  constexpr unsigned int GL_TEXTURE_MIN_FILTER_v = 0x2801;
  constexpr unsigned int GL_TEXTURE_MAG_FILTER_v = 0x2800;
  constexpr unsigned int GL_TEXTURE_WRAP_S_v = 0x2802;
  constexpr unsigned int GL_TEXTURE_WRAP_T_v = 0x2803;
  constexpr unsigned int GL_LINEAR_v = 0x2601;
  constexpr unsigned int GL_CLAMP_TO_EDGE_v = 0x812F;
  gl->glBindTexture(GL_TEXTURE_2D_v, tex);
  gl->glTexParameteri(GL_TEXTURE_2D_v, GL_TEXTURE_MIN_FILTER_v, GL_LINEAR_v);
  gl->glTexParameteri(GL_TEXTURE_2D_v, GL_TEXTURE_MAG_FILTER_v, GL_LINEAR_v);
  gl->glTexParameteri(
      GL_TEXTURE_2D_v, GL_TEXTURE_WRAP_S_v, GL_CLAMP_TO_EDGE_v);
  gl->glTexParameteri(
      GL_TEXTURE_2D_v, GL_TEXTURE_WRAP_T_v, GL_CLAMP_TO_EDGE_v);
  return tex;
}

} // namespace score::gfx
#endif // __linux__
