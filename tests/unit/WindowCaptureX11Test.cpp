// What the X11 window capture hands to the renderer when a capture fails
// right after the captured window changed size: the renderer copies
// stride * height bytes from the frame's data, so that memory has to exist.
//
// The test brings up its own Xvfb without the Composite extension, so that
// the backend reads the window itself. A window that reaches past the edge
// of the screen cannot be read that way (the server answers BadMatch), which
// makes the capture that follows a resize fail on demand.

#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#include <catch2/catch_test_macros.hpp>

#include <X11/Xlib.h>
#undef None

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace Gfx::WindowCapture
{
std::unique_ptr<WindowCaptureBackend> createX11Backend();
}

using namespace Gfx::WindowCapture;

namespace
{
struct PrivateXvfb
{
  pid_t pid{-1};
  std::string display;

  PrivateXvfb()
  {
    int fds[2];
    if(pipe(fds) != 0)
      return;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);

    const std::string fd = std::to_string(fds[1]);
    const char* argv[] = {"Xvfb",        "-displayfd", fd.c_str(), "-screen",
                          "0",           "640x480x24", "-extension", "Composite",
                          "-nolisten",   "tcp",        nullptr};
    if(posix_spawnp(
           &pid, "Xvfb", &actions, nullptr, const_cast<char**>(argv), environ)
       != 0)
      pid = -1;
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);

    if(pid > 0)
    {
      char buf[32]{};
      std::size_t n = 0;
      pollfd p{fds[0], POLLIN, 0};
      while(n < sizeof(buf) - 1 && poll(&p, 1, 10000) > 0)
      {
        auto r = read(fds[0], buf + n, sizeof(buf) - 1 - n);
        if(r <= 0)
          break;
        n += r;
        if(std::memchr(buf, '\n', n))
          break;
      }
      for(std::size_t i = 0; i < n && buf[i] >= '0' && buf[i] <= '9'; i++)
        display += buf[i];
      if(!display.empty())
        display = ":" + display;
    }
    close(fds[0]);
  }

  ~PrivateXvfb()
  {
    if(pid > 0)
    {
      kill(pid, SIGTERM);
      waitpid(pid, nullptr, 0);
    }
  }
};

// The application's X error handler: Qt's xcb platform installs one that
// ignores errors, so a failed request only makes its Xlib call return false.
int ignoreXErrors(Display*, XErrorEvent*)
{
  return 0;
}

bool isMapped(const void* data, std::size_t size)
{
  const auto page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
  const auto begin = reinterpret_cast<uintptr_t>(data) & ~(page - 1);
  const auto end = reinterpret_cast<uintptr_t>(data) + size;
  std::vector<unsigned char> residency((end - begin + page - 1) / page);
  return mincore(reinterpret_cast<void*>(begin), end - begin, residency.data()) == 0;
}
}

TEST_CASE(
    "a capture that fails after a window resize hands out no freed pixels",
    "[unit][windowcapture]")
{
  PrivateXvfb xvfb;
  if(xvfb.display.empty())
    SKIP("Xvfb is not available");
  setenv("DISPLAY", xvfb.display.c_str(), 1);
  XSetErrorHandler(ignoreXErrors);

  Display* dpy = XOpenDisplay(xvfb.display.c_str());
  REQUIRE(dpy);
  const Window root = DefaultRootWindow(dpy);
  const Window win = XCreateSimpleWindow(dpy, root, 0, 0, 300, 120, 0, 0, 0x336699);
  XStoreName(dpy, win, "capture-me");
  XMapWindow(dpy, win);
  XSync(dpy, false);

  auto backend = createX11Backend();
  REQUIRE(backend);
  CaptureTarget target;
  target.mode = CaptureMode::Window;
  target.windowId = win;
  REQUIRE(backend->start(target));

  const auto before = backend->grab();
  REQUIRE(before.type == CapturedFrame::CPU_BGRA);
  REQUIRE(before.width == 300);
  REQUIRE(before.height == 120);

  // Smaller, and past the right edge of the 640 pixel wide screen.
  XMoveResizeWindow(dpy, win, 500, 0, 200, 100);
  XSync(dpy, false);

  for(int i = 0; i < 3; i++)
  {
    const auto after = backend->grab();
    if(after.type == CapturedFrame::None)
      continue;
    REQUIRE(after.data);
    REQUIRE(isMapped(after.data, std::size_t(after.stride) * after.height));
    // What the renderer does with it.
    std::vector<uint8_t> copy(std::size_t(after.stride) * after.height);
    std::memcpy(copy.data(), after.data, copy.size());
  }

  // Back on screen: the capture resumes at the new size.
  XMoveWindow(dpy, win, 0, 0);
  XSync(dpy, false);
  const auto resumed = backend->grab();
  REQUIRE(resumed.type == CapturedFrame::CPU_BGRA);
  CHECK(resumed.width == 200);
  CHECK(resumed.height == 100);
  REQUIRE(isMapped(resumed.data, std::size_t(resumed.stride) * resumed.height));

  backend->stop();
  CHECK(backend->grab().type == CapturedFrame::None);

  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
}
