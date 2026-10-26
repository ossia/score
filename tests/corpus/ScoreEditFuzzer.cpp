// Adversarial editing of a score *while it plays*: ONE .score document per
// process, driven over a directory by run-edit-fuzz.sh, the same contract as
// ScoreCorpusTester.cpp (whose bootstrap this file reuses).
//
// Usage:
//   score_edit_fuzzer <file.score> [--seed N] [--ops N] [--warmup-ms N]
//                     [--timeout S]
//
// After loading the document and starting playback, a seeded random sequence
// of edits runs, each one as its own event-loop turn (a user can't be faster
// than that), with 0-30 ms between them:
//  - remove a process            (ObjectEditorList::remove, as the Delete key)
//  - remove a process, keep link (same, with Shift held: the "translate the
//                                 connections across the removed node" path,
//                                 ScenarioEditor.cpp RemoveProcessAndKeepLinked)
//  - remove a cable              (ObjectEditorList::remove on a cable)
//  - remove scenario elements    (interval / state / event / sync / comment,
//                                 with the parent scenario focused)
//  - create a cable              (Dataflow::onCreateCable, the drag path)
//  - undo / redo bursts          (CommandStack::undo/redo, up to 64 at 0 ms)
//  - undo/redo ping-pong storms
//  - transport: stop+play, pause/resume, play-from-here
// Devices and addresses are never removed: that is unsupported while playing.
//
// Oracles, beyond the crash / assert / hang that run-edit-fuzz.sh turns into
// CRASH_SIG<n> / TIMEOUT verdicts:
//  - undo everything -> the document structure (processes, cables and their
//    endpoints, scenario elements, port addresses) must equal the loaded one:
//    UNDO_MISMATCH otherwise;
//  - redo everything -> must equal the structure before the undo-all:
//    REDO_MISMATCH otherwise;
//  - execution must still be running at the end, and must restart after a
//    stop: NOT_PLAYING otherwise;
//  - closing the document must not crash (stage "close").
// Every edit is logged to stderr as "STAGE <n> <op> <target>" before it runs,
// so the last STAGE line of a crash log names the edit that crashed.
//
// Verdicts: OK / LOADFAIL / UNKNOWN_UUID:<uuid> / UNDO_MISMATCH /
// REDO_MISMATCH / NOT_PLAYING, plus CRASH_SIG<n> / TIMEOUT from the driver.

// First: score's Process/ExecutionContext.hpp defines an `in_exec` macro that
// collides with a QCoreApplicationPrivate member.
#include <QtGui/private/qguiapplication_p.h>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/context.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/flat_set.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPointer>
#include <QTimer>

