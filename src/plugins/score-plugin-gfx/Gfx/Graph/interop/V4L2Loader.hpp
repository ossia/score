#pragma once

/**
 * @file V4L2Loader.hpp
 * @brief dlopen'd libv4l2 entry points, shared by every V4L2 consumer.
 *
 * Same shape as DrmFunctions / CudaFunctions: no link-time dependency, and a
 * machine without libv4l2 degrades a feature rather than failing to load the
 * plugin. This lived as a private class inside CameraDevice.v4l2.cpp; the
 * control tree needs the same three symbols, and a second copy of a dlopen
 * singleton is how the two drift apart.
 *
 * `available()` rather than an assert: control ioctls need none of libv4l2's
 * format emulation, so a caller that only reads and writes controls can fall
 * back to the raw syscall and still work. Capture, which does rely on the
 * emulation, checks and declines instead.
 */

#include <ossia/detail/dylib_loader.hpp>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>

// libv4l2.h is not always installed even where the library is, so the three
// entry points we use are declared here rather than included.
extern "C" {
int v4l2_open(const char* file, int oflag, ...);
int v4l2_close(int fd);
int v4l2_dup(int fd);
int v4l2_ioctl(int fd, unsigned long int request, ...);
ssize_t v4l2_read(int fd, void* buffer, size_t n);
ssize_t v4l2_write(int fd, const void* buffer, size_t n);
void* v4l2_mmap(void* start, size_t length, int prot, int flags, int fd, int64_t offset);
int v4l2_munmap(void* _start, size_t length);
}

namespace score::gfx::v4l2
{

class Libv4l2
{
public:
  decltype(&::v4l2_open) open{};
  decltype(&::v4l2_close) close{};
  decltype(&::v4l2_ioctl) ioctl{};

  bool available() const noexcept { return open && close && ioctl; }

  static const Libv4l2& instance()
  {
    static const Libv4l2 self;
    return self;
  }

private:
  Libv4l2()
      : library("libv4l2.so.0")
  {
    open = library.symbol<decltype(&::v4l2_open)>("v4l2_open");
    close = library.symbol<decltype(&::v4l2_close)>("v4l2_close");
    ioctl = library.symbol<decltype(&::v4l2_ioctl)>("v4l2_ioctl");
  }

  ossia::dylib_loader library;
};

/// ioctl with the EINTR retry every V4L2 caller needs, through libv4l2 when it
/// is there. Shared rather than re-declared per file: this addon is built as a
/// unity build, so two files each carrying their own `xioctl` in an anonymous
/// namespace end up in one translation unit and collide.
inline int retryIoctl(int fd, unsigned long request, void* arg) noexcept
{
  const auto& lib = Libv4l2::instance();
  int r;
  do
  {
    r = lib.available() ? lib.ioctl(fd, request, arg) : ::ioctl(fd, request, arg);
  } while(r == -1 && errno == EINTR);
  return r;
}

/// The same three, bypassing libv4l2 outright.
///
/// libv4l2 dlopens every plugin in /usr/lib/libv4l/plugins and offers each one
/// the fd. On Tegra that includes libv4l2_nvargus.so, the Argus-backed V4L2
/// shim: with nvargus-daemon stopped -- which is exactly the state the raw
/// Bayer path needs, since Argus otherwise sets bypass_mode=1 and V4L2 delivers
/// nothing -- its open() fails, it stays attached to the fd anyway, and
/// v4l2_close() then segfaults inside libnvargus_socketclient dereferencing
/// state it never initialised. Closing the device or quitting score both take
/// that path, so both crash.
///
/// The direct-video backend wants none of what libv4l2 offers regardless: its
/// job is format emulation, and this path deliberately takes the sensor's raw
/// Bayer and demosaics on the GPU. CameraDevice keeps the wrappers, where
/// converting an odd webcam format is the whole point.
inline int openDeviceRaw(const char* path, int flags) noexcept
{
  return ::open(path, flags);
}

inline void closeDeviceRaw(int fd) noexcept
{
  ::close(fd);
}

inline int retryIoctlRaw(int fd, unsigned long request, void* arg) noexcept
{
  int r;
  do
  {
    r = ::ioctl(fd, request, arg);
  } while(r == -1 && errno == EINTR);
  return r;
}

inline int openDevice(const char* path, int flags) noexcept
{
  const auto& lib = Libv4l2::instance();
  return lib.available() ? lib.open(path, flags) : ::open(path, flags);
}

inline void closeDevice(int fd) noexcept
{
  const auto& lib = Libv4l2::instance();
  if(lib.available())
    lib.close(fd);
  else
    ::close(fd);
}

} // namespace score::gfx::v4l2
