#pragma once

// Local streaming peers for the network tests, and the hard deadline every
// lifecycle case runs under.
//
// The harness owns the server processes rather than the runner script, because
// half of what has to be tested is what happens when the peer GOES AWAY: a
// mid-stream drop, a reconnect, a destroy while the input is blocked on a read.
// Only the process that can kill the peer can stage those.
//
// POSIX only: fork/execvp/kill and a raw socket to pick a free port. There is no
// Windows equivalent cheap enough to be worth a second implementation, and the
// rigs these run on are Linux.

#include <QTcpServer>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace score::test::stream
{

#if !defined(_WIN32)

// A child process in its own session, killed as a group so that a peer which
// forked helpers of its own cannot outlive the case that started it.
class Process
{
public:
  Process() = default;
  explicit Process(const std::vector<std::string>& argv) { start(argv); }

  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  Process(Process&& o) noexcept
      : m_pid{o.m_pid}
  {
    o.m_pid = -1;
  }
  Process& operator=(Process&& o) noexcept
  {
    if(this != &o)
    {
      terminate();
      m_pid = o.m_pid;
      o.m_pid = -1;
    }
    return *this;
  }

  ~Process() { terminate(); }

  void start(const std::vector<std::string>& argv)
  {
    terminate();
    REQUIRE_FALSE(argv.empty());

    std::vector<char*> raw;
    raw.reserve(argv.size() + 1);
    for(const auto& a : argv)
      raw.push_back(const_cast<char*>(a.c_str()));
    raw.push_back(nullptr);

    const pid_t pid = ::fork();
    REQUIRE(pid >= 0);
    if(pid == 0)
    {
      ::setsid();
      const int devnull = ::open("/dev/null", O_RDWR);
      if(devnull >= 0)
      {
        ::dup2(devnull, STDIN_FILENO);
        ::dup2(devnull, STDOUT_FILENO);
        ::dup2(devnull, STDERR_FILENO);
      }
      ::execvp(raw[0], raw.data());
      ::_exit(127);
    }
    m_pid = pid;
  }

  bool running() const noexcept
  {
    if(m_pid <= 0)
      return false;
    int status{};
    return ::waitpid(m_pid, &status, WNOHANG) == 0;
  }

  void terminate() noexcept
  {
    if(m_pid <= 0)
      return;
    ::kill(-m_pid, SIGKILL);
    ::kill(m_pid, SIGKILL);
    int status{};
    ::waitpid(m_pid, &status, 0);
    m_pid = -1;
  }

  pid_t pid() const noexcept { return m_pid; }

private:
  pid_t m_pid{-1};
};

// A UDP port nothing else holds. Bound, read back, released: the window before
// the peer claims it is small enough for a local harness and there is no
// portable way to hand an already-bound socket to ffmpeg.
inline int freeUdpPort()
{
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  REQUIRE(fd >= 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  REQUIRE(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
  socklen_t len = sizeof(addr);
  REQUIRE(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
  const int port = ::ntohs(addr.sin_port);
  ::close(fd);
  REQUIRE(port > 0);
  return port;
}

inline int freeTcpPort()
{
  QTcpServer s;
  REQUIRE(s.listen(QHostAddress::LocalHost, 0));
  const int port = s.serverPort();
  s.close();
  REQUIRE(port > 0);
  return port;
}

enum class ChildOutcome
{
  Ok,
  Failed,
  Crashed
};

inline const char* toString(ChildOutcome o)
{
  switch(o)
  {
    case ChildOutcome::Ok:
      return "ok";
    case ChildOutcome::Failed:
      return "failed";
    case ChildOutcome::Crashed:
      return "crashed";
  }
  return "?";
}

//! Runs fn in a forked child, so that a crash inside a third-party library is
//! attributed to the case that provoked it instead of taking the suite down.
template <typename F>
ChildOutcome runInChild(F&& fn)
{
  ::fflush(nullptr);
  const pid_t pid = ::fork();
  REQUIRE(pid >= 0);
  if(pid == 0)
  {
    // Catch2's crash handler would print a second, confusing report from the
    // child; here the exit status IS the result.
    for(int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL})
      ::signal(sig, SIG_DFL);
    const bool ok = fn();
    ::_exit(ok ? 0 : 1);
  }
  int status{};
  REQUIRE(::waitpid(pid, &status, 0) == pid);
  if(WIFSIGNALED(status))
    return ChildOutcome::Crashed;
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ChildOutcome::Ok
                                                         : ChildOutcome::Failed;
}

// Accepts connections and then never sends anything, like a peer that is up but
// wedged. The connection succeeds, so nothing short of a deadline gets the
// caller back.
struct SilentTcpServer
{
  SilentTcpServer() { REQUIRE(server.listen(QHostAddress::LocalHost, 0)); }

  int port() const { return server.serverPort(); }

  QTcpServer server;
};

#endif

// Everything a deadline-guarded call touches, kept in one heap block so that a
// case which never returns can be abandoned instead of joined.
template <typename State>
struct Run
{
  State state;
  std::atomic_bool done{};
  bool ok{};
};

//! Runs f(state) on a thread and reports whether it returned within the delay.
//!
//! A blocked libav call cannot be cancelled: when it does not return, the thread
//! is detached and the state it keeps writing to is leaked ON PURPOSE, so that a
//! hang is reported as a failed deadline rather than turning into a
//! use-after-free or wedging the whole suite. `state` is null afterwards.
template <typename State, typename F>
bool finishesWithin(
    std::chrono::milliseconds delay, std::unique_ptr<Run<State>>& state, F&& f)
{
  auto* st = state.get();
  REQUIRE(st != nullptr);

  std::thread th{[st, f] {
    st->ok = f(st->state);
    st->done.store(true, std::memory_order_release);
  }};

  const auto deadline = std::chrono::steady_clock::now() + delay;
  while(!st->done.load(std::memory_order_acquire)
        && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  if(!st->done.load(std::memory_order_acquire))
  {
    th.detach();
    (void)state.release();
    return false;
  }

  th.join();
  return true;
}

}
