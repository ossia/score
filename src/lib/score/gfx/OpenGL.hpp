#pragma once

#include <QString>
#include <QStringList>
#include <QSurfaceFormat>
#if __has_include(<private/qshader_p.h>)
#include <private/qshader_p.h>
#endif

#include <score_lib_base_export.h>

namespace score
{
//! Whether the NVIDIA GLX vendor library can be asked to provide GL.
enum class NvidiaGlxVendorState
{
  //! No NVIDIA kernel module.
  NoDriver,
  //! Kernel module loaded, no GLX vendor library.
  NotInstalled,
  //! Different releases: every GLX context creation fails with BadValue. What a
  //! driver upgrade leaves behind until reboot.
  VersionMismatch,
  //! Both halves agree.
  Consistent,
  //! Versions unreadable. Treated as usable, so an unusual library layout does
  //! not cost a working machine its GPU.
  Unknown
};

//! Driver release in /proc/driver/nvidia/version, e.g. "610.57.04"; empty if
//! absent. Takes the first dotted number on the NVRM line, since the field
//! position differs between the proprietary and open modules.
SCORE_LIB_BASE_EXPORT QString
nvidiaKernelDriverVersion(const QString& procVersionText) noexcept;

//! Driver release from a GLX vendor library name, e.g.
//! "libGLX_nvidia.so.615.71.09" -> "615.71.09". Empty for a bare ".so.0".
SCORE_LIB_BASE_EXPORT QString
nvidiaGlxLibraryVersion(const QString& libraryPath) noexcept;

//! Pure function of the two versions, so it is testable without an NVIDIA card.
//! Empty @p kernelVersion means no driver.
SCORE_LIB_BASE_EXPORT NvidiaGlxVendorState nvidiaGlxVendorState(
    const QString& kernelVersion, const QString& libraryVersion) noexcept;

//! The same decision against the running system: reads
//! /proc/driver/nvidia/version and resolves the libGLX_nvidia the dynamic
//! loader would use. Does not open a display and makes no GL or GLX call, so it
//! is safe before QGuiApplication exists -- a real GLX probe is not, since an
//! Xlib protocol error there aborts the process through the default handler.
SCORE_LIB_BASE_EXPORT NvidiaGlxVendorState nvidiaGlxVendorState() noexcept;


//! Install the default QSurfaceFormat score's RHI needs, where that can be
//! decided without probing (Apple, embedded GLES). No-op elsewhere: desktop GL
//! is probed in the application.
//!
//! MUST run before the first QOpenGLContext or QWindow exists -- it is the
//! DEFAULT format that decides which profile Apple hands out, and
//! score::gfx::Window builds its own format from QSurfaceFormat::defaultFormat().
//!
//! Lives here rather than in main.cpp because the TESTS need it too: the app
//! sets its profile in setup_opengl(), which no test bootstrap reaches, so
//! without this every test touching the OpenGL backend on macOS runs on
//! Apple's legacy profile -- GL 2.1 / GLSL 1.20 -- instead of the 4.1 core the
//! app itself uses. On that context QRhi has no texture arrays and no 3D
//! textures, so RenderList creation throws and no window ever presents a frame.
SCORE_LIB_BASE_EXPORT void setupDefaultOpenGLFormat() noexcept;

struct SCORE_LIB_BASE_EXPORT GLCapabilities
{
public:
  GLCapabilities();

  int major{};
  int minor{};
  int shaderVersion{};
  QSurfaceFormat::RenderableType type{};

#if __has_include(<private/qshader_p.h>)
  QShaderVersion qShaderVersion;
#endif

  void setupFormat(QSurfaceFormat& fmt);
};
}
