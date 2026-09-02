// Unit tests for Gfx/Graph/interop/V4L2Loader.hpp — the shared dlopen'd
// libv4l2 entry points and the ioctl wrapper every V4L2 caller goes through.
//
// What is assertable without a capture device is the loader's contract:
//
//  * available() is all-or-nothing: a loader that reports available must hold
//    all three entry points, and the fallback path (raw syscalls) must work
//    either way — that is the whole point of "degrade a feature rather than
//    fail to load the plugin".
//  * openDevice/closeDevice really open and really close, whichever rung was
//    chosen (libv4l2 falls back to plain open/close for a non-V4L2 file).
//  * retryIoctl passes a working ioctl through (FIONREAD on a pipe — a real
//    ioctl, no video device needed) and returns a genuine error unchanged
//    rather than retrying it: only EINTR is retried, so a persistent EBADF
//    must come back promptly as -1/EBADF. A retryIoctl that looped on any -1
//    would hang this test, and one that swallowed errno would fail it.
//
// Linux-only, like the header.

#if defined(__linux__)

#include <Gfx/Graph/interop/V4L2Loader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

using namespace score::gfx::v4l2;

TEST_CASE("the loader is a singleton and available() is all-or-nothing")
{
  const auto& a = Libv4l2::instance();
  const auto& b = Libv4l2::instance();
  CHECK(&a == &b);

  // available() means every entry point resolved; a partial resolve (a
  // mismatched or stripped libv4l2) must degrade, not half-work.
  if(a.available())
  {
    CHECK(a.open != nullptr);
    CHECK(a.close != nullptr);
    CHECK(a.ioctl != nullptr);
  }
  else
  {
    // The raw-syscall fallback carries the feature; nothing to resolve.
    SUCCEED("libv4l2 not present; fallback rung in use");
  }
}

TEST_CASE("openDevice reports a missing device the same on either rung")
{
  // The one open() outcome the two rungs agree on without a camera: a path
  // that does not exist is ENOENT through libv4l2 and through the raw
  // syscall alike, and the loader must pass that through rather than eat it.
  // (A path that EXISTS but is not a V4L2 device is rung-dependent —
  // libv4l2's v4l2_open refuses it with ENOTTY where raw open() succeeds —
  // so that case is deliberately not asserted here.)
  errno = 0;
  const int fd = openDevice("/nonexistent/score-v4l2-loader-test", O_RDWR);
  CHECK(fd == -1);
  CHECK(errno == ENOENT);
}

TEST_CASE("closeDevice really closes, whatever opened the fd")
{
  // A capture teardown path can hand closeDevice an fd that was opened before
  // the loader was consulted (raw open); libv4l2's v4l2_close falls back to
  // close() for an fd it does not manage. Either rung, the fd must be gone.
  const int fd = ::open("/dev/null", O_RDONLY);
  REQUIRE(fd >= 0);

  closeDevice(fd);

  errno = 0;
  CHECK(::fcntl(fd, F_GETFD) == -1);
  CHECK(errno == EBADF);
}

TEST_CASE("retryIoctl passes a working ioctl through")
{
  int fds[2];
  REQUIRE(::pipe(fds) == 0);

  const char payload[3] = {'a', 'b', 'c'};
  REQUIRE(::write(fds[1], payload, sizeof(payload)) == 3);

  // FIONREAD: how many bytes wait in the pipe. A real ioctl, no device needed.
  int queued = 0;
  const int r = retryIoctl(fds[0], FIONREAD, &queued);
  CHECK(r == 0);
  CHECK(queued == 3);

  ::close(fds[0]);
  ::close(fds[1]);
}

TEST_CASE("retryIoctl retries only EINTR, not every error")
{
  // A persistently-bad fd must come straight back: the retry loop is for
  // EINTR alone. If the loop retried any -1 this would never return, and the
  // ctest timeout — not an assertion — would be the only thing catching it.
  errno = 0;
  int arg = 0;
  const int r = retryIoctl(-1, FIONREAD, &arg);
  CHECK(r == -1);
  CHECK(errno == EBADF);
}

#else

#include <catch2/catch_test_macros.hpp>

TEST_CASE("the v4l2 loader is a Linux path")
{
  SUCCEED();
}

#endif
