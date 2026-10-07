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
  //! No NVIDIA kernel module: there is nothing to offload to.
  NoDriver,
  //! The kernel module is loaded but no NVIDIA GLX vendor library is present.
  NotInstalled,
  //! Kernel module and user-space library are different driver releases. Every
  //! GLX context creation then fails -- X_GLXCreateNewContext comes back
  //! BadValue -- which is the state a driver upgrade leaves behind until the
  //! machine is rebooted.
  VersionMismatch,
  //! Both halves report the same release.
  Consistent,
  //! Present, but their versions could not be read. Treated as usable: the
  //! probe must not take a working machine's GPU away because a distribution
  //! names its libraries unusually.
  Unknown
};

//! Driver release in the contents of /proc/driver/nvidia/version, e.g.
//! "610.57.04". Empty when there is none to find.
//!
//! The line differs between the proprietary and open modules --
//!   "NVRM version: NVIDIA UNIX x86_64 Kernel Module  550.54.14  Thu Feb 22 ..."
//!   "NVRM version: NVIDIA UNIX Open Kernel Module for x86_64  610.57.04  ..."
//! -- so this takes the first dotted number on the NVRM line rather than a
//! fixed field position.
SCORE_LIB_BASE_EXPORT QString
nvidiaKernelDriverVersion(const QString& procVersionText) noexcept;

//! Driver release in the name of a GLX vendor library, e.g.
//! "/usr/lib/libGLX_nvidia.so.615.71.09" -> "615.71.09". Empty when the path
//! carries no version (a bare ".so.0" symlink that could not be resolved).
SCORE_LIB_BASE_EXPORT QString
nvidiaGlxLibraryVersion(const QString& libraryPath) noexcept;

//! The decision, as a pure function of the two version strings, so it can be
//! exercised without an NVIDIA card. Empty `kernelVersion` means no driver.
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
