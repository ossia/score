#pragma once

/**
 * @file GLCaptureUpload.hpp
 * @brief OpenGL-specific upload helpers for GPU-direct video CAPTURE
 *        strategies that land a captured frame in a QRhi GL texture.
 *
 * Extracted from the per-strategy headers in the AJA addon
 * (CaptureInteropGLCpu, CaptureInteropGLTier3) which had byte-for-byte
 * identical copies of the `glTexSubImage2D` upload skeleton. The
 * backend-neutral plumbing (slot handoff, byte-size validation) lives in
 * CaptureStrategyCommon.hpp, which this file includes; the vendor-specific
 * concerns (DMABufferLock, buffer allocation, P2P/DVP bridge calls, pacing)
 * stay in the addon.
 *
 * The two upload helpers intentionally keep distinct behaviour matching the
 * paths they replace:
 *   - uploadClientToGLTexture (sysmem staging) resets pixel-unpack state, since
 *     the decoder/QRhi may have left a stale ROW_LENGTH/SKIP that would shear
 *     a client-pointer upload.
 *   - uploadGLBufferToGLTexture (P2P: data already in a GL buffer) binds the
 *     source buffer and uploads from offset 0.
 */

#include <Gfx/Graph/interop/CaptureStrategyCommon.hpp>

#include <QtGui/private/qrhigles2_p.h>

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <cstdint>

namespace score::gfx::interop
{

/**
 * @brief Upload one captured frame from client memory into a QRhi GL texture.
 *
 * For the CPU-staging path: the vendor DMA'd the frame into a page-locked host
 * buffer; this copies it sysmem -> VRAM. Resets pixel-unpack state first
 * (PIXEL_UNPACK_BUFFER unbound + ALIGNMENT/ROW_LENGTH/SKIP defaults) so a stale
 * setting left by the decoder doesn't misalign rows.
 *
 * @p bgra selects GL_BGRA vs GL_RGBA (ARGB framestores are BGRA in memory).
 */
inline void uploadClientToGLTexture(
    QOpenGLContext& glctx, QRhiTexture& outTex, const void* src,
    bool bgra) noexcept
{
  if(!src)
    return;
  const auto nt = outTex.nativeTexture();
  if(!nt.object)
    return;
  auto* f = glctx.extraFunctions();
  if(!f)
    return;

  const auto sz = outTex.pixelSize();
  const GLenum fmt = bgra ? GL_BGRA : GL_RGBA;
  f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  f->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  f->glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  f->glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  f->glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(nt.object));
  f->glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0, sz.width(), sz.height(), fmt, GL_UNSIGNED_BYTE,
      src);
  f->glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * @brief Upload one captured frame from a GL buffer (bound as
 *        PIXEL_UNPACK_BUFFER) into a QRhi GL texture.
 *
 * For the tier-3 P2P path: the vendor P2P-wrote straight into the GL buffer's
 * VRAM, so the "upload" is a server-side buffer -> texture copy from offset 0.
 *
 * @p srcGlBuffer the GL buffer object name holding the captured frame.
 * @p bgra selects GL_BGRA vs GL_RGBA.
 */
inline void uploadGLBufferToGLTexture(
    QOpenGLContext& glctx, QRhiTexture& outTex, std::uint32_t srcGlBuffer,
    bool bgra) noexcept
{
  if(!srcGlBuffer)
    return;
  const auto nt = outTex.nativeTexture();
  if(!nt.object)
    return;
  auto* f = glctx.extraFunctions();
  if(!f)
    return;

  const auto sz = outTex.pixelSize();
  const GLenum fmt = bgra ? GL_BGRA : GL_RGBA;
  // The storage buffer can be bound to GL_PIXEL_UNPACK_BUFFER even though it
  // was allocated for GL_SHADER_STORAGE_BUFFER: both are server-side memory;
  // the binding target picks the usage, not the storage type.
  f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, srcGlBuffer);
  f->glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(nt.object));
  f->glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0, sz.width(), sz.height(), fmt, GL_UNSIGNED_BYTE,
      nullptr);
  f->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  f->glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace score::gfx::interop