#include <QSettings>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <clocale>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Crash and hang reports. The C++ standard has signals (<csignal>) but no
// stack walker (std::stacktrace is C++23, and libc++ does not ship it), no
// thread enumeration and no siginfo: those parts are per-platform below, and a
// platform without them still runs the fuzzer, with shorter reports.
#if !defined(_WIN32)
#define SCORE_FUZZ_POSIX_SIGNALS 1
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#if __has_include(<execinfo.h>) && __has_include(<dlfcn.h>)
// glibc and macOS; not musl, not Windows.
#define SCORE_FUZZ_BACKTRACE 1
#include <dlfcn.h>
#include <execinfo.h>
#endif
#if defined(__linux__)
#include <dirent.h>
#if defined(__x86_64__)
#define SCORE_FUZZ_STACK_SCAN 1
#include <ucontext.h>
#endif
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace
{
// ---------------------------------------------------------------------------
// Logging: every warning is counted and the distinct ones kept for the
// verdict, so "soft" failures (a warning per edit) show up in the report too.
// ---------------------------------------------------------------------------
namespace msgs
{
inline ossia::flat_map<std::string, int> distinct;
inline int warnings = 0;
inline int criticals = 0;
inline int vk_validation = 0; // VK_LAYER_KHRONOS_validation errors
inline int gl_errors = 0;     // KHR_debug HighSeverity ErrorType
inline std::string first_gpu_error;
inline QtMessageHandler chained = nullptr;

inline void handler(QtMsgType t, const QMessageLogContext& c, const QString& m)
{
  if(t == QtWarningMsg)
    ++warnings;
  else if(t == QtCriticalMsg || t == QtFatalMsg)
    ++criticals;
  const bool bad = t == QtWarningMsg || t == QtCriticalMsg || t == QtFatalMsg;
  if(m.contains(QStringLiteral("VUID-")) || m.contains(QStringLiteral("Validation Error")))
  {
    if(vk_validation++ == 0)
      first_gpu_error = m.left(400).toStdString();
  }
  else if(m.contains(QStringLiteral("GL-VALIDATION")) && m.contains(QStringLiteral("HighSeverity")))
  {
    if(gl_errors++ == 0 && first_gpu_error.empty())
      first_gpu_error = m.left(400).toStdString();
  }
  if(bad && distinct.size() < 200)
  {
    auto s = m.left(160).toStdString();
    distinct[s]++;
  }
  if(chained)
    chained(t, c, m);
}
inline void install() { chained = qInstallMessageHandler(&handler); }
}

// stderr without stdio: the crash and thread-dump handlers run inside signal
// handlers, where only write(2) is safe.
void err_write(const char* buf, int len)
{
  if(len <= 0)
    return;
#if defined(_WIN32)
  _write(2, buf, unsigned(len));
#else
  [[maybe_unused]] auto r = ::write(2, buf, size_t(len));
#endif
}

// SCORE_ASSERT is a bare DEBUG_BREAK in debug builds: a SIGTRAP with no
// message. Print every frame as "BT <module> <offset>" so run-edit-fuzz.sh can
// symbolize it with llvm-symbolizer (gdb 15 cannot read clang 23's DWARF), or
// atos on macOS.
void print_backtrace_frames()
{
#if defined(SCORE_FUZZ_BACKTRACE)
  void* frames[128];
  const int n = backtrace(frames, 128);
  char buf[1024];
  for(int i = 0; i < n; i++)
  {
    Dl_info info{};
    int len;
    if(dladdr(frames[i], &info) && info.dli_fname)
      len = snprintf(
          buf, sizeof(buf), "BT %s 0x%llx\n", info.dli_fname,
          (unsigned long long)((char*)frames[i] - (char*)info.dli_fbase));
    else
      len = snprintf(buf, sizeof(buf), "BT ? %p\n", frames[i]);
    err_write(buf, len);
  }
#endif
}

// A thread's label in the THREAD lines of a dump: std::thread::id is the only
// portable handle. Hashing it does not allocate.
unsigned long long thread_label()
{
  return (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id());
}

#if defined(SCORE_FUZZ_POSIX_SIGNALS)
pthread_t g_main_thread{};
// Set by a thread that forwards an external SIGABRT to the GUI thread, so the
// GUI thread knows the abort it receives is a timeout and not its own assert.
std::atomic<bool> g_abort_forwarded{false};

// On a timeout, every other thread prints its stack too (SIGUSR2), so a
// deadlock shows who holds the lock. Serialized by a spin flag.
std::atomic_flag g_thread_dump_busy = ATOMIC_FLAG_INIT;
void thread_dump_handler(int)
{
  while(g_thread_dump_busy.test_and_set())
    ;
  char buf[64];
  const bool main = pthread_equal(pthread_self(), g_main_thread);
  int len = snprintf(buf, sizeof(buf), "THREAD %llx%s\n", thread_label(), main ? " main" : "");
  err_write(buf, len);
  print_backtrace_frames();
  err_write("ENDTHREAD\n", 10);
  g_thread_dump_busy.clear();
}

// Makes every thread of the process but the calling one print its stack,
// the GUI thread first. POSIX has no way to list a process's threads: Linux
// reads /proc, macOS asks the Mach task; elsewhere only the GUI thread is
// dumped.
void dump_threads()
{
  using namespace std::chrono_literals;
  const pthread_t self = pthread_self();
  if(!pthread_equal(self, g_main_thread))
  {
    pthread_kill(g_main_thread, SIGUSR2);
    std::this_thread::sleep_for(50ms);
  }
#if defined(__linux__)
  // Thread ids are kernel ids here: signalling one needs tgkill (glibc 2.30).
  const pid_t self_tid = gettid();
  const pid_t main_tid = getpid();
  if(DIR* d = opendir("/proc/self/task"))
  {
    while(dirent* e = readdir(d))
    {
      const long tid = atol(e->d_name);
      if(tid <= 0 || tid == self_tid || tid == main_tid)
        continue;
      tgkill(main_tid, pid_t(tid), SIGUSR2);
      std::this_thread::sleep_for(20ms);
    }
    closedir(d);
  }
#elif defined(__APPLE__)
  thread_act_array_t list{};
  mach_msg_type_number_t count{};
  if(task_threads(mach_task_self(), &list, &count) == KERN_SUCCESS)
  {
    for(mach_msg_type_number_t i = 0; i < count; i++)
    {
      const pthread_t t = pthread_from_mach_thread_np(list[i]);
      if(t && !pthread_equal(t, self) && !pthread_equal(t, g_main_thread))
      {
        pthread_kill(t, SIGUSR2);
        std::this_thread::sleep_for(20ms);
      }
      mach_port_deallocate(mach_task_self(), list[i]);
    }
    vm_deallocate(
        mach_task_self(), (vm_address_t)list, count * sizeof(thread_act_t));
  }
#endif
  std::this_thread::sleep_for(200ms);
}

// After a jump to a bad address (a freed vtable or function pointer),
// backtrace() finds no frames. Print the interrupted PC, then every word of
// the interrupted stack that points into a loaded module, as "STACKWORD"
// lines: the first ones are the return addresses of the caller chain.
void print_stack_scan(void* uctx)
{
#if defined(SCORE_FUZZ_STACK_SCAN) && defined(SCORE_FUZZ_BACKTRACE)
  auto* uc = static_cast<ucontext_t*>(uctx);
  if(!uc)
    return;
  auto pc = (void*)uc->uc_mcontext.gregs[REG_RIP];
  auto sp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
  char buf[1024];
  int len = snprintf(buf, sizeof(buf), "PC %p SP %p\n", pc, (void*)sp);
  err_write(buf, len);

  // Bounds of this thread's stack, so the scan never faults.
  uintptr_t lo = sp, hi = sp + 64 * 1024;
  pthread_attr_t attr;
  if(pthread_getattr_np(pthread_self(), &attr) == 0)
  {
    void* addr{};
    size_t size{};
    if(pthread_attr_getstack(&attr, &addr, &size) == 0)
    {
      lo = (uintptr_t)addr;
      hi = lo + size;
    }
    pthread_attr_destroy(&attr);
  }
  if(sp < lo || sp >= hi)
    return;

  int found = 0;
  for(uintptr_t p = sp & ~uintptr_t(7); p + sizeof(void*) <= hi && found < 48; p += sizeof(void*))
  {
    void* w = *(void**)p;
    Dl_info info{};
    if(w && dladdr(w, &info) && info.dli_fname)
    {
      len = snprintf(
          buf, sizeof(buf), "STACKWORD %s 0x%llx\n", info.dli_fname,
          (unsigned long long)((char*)w - (char*)info.dli_fbase));
      err_write(buf, len);
      found++;
    }
  }
#else
  (void)uctx;
#endif
}

void crash_handler(int sig, siginfo_t* info, void* uctx)
{
  // An external SIGABRT (the driver's backstop timeout(1), or a user's kill)
  // may land on any thread (often D-Bus's): forward it to the GUI thread,
  // which is the one to trace. An abort() raised in this process (a failed
  // assert, libc++abi's "pure virtual function called", malloc's corruption
  // checks) is traced on the thread that raised it: returning would let
  // abort() kill the process before the GUI thread prints anything.
  const bool on_main = pthread_equal(pthread_self(), g_main_thread);
  const bool external = info && info->si_code == SI_USER && info->si_pid != getpid();
  const bool forwarded = on_main && g_abort_forwarded.exchange(false);
  if(sig == SIGABRT && external && !on_main)
  {
    g_abort_forwarded = true;
    pthread_kill(g_main_thread, SIGABRT);
    return;
  }
  // The external abort reaching the GUI thread: a hang, so dump the others
  // first; a real assert/crash only needs this thread.
  if(sig == SIGABRT && (external || forwarded) && on_main)
    dump_threads();
  char buf[64];
  int len = (sig == SIGSEGV || sig == SIGBUS) && info
                ? snprintf(buf, sizeof(buf), "CRASH signal %d addr %p\n", sig, info->si_addr)
                : snprintf(buf, sizeof(buf), "CRASH signal %d\n", sig);
  err_write(buf, len);
  print_backtrace_frames();
  if(sig == SIGSEGV || sig == SIGBUS || sig == SIGILL)
    print_stack_scan(uctx);
  signal(sig, SIG_DFL);
  raise(sig);
}

void install_crash_handler()
{
  g_main_thread = pthread_self();
  // An alternate stack, so a stack overflow (deep recursion) can still be
  // reported: the handler would otherwise fault on the exhausted stack.
  static std::vector<char> altstack(1 << 20);
  stack_t ss{};
  ss.ss_sp = altstack.data();
  ss.ss_size = altstack.size();
  sigaltstack(&ss, nullptr);
  // Only SIGSEGV goes to it: unwinding from the alternate stack loses the
  // interrupted frames, which the other signals (asserts, aborts) need.
  struct sigaction sa{};
  sa.sa_sigaction = crash_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  for(int sig : {SIGTRAP, SIGABRT, SIGBUS, SIGILL, SIGFPE})
    sigaction(sig, &sa, nullptr);
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaction(SIGSEGV, &sa, nullptr);
  signal(SIGUSR2, thread_dump_handler);
}
#else
// Standard C signals only: no siginfo, no other threads' stacks.
void dump_threads() { }

void crash_handler(int sig)
{
  char buf[64];
  int len = snprintf(buf, sizeof(buf), "CRASH signal %d\n", sig);
  err_write(buf, len);
  print_backtrace_frames();
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

void install_crash_handler()
{
  for(int sig : {SIGABRT, SIGILL, SIGFPE, SIGSEGV})
    std::signal(sig, crash_handler);
}
#endif

// In-process timeout (--timeout N seconds), so a hang is reported the same
// way on every platform, without GNU timeout(1): when the deadline passes
// before the run ends, the stacks of all threads are printed (the GUI thread
// first) and the process exits with 124, timeout(1)'s code, which
// run-edit-fuzz.sh reports as TIMEOUT.
struct Watchdog
{
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
  std::thread thread;

  void start(int seconds)
  {
    if(seconds <= 0)
      return;
    thread = std::thread{[this, seconds] {
      std::unique_lock lock{mutex};
      if(cv.wait_for(lock, std::chrono::seconds(seconds), [this] { return done; }))
        return;
      char buf[64];
      int len = snprintf(buf, sizeof(buf), "TIMEOUT after %d s\n", seconds);
      err_write(buf, len);
      dump_threads();
      std::fflush(stdout);
      std::_Exit(124);
    }};
  }

  ~Watchdog()
  {
    if(!thread.joinable())
      return;
    {
      std::lock_guard lock{mutex};
      done = true;
    }
    cv.notify_all();
    thread.join();
  }
};

void stage(int n, const char* op, const std::string& what = {})
{
  static const auto t0 = std::chrono::steady_clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
  std::fprintf(stderr, "STAGE %d %s %s [t=%lld]\n", n, op, what.c_str(), (long long)ms);
  std::fflush(stderr);
}

std::string json_escape(const std::string& s)
{
  std::string r;
  r.reserve(s.size());
  for(char c : s)
  {
    if(c == '"' || c == '\\')
      r += '\\';
    if((unsigned char)c < 0x20)
    {
      r += ' ';
      continue;
    }
    r += c;
  }
  return r;
}

struct Verdict
{
  std::string status = "OK";
  std::string note;
  ossia::flat_map<std::string, int> ops; // op name -> times it actually did something
  int stack_size = 0;
};

void note_append(std::string& note, const std::string& what)
{
  if(!note.empty())
    note += "; ";
  note += what;
}

[[noreturn]] void finish(const std::string& file, uint32_t seed, Verdict v)
{
  // GPU API misuse is a failure even when nothing crashed.
  if(v.status == "OK" && msgs::vk_validation > 0)
    v.status = "VK_VALIDATION";
  else if(v.status == "OK" && msgs::gl_errors > 0)
    v.status = "GL_ERROR";
  if(msgs::vk_validation + msgs::gl_errors > 0)
    note_append(
        v.note, std::to_string(msgs::vk_validation) + " Vulkan validation / "
                    + std::to_string(msgs::gl_errors)
                    + " GL errors, first: " + msgs::first_gpu_error);
  std::string ops;
  for(auto& [k, n] : v.ops)
    ops += (ops.empty() ? "" : ",") + ("\"" + k + "\":" + std::to_string(n));
  std::string warn;
  int shown = 0;
  std::vector<std::pair<int, std::string>> sorted;
  for(auto& [m, n] : msgs::distinct)
    sorted.push_back({n, m});
  std::sort(sorted.rbegin(), sorted.rend());
  for(auto& [n, m] : sorted)
  {
    if(shown++ == 12)
      break;
    warn += (warn.empty() ? "" : ",") + ("\"" + json_escape(std::to_string(n) + "x " + m) + "\"");
  }
  std::printf(
      "{\"mode\":\"edit-fuzz\",\"file\":\"%s\",\"seed\":%u,\"status\":\"%s\","
      "\"stack\":%d,\"warnings\":%d,\"criticals\":%d,\"ops\":{%s},\"top_messages\":[%s],"
      "\"note\":\"%s\"}\n",
      json_escape(file).c_str(), seed, v.status.c_str(), v.stack_size, msgs::warnings,
      msgs::criticals, ops.c_str(), warn.c_str(), json_escape(v.note).c_str());
  std::fflush(stdout);
  // Same rationale as ScoreCorpusTester: everything under test already ran,
  // including the document close; process teardown is not under test.
  std::_Exit(0);
}

// ---------------------------------------------------------------------------
// Pre-scan, environment: identical to ScoreCorpusTester.cpp.
// ---------------------------------------------------------------------------
struct Prescan
{
  bool json = false;
  ossia::flat_set<QString> process_uuids;
  QStringList device_names;
};

void prescan_walk(const QJsonValue& val, Prescan& out, bool in_processes)
{
  if(val.isObject())
  {
    const QJsonObject obj = val.toObject();
    if(in_processes)
    {
      const auto u = obj.value(QStringLiteral("uuid"));
      if(u.isString())
        out.process_uuids.insert(u.toString().toLower());
    }
    if(obj.contains(QStringLiteral("Protocol"))
       && obj.value(QStringLiteral("Name")).isString())
      out.device_names.push_back(obj.value(QStringLiteral("Name")).toString());
    for(auto it = obj.begin(); it != obj.end(); ++it)
      prescan_walk(it.value(), out, it.key() == QStringLiteral("Processes"));
  }
  else if(val.isArray())
  {
    for(const auto& elem : val.toArray())
      prescan_walk(elem, out, in_processes);
  }
}

Prescan prescan(const QByteArray& bytes)
{
  Prescan out;
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(bytes, &err);
  if(err.error != QJsonParseError::NoError || !doc.isObject())
    return out;
  out.json = true;
  prescan_walk(doc.object(), out, false);
  return out;
}

bool has_display()
{
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == QStringLiteral("offscreen"))
    return false;
  return qEnvironmentVariableIsSet("DISPLAY") || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
}

void prepare_environment()
{
#if defined(SCORE_TEST_BINARY_DIR)
  if(!QDir{QStringLiteral("plugins")}.exists())
    QDir::setCurrent(QStringLiteral(SCORE_TEST_BINARY_DIR));
#endif
  if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && !has_display())
    qputenv("QT_QPA_PLATFORM", "offscreen");
  if(!qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"))
    qputenv("SCORE_SANITIZE_SKIP_CHECKS", "1");
  if(!qEnvironmentVariableIsSet("SCORE_AUDIO_BACKEND"))
    qputenv("SCORE_AUDIO_BACKEND", "dummy");
  if(!qEnvironmentVariableIsSet("SCORE_LOCAL_OSC_PORT"))
    qputenv("SCORE_LOCAL_OSC_PORT", "0");
  if(!qEnvironmentVariableIsSet("SCORE_LOCAL_WS_PORT"))
    qputenv("SCORE_LOCAL_WS_PORT", "0");
  if(!qEnvironmentVariableIsSet("XDG_CONFIG_HOME"))
  {
    const QString cfg = QDir::tempPath() + "/score-edit-fuzz/config."
                        + QString::number(QCoreApplication::applicationPid());
    QDir{}.mkpath(cfg);
    qputenv("XDG_CONFIG_HOME", cfg.toUtf8());
    // XDG_CONFIG_HOME only moves the settings on Linux; macOS (plist) and
    // Windows (registry) native settings would land in the user's own.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, cfg);
  }
  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  // "score": the user library / packages path derives from it, see
  // ScoreCorpusTester.cpp.
  QCoreApplication::setApplicationName("score");
}

// ---------------------------------------------------------------------------
// Document structure, as a sorted list of lines. Compared after undo-all
// (against the loaded document) and after redo-all (against the pre-undo one).
// ---------------------------------------------------------------------------
std::string path_of(const QObject* o)
{
  std::string p;
  for(; o; o = o->parent())
  {
    std::string seg = o->objectName().toStdString();
    if(auto ida = qobject_cast<const IdentifiedObjectAbstract*>(o))
      seg += "." + std::to_string(ida->id_val());
    p = seg + (p.empty() ? "" : "/") + p;
    if(qobject_cast<const score::DocumentModel*>(o))
      break;
  }
  return p;
}

std::vector<std::string> structure(score::Document& doc)
{
  std::vector<std::string> out;
  auto& root = doc.model();
  for(auto* p : root.findChildren<Process::ProcessModel*>())
  {
    out.push_back("P " + path_of(p) + " " + p->metaObject()->className());
    for(auto* in : p->inlets())
      out.push_back(
          "I " + path_of(in) + " @" + in->address().toString().toStdString());
    for(auto* o : p->outlets())
      out.push_back(
          "O " + path_of(o) + " @" + o->address().toString().toStdString());
  }
  for(auto* c : root.findChildren<Process::Cable*>())
    out.push_back(
        "C " + path_of(c) + " " + c->source().unsafePath().toString().toStdString()
        + " -> " + c->sink().unsafePath().toString().toStdString());
  for(auto* x : root.findChildren<Scenario::IntervalModel*>())
    out.push_back("IV " + path_of(x));
  for(auto* x : root.findChildren<Scenario::StateModel*>())
    out.push_back("ST " + path_of(x));
  for(auto* x : root.findChildren<Scenario::EventModel*>())
    out.push_back("EV " + path_of(x));
  for(auto* x : root.findChildren<Scenario::TimeSyncModel*>())
    out.push_back("TS " + path_of(x));
  std::sort(out.begin(), out.end());
  return out;
}

std::string diff_summary(
    const std::vector<std::string>& expected, const std::vector<std::string>& got)
{
  std::vector<std::string> missing, extra;
  std::set_difference(
      expected.begin(), expected.end(), got.begin(), got.end(),
      std::back_inserter(missing));
  std::set_difference(
      got.begin(), got.end(), expected.begin(), expected.end(),
      std::back_inserter(extra));
  std::string s = std::to_string(missing.size()) + " missing, "
                  + std::to_string(extra.size()) + " extra";
  for(std::size_t i = 0; i < std::min<std::size_t>(4, missing.size()); i++)
    s += " | -" + missing[i];
  for(std::size_t i = 0; i < std::min<std::size_t>(4, extra.size()); i++)
    s += " | +" + extra[i];
  return s;
}

// ---------------------------------------------------------------------------
// The fuzzer itself. One instance per document, driven by single-shot timers.
// ---------------------------------------------------------------------------
struct Fuzzer
{
  std::string file;
  score::Document* doc{};
  uint32_t seed{};
  int max_ops{};
  std::mt19937 rng;
  Verdict v;
  std::vector<std::string> initial;
  int op = 0;

  const score::DocumentContext& ctx() const { return doc->context(); }
  score::CommandStack& stack() { return doc->commandStack(); }
  Execution::ExecutionController& exec()
  {
    return score::GUIAppContext()
        .guiApplicationPlugin<Engine::ApplicationPlugin>()
        .execution();
  }
  bool playing()
  {
    auto* e = ctx().findPlugin<Execution::DocumentPlugin>();
    return e && e->isPlaying();
  }

  int rnd(int n) { return n <= 0 ? 0 : std::uniform_int_distribution<int>{0, n - 1}(rng); }
  template <typename T>
  T* pick(const QList<T*>& l)
  {
    return l.isEmpty() ? nullptr : l[rnd(int(l.size()))];
  }

  // Goes through every ObjectEditor exactly like
  // ObjectMenuActions::removeSelectedElements.
  bool remove_via_editors(const Selection& sel, bool shift)
  {
    auto& c = ctx();
    c.selectionStack.pushNewSelection(sel);
    QGuiApplicationPrivate::modifier_buttons
        = shift ? Qt::ShiftModifier : Qt::NoModifier;
    bool done = false;
    for(auto& iface : c.app.interfaces<score::ObjectEditorList>())
      if(iface.remove(c.selectionStack.currentSelection(), c))
      {
        done = true;
        break;
      }
    QGuiApplicationPrivate::modifier_buttons = Qt::NoModifier;
    return done;
  }

  void count(const char* name, bool did)
  {
    if(did)
      v.ops[name]++;
  }

  void op_remove_process(bool keep_link)
  {
    auto procs = doc->model().findChildren<Process::ProcessModel*>();
    // Never the root scenario: removing it is not a user operation.
    procs.removeIf([](auto* p) {
      return qobject_cast<Scenario::ScenarioDocumentModel*>(p->parent())
             || !p->parent();
    });
    auto* p = pick(procs);
    if(!p)
      return;
    stage(op, keep_link ? "remove-keep-link" : "remove", path_of(p) + " " + p->metaObject()->className());
    Selection s;
    s.append(p);
    count(keep_link ? "remove_keep_link" : "remove", remove_via_editors(s, keep_link));
  }

  void op_remove_cable()
  {
    auto cables = doc->model().findChildren<Process::Cable*>();
    auto* c = pick(cables);
    if(!c)
      return;
    stage(op, "remove-cable", path_of(c));
    Selection s;
    s.append(c);
    count("remove_cable", remove_via_editors(s, rnd(2)));
  }

  void op_remove_scenario_element()
  {
    auto scenarios = doc->model().findChildren<Scenario::ProcessModel*>();
    auto* sm = pick(scenarios);
    if(!sm)
      return;
    QList<IdentifiedObjectAbstract*> elts;
    for(auto& x : sm->intervals)
      elts.push_back(&x);
    for(auto& x : sm->states)
      elts.push_back(&x);
    for(auto& x : sm->events)
      elts.push_back(&x);
    for(auto& x : sm->timeSyncs)
      elts.push_back(&x);
    for(auto& x : sm->comments)
      elts.push_back(&x);
    if(elts.isEmpty())
      return;
    Selection s;
    const int n = 1 + (rnd(4) == 0 ? rnd(4) : 0);
    std::string what = path_of(sm) + " :";
    for(int i = 0; i < n; i++)
    {
      auto* e = elts[rnd(int(elts.size()))];
      if(!s.contains(e))
      {
        s.append(e);
        what += " " + std::string(e->metaObject()->className()) + "." + std::to_string(e->id_val());
      }
    }
    stage(op, "remove-scenario-elements", what);
    doc->focusManager().set(QPointer<Scenario::ProcessModel>{sm});
    count("remove_scenario_elements", remove_via_editors(s, false));
    doc->focusManager().set(nullptr);
  }

  void op_create_cable()
  {
    auto outs = doc->model().findChildren<Process::Outlet*>();
    auto ins = doc->model().findChildren<Process::Inlet*>();
    if(outs.isEmpty() || ins.isEmpty())
      return;
    // A few tries for a type-compatible pair, as a user would drag.
    for(int i = 0; i < 8; i++)
    {
      auto* o = pick(outs);
      auto* in = pick(ins);
      if(o->type() != in->type())
        continue;
      stage(op, "create-cable", path_of(o) + " -> " + path_of(in));
      const int before = stack().size();
      Dataflow::onCreateCable(ctx(), *o, *in);
      count("create_cable", stack().size() != before);
      return;
    }
  }

  void op_transport()
  {
    switch(rnd(4))
    {
      case 0:
        stage(op, "stop-play");
        exec().request_stop();
        QTimer::singleShot(rnd(50), [this] { exec().request_play_local(true); });
        break;
      case 1:
        stage(op, "pause-resume");
        exec().request_play_local(false);
        QTimer::singleShot(rnd(50), [this] { exec().request_play_local(true); });
        break;
      case 2:
      {
        auto& root = doc->model().modelDelegate();
        auto* sd = qobject_cast<Scenario::ScenarioDocumentModel*>(&root);
        if(!sd)
          return;
        const auto dur = sd->baseInterval().duration.defaultDuration();
        const auto t = TimeVal::fromMsecs(rnd(std::max(1, int(dur.msec()))));
        stage(op, "play-from-here", std::to_string(t.msec()));
        exec().request_play_from_here(t);
        break;
      }
      default:
        stage(op, "play");
        exec().request_play_local(true);
        break;
    }
    v.ops["transport"]++;
  }

  // Undo / redo storms, one step per event-loop turn at 0 ms: as fast as a
  // held Ctrl+Z can possibly go, and faster than any key auto-repeat.
  void burst(int undos, int redos, std::function<void()> then)
  {
    // Stops early when the stack runs out: undo-all / redo-all pass 1 << 30.
    if(undos > 0 && stack().canUndo())
    {
      stack().undo();
      v.ops["undo"]++;
      QTimer::singleShot(0, [this, undos, redos, then] { burst(undos - 1, redos, then); });
      return;
    }
    if(redos > 0 && stack().canRedo())
    {
      stack().redo();
      v.ops["redo"]++;
      QTimer::singleShot(0, [this, redos, then] { burst(0, redos - 1, then); });
      return;
    }
    then();
  }

  void pingpong(int n, std::function<void()> then)
  {
    if(n <= 0)
      return then();
    if(n % 2 ? stack().canUndo() : stack().canRedo())
    {
      n % 2 ? stack().undo() : stack().redo();
      v.ops["pingpong"]++;
    }
    QTimer::singleShot(0, [this, n, then] { pingpong(n - 1, then); });
  }

  void next()
  {
    if(op >= max_ops)
      return QTimer::singleShot(200, [this] { finale(); });
    ++op;
    auto again = [this] { QTimer::singleShot(std::array{0, 0, 1, 2, 5, 10, 30}[rnd(7)], [this] { next(); }); };

    const int r = rnd(100);
    if(r < 18)
      op_remove_process(false);
    else if(r < 36)
      op_remove_process(true);
    else if(r < 48)
      op_remove_cable();
    else if(r < 58)
      op_remove_scenario_element();
    else if(r < 66)
      op_create_cable();
    else if(r < 71)
      op_transport();
    else if(r < 92)
    {
      const int u = 1 + rnd(64);
      const int re = rnd(u + 1);
      stage(op, "undo-redo-burst", std::to_string(u) + "/" + std::to_string(re));
      return burst(u, re, again);
    }
    else
    {
      const int n = 2 * (5 + rnd(100));
      stage(op, "pingpong", std::to_string(n));
      return pingpong(n, again);
    }
    again();
  }

  void finale()
  {
    v.stack_size = stack().size();
    // Make sure we are playing for the undo-all, whatever the transport ops
    // left behind.
    exec().request_play_local(true);
    QTimer::singleShot(300, [this] {
      const auto before_undo = structure(*doc);
      // The stack may have a redo tail (undone commands): redo-all must stop
      // where the document was, not replay that tail too.
      const int index = stack().currentIndex();
      stage(op, "undo-all", std::to_string(index));
      burst(1 << 30, 0, [this, before_undo, index] {
        const auto after_undo = structure(*doc);
        if(after_undo != initial)
        {
          v.status = "UNDO_MISMATCH";
          note_append(v.note, "undo-all vs loaded: " + diff_summary(initial, after_undo));
        }
        stage(op, "redo-all", std::to_string(index));
        burst(0, index - stack().currentIndex(), [this, before_undo] {
          const auto after_redo = structure(*doc);
          if(after_redo != before_undo && v.status == "OK")
          {
            v.status = "REDO_MISMATCH";
            note_append(
                v.note, "redo-all vs pre-undo: " + diff_summary(before_undo, after_redo));
          }
          stage(op, "undo-all-2");
          burst(1 << 30, 0, [this] { restart_check(); });
        });
      });
    });
  }

  void restart_check()
  {
    stage(op, "restart");
    exec().request_stop();
    QTimer::singleShot(400, [this] {
      exec().request_play_local(true);
      QTimer::singleShot(1000, [this] {
        if(!playing() && v.status == "OK")
        {
          v.status = "NOT_PLAYING";
          note_append(v.note, "execution did not restart after the edits");
        }
        exec().request_stop();
        QTimer::singleShot(300, [this] {
          stage(op, "close");
          auto& c = score::GUIAppContext();
          c.docManager.forceCloseDocument(c, *doc);
          QCoreApplication::processEvents();
          finish(file, seed, v);
        });
      });
    });
  }
};

void run_document(
    const QString& path, const Prescan& scan, uint32_t seed, int ops, int warmup_ms)
{
  static Fuzzer f;
  f.file = path.toStdString();
  f.seed = seed;
  f.max_ops = ops;
  f.rng.seed(seed);
  const auto& ctx = score::GUIAppContext();

  if(scan.json)
  {
    ossia::flat_set<QString> known;
    for(auto& factory : ctx.interfaces<Process::ProcessFactoryList>())
      known.insert(
          QString::fromUtf8(score::uuids::toByteArray(factory.concreteKey().impl()))
              .toLower());
    for(const auto& u : scan.process_uuids)
      if(known.find(u) == known.end())
      {
        f.v.status = "UNKNOWN_UUID:" + u.toStdString();
        finish(f.file, seed, f.v);
      }
  }

  stage(0, "load");
  try
  {
    f.doc = ctx.docManager.loadFile(ctx, path);
  }
  catch(const std::exception& e)
  {
    note_append(f.v.note, std::string{"exception: "} + e.what());
  }
  if(!f.doc)
  {
    f.v.status = "LOADFAIL";
    finish(f.file, seed, f.v);
  }
  QApplication::processEvents();
  f.initial = structure(*f.doc);
  note_append(f.v.note, std::to_string(f.initial.size()) + " structure lines");

  // A play request made right after loading a heavy document is dropped (see
  // the F3 entry of the bug ledger: 100 ms is lost on generative-3d.score,
  // 1 s is not). SCORE_EDIT_FUZZ_PLAY_DELAY_MS overrides the delay.
  const int play_delay = qEnvironmentVariableIsSet("SCORE_EDIT_FUZZ_PLAY_DELAY_MS")
                             ? qEnvironmentVariableIntValue("SCORE_EDIT_FUZZ_PLAY_DELAY_MS")
                             : 1000;
  QTimer::singleShot(play_delay, [warmup_ms] {
    stage(0, "play");
    f.exec().request_play_local(true);
    // Then wait for the graph to actually run (heavy 3D documents take
    // seconds to start in a debug build on llvmpipe), plus warmup_ms.
    auto* poll = new QTimer{qApp};
    auto waited = std::make_shared<int>(0);
    QObject::connect(poll, &QTimer::timeout, [poll, waited, warmup_ms] {
      *waited += 100;
      if(!f.playing() && *waited < 15000)
        return;
      poll->deleteLater();
      if(!f.playing())
        note_append(f.v.note, "not playing after 15 s");
      else
        note_append(f.v.note, "playing after " + std::to_string(*waited) + " ms");
      QTimer::singleShot(warmup_ms, [] { f.next(); });
    });
    poll->start(100);
  });
}
} // namespace

