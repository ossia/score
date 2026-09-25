// =============================================================================
// Agent M1: booting the GUI application, rendering a gfx pipeline on it and
// tearing both down leaves the process with as many file descriptors as it
// had before. macOS starts a process with 256 descriptors, and a test binary
// that renders a few dozen times used to run out of them.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <sstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/param.h>
#endif
#define M1_HAS_FD_PROBE 1
#endif

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
#if defined(M1_HAS_FD_PROBE)
std::string describe_fd(int fd)
{
  struct stat st{};
  if(fstat(fd, &st) != 0)
    return "?";
  std::string kind;
  switch(st.st_mode & S_IFMT)
  {
    case S_IFIFO:
      kind = "fifo";
      break;
    case S_IFSOCK:
      kind = "socket";
      break;
    case S_IFCHR:
      kind = "chr";
      break;
    case S_IFREG:
      kind = "file";
      break;
    case S_IFDIR:
      kind = "dir";
      break;
    default:
      kind = "other";
      break;
  }
#if defined(__APPLE__)
  char path[MAXPATHLEN]{};
  if(fcntl(fd, F_GETPATH, path) == 0)
    kind += std::string{" "} + path;
#else
  char path[4096]{};
  const std::string link = "/proc/self/fd/" + std::to_string(fd);
  if(const auto n = readlink(link.c_str(), path, sizeof(path) - 1); n > 0)
    kind += std::string{" "} + std::string(path, std::size_t(n));
#endif
  return kind;
}

std::map<int, std::string> open_fds()
{
  std::map<int, std::string> fds;
  const int max = int(sysconf(_SC_OPEN_MAX));
  for(int fd = 0; fd < std::min(max, 65536); ++fd)
    if(fcntl(fd, F_GETFD) != -1)
      fds[fd] = describe_fd(fd);
  return fds;
}

std::string diff(const std::map<int, std::string>& before, const std::map<int, std::string>& after)
{
  std::ostringstream s;
  for(const auto& [fd, d] : after)
    if(!before.contains(fd))
      s << "  +" << fd << " " << d << "\n";
  for(const auto& [fd, d] : before)
    if(!after.contains(fd))
      s << "  -" << fd << " " << d << "\n";
  return s.str();
}

struct RenderOutcome
{
  bool skipped = false;
  std::string skip_reason, error;
};

RenderOutcome render_once(score::gfx::GraphicsApi be, const char* shader)
{
  RenderOutcome r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(!shader)
      return;
    GfxPipeline p;
    const QString path = corpus(shader);
    const int src = path.endsWith(".cs") ? p.addCsf(path) : p.addIsf(path);
    const int sink = p.addSink({32, 32});
    if(src < 0)
    {
      r.error = p.error();
      return;
    }
    p.wire(p.imageOut(src, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    p.render(2);
    if(!p.readback(sink).valid())
      r.error = "empty readback";
  });
  return r;
}

void check_stable(score::gfx::GraphicsApi be, const char* shader)
{
  constexpr int warmup = 2;
  constexpr int runs = 6;
  for(int i = 0; i < warmup; ++i)
    render_once(be, shader);
  const auto before = open_fds();
  RenderOutcome last;
  for(int i = 0; i < runs; ++i)
    last = render_once(be, shader);
  const auto after = open_fds();
  if(last.skipped)
    SKIP(std::string{backend_name(be)} + ": " + last.skip_reason);
  INFO("descriptors opened and not closed over " << runs << " runs:\n" << diff(before, after));
  CHECK(last.error.empty());
  CHECK(after.size() <= before.size());
}
#endif
}

TEST_CASE("M1: booting the GUI application does not leak descriptors", "[gfx][fd][m1]")
{
#if defined(M1_HAS_FD_PROBE)
  check_stable(score::gfx::GraphicsApi::Null, nullptr);
#else
  SKIP("no descriptor probe on this platform");
#endif
}

TEST_CASE("M1: an ISF render does not leak descriptors", "[gfx][fd][m1]")
{
#if defined(M1_HAS_FD_PROBE)
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  check_stable(backend, "isf-solid-color.fs");
#else
  SKIP("no descriptor probe on this platform");
#endif
}

TEST_CASE("M1: a CSF render does not leak descriptors", "[gfx][fd][m1]")
{
#if defined(M1_HAS_FD_PROBE)
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  check_stable(backend, "fixc-image-alpha.cs");
#else
  SKIP("no descriptor probe on this platform");
#endif
}