int main(int argc, char** argv)
{
  QString file;
  uint32_t seed = 1;
  int ops = 400;
  int warmup_ms = 1500;
  int timeout_s = 0;
  for(int i = 1; i < argc; i++)
  {
    const std::string a = argv[i];
    if(a == "--seed" && i + 1 < argc)
      seed = uint32_t(std::strtoul(argv[++i], nullptr, 10));
    else if(a == "--ops" && i + 1 < argc)
      ops = std::max(0, atoi(argv[++i]));
    else if(a == "--warmup-ms" && i + 1 < argc)
      warmup_ms = std::max(0, atoi(argv[++i]));
    else if(a == "--timeout" && i + 1 < argc)
      timeout_s = std::max(0, atoi(argv[++i]));
    else
      file = QString::fromLocal8Bit(argv[i]);
  }
  if(file.isEmpty())
  {
    std::fprintf(
        stderr, "usage: %s [--seed N] [--ops N] [--warmup-ms N] [--timeout S] <file.score>\n",
        argv[0]);
    return 2;
  }
  file = QFileInfo{file}.absoluteFilePath();

  QByteArray bytes;
  {
    QFile f{file};
    if(!f.open(QIODevice::ReadOnly))
    {
      std::fprintf(stderr, "cannot read: %s\n", file.toUtf8().constData());
      return 2;
    }
    bytes = f.readAll();
  }

  // Never play a document with a Shell command process (avendish
  // examples/Raw/Shell.hpp): it runs its command line on every trigger, and
  // the fuzzer triggers it hundreds of times (shell-command.score opened ~160
  // terminals running telnet to a remote host).
  if(bytes.contains("7e4ae744-1825-4f1c-9fc9-675e41f316bc"))
  {
    std::printf(
        "{\"mode\":\"edit-fuzz\",\"file\":\"%s\",\"seed\":%u,\"status\":\"SKIPPED_SHELL\"}\n",
        file.toUtf8().constData(), seed);
    return 0;
  }

  msgs::install();
  install_crash_handler();
  Watchdog watchdog;
  watchdog.start(timeout_s);
  const Prescan scan = prescan(bytes);
  if(!scan.device_names.isEmpty()
     && !qEnvironmentVariableIsSet("SCORE_FORCE_OFFSCREEN_WINDOW"))
    qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", scan.device_names.join(',').toUtf8());

  prepare_environment();
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  ossia::context ossia_ctx;
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};

  QMetaObject::invokeMethod(
      &app, [&] { run_document(file, scan, seed, ops, warmup_ms); },
      Qt::QueuedConnection);
  return app.exec();
}
