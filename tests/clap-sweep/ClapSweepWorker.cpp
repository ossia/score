// The worker of the CLAP system sweep (clap_sweep.py drives it): every CLAP
// plug-in installed on the machine, run through score's host with a stimulus
// fitting what it is (an instrument gets notes, an effect gets a signal), and
// through a minimal host of its own written against the specification only.
// A plug-in that misbehaves in score but not in that host points at score; in
// both, at the plug-in.
//
// Both hosts load the plug-in through clap_checker.clap (ClapChecker.cpp),
// which forwards every call to it and reports what a host did that the
// specification forbids: a main-thread call on the audio thread, a process
// call while not processing, events out of order...
//
// The worker measures and the driver judges: thresholds live in clap_sweep.py.
//
//  SCORE_CLAP_SWEEP_LIST=<file> SCORE_CLAP_SWEEP_FILE=<.clap>
//      write the plug-ins of that file, one per line:
//      path \t id \t name \t vendor \t version \t features (;-separated)
//  SCORE_CLAP_SWEEP_JOBS=<file> SCORE_CLAP_SWEEP_RESULTS=<file>
//  SCORE_CLAP_SWEEP_CHECKER=<clap_checker.clap>
//      probe the plug-ins of these lines (path \t id), appending one JSON
//      object per line to the results: a "phase" record before each step, so
//      that a crash or a hang can be told where it happened, then a "result".
//  SCORE_CLAP_SWEEP_TRACE=1
//      print the controls before each play of score's host, and the peak of
//      each of its blocks, on stderr
//  SCORE_SWEEP_LIVE=1
//      instead, add each plug-in, still through the checker, to a playing
//      document in every way a user can (score_test/LivePlugin.hpp)
//
// None set, the test case has nothing to do and passes: ctest runs the
// driver, not this.

#include <Clap/EffectModel.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/LivePlugin.hpp>
#include <score_test/Project.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/data_copy.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>

#include <clap/all.h>
#include <libremidi/detail/conversion.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <execinfo.h>
#include <poll.h>
#include <unistd.h>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");

constexpr double play_seconds = 2.0;
constexpr double replay_seconds = 1.0;
constexpr unsigned settle_ms = 300;
constexpr std::size_t max_edited_params = 32;

//! The chord covers General MIDI drums (kick, snare, hat) as well as pitched
//! instruments, which may only map part of the keyboard.
constexpr int chord[]{36, 38, 42, 48, 60, 64, 67};
constexpr double chord_on = 0.05, chord_off = 1.2;
constexpr int lead = 72;
constexpr double lead_on = 1.4, lead_off = 1.8;

std::string json_escape(std::string_view s)
{
  std::string r;
  r.reserve(s.size() + 2);
  for(unsigned char c : s)
  {
    switch(c)
    {
      case '"': r += "\\\""; break;
      case '\\': r += "\\\\"; break;
      case '\n': r += "\\n"; break;
      case '\r': r += "\\r"; break;
      case '\t': r += "\\t"; break;
      default:
        if(c < 0x20)
        {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          r += buf;
        }
        else
          r += char(c);
    }
  }
  return r;
}

//! A JSON object written key by key
struct json
{
  std::ostringstream s;
  bool first = true;
  json() { s << '{'; }
  json& key(std::string_view k)
  {
    if(!first)
      s << ',';
    first = false;
    s << '"' << k << "\":";
    return *this;
  }
  json& str(std::string_view k, std::string_view v)
  {
    key(k);
    s << '"' << json_escape(v) << '"';
    return *this;
  }
  json& num(std::string_view k, double v)
  {
    key(k);
    if(ossia::safe_isfinite(v))
      s << v;
    else
      s << "null";
    return *this;
  }
  json& boolean(std::string_view k, bool v)
  {
    key(k);
    s << (v ? "true" : "false");
    return *this;
  }
  json& raw(std::string_view k, std::string_view v)
  {
    key(k);
    s << v;
    return *this;
  }
  json& strings(std::string_view k, const std::vector<std::string>& v)
  {
    key(k);
    s << '[';
    for(std::size_t i = 0; i < v.size(); i++)
      s << (i ? "," : "") << '"' << json_escape(v[i]) << '"';
    s << ']';
    return *this;
  }
  std::string done()
  {
    s << '}';
    return s.str();
  }
};

struct results_file
{
  std::FILE* f{};
  explicit results_file(const QString& path)
      : f{std::fopen(path.toLocal8Bit().constData(), "a")}
  {
  }
  ~results_file()
  {
    if(f)
      std::fclose(f);
  }
  void write(const std::string& line)
  {
    if(!f)
      return;
    std::fputs(line.c_str(), f);
    std::fputc('\n', f);
    std::fflush(f);
  }
  void phase(const std::string& uri, const char* phase)
  {
    write(json{}.str("type", "phase").str("uri", uri).str("phase", phase).done());
  }
};

//! A backtrace on the log before the worker dies, for the driver to show
void crash_handler(int sig)
{
  char head[64];
  const int n = std::snprintf(head, sizeof(head), "CRASH signal %d\n", sig);
  (void)!write(2, head, n);
  void* frames[64];
  const int depth = backtrace(frames, 64);
  backtrace_symbols_fd(frames, depth, 2);
  (void)!write(2, "CRASH end\n", 10);
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

void install_crash_handler()
{
  for(int sig : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT})
    std::signal(sig, crash_handler);
}

//! The same signal for both hosts: three partials and a little noise, each
//! channel a little apart from the others
struct stimulus
{
  int rate{};
  explicit stimulus(int r)
      : rate{r}
  {
  }
  double audio(int channel, int64_t frame) const noexcept
  {
    const double t = double(frame) / rate;
    const double ph = channel * 0.37;
    uint32_t x = uint32_t(frame * 2654435761u + channel * 40503u);
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    const double noise = (double(x & 0xffff) / 32768. - 1.) * 0.02;
    return 0.15 * std::sin(2 * M_PI * 110 * t + ph)
           + 0.15 * std::sin(2 * M_PI * 440 * t + ph)
           + 0.15 * std::sin(2 * M_PI * 1760 * t + ph) + noise;
  }

  struct note
  {
    int64_t frame;
    bool on;
    int pitch;
  };
  //! The notes of the frames [start; start + n[, in order, relative to start
  std::vector<note> notes(int64_t start, int n) const
  {
    std::vector<note> all;
    auto at = [&](double s) { return int64_t(s * rate); };
    for(int p : chord)
      all.push_back({at(chord_on), true, p});
    for(int p : chord)
      all.push_back({at(chord_off), false, p});
    all.push_back({at(lead_on), true, lead});
    all.push_back({at(lead_off), false, lead});
    std::vector<note> res;
    for(auto& e : all)
      if(e.frame >= start && e.frame < start + n)
        res.push_back({e.frame - start, e.on, e.pitch});
    return res;
  }
};

//! What came out of the audio outputs of a run
struct audio_stats
{
  double peak{};
  double sumsq{};
  double sum{};
  int64_t samples{};
  int64_t nonfinite{};
  double sumsq_held{};
  int64_t samples_held{};
  double peak_tail{};
  //! After the first 0.1 s: what a plug-in starting up gives in its first
  //! blocks does not count as sound
  double peak_late{};
  double max_diff_input{};
  bool compared_input{};
  double seconds{};

  void add(double v, double t)
  {
    if(!ossia::safe_isfinite(v))
    {
      nonfinite++;
      return;
    }
    const double a = std::abs(v);
    peak = std::max(peak, a);
    sumsq += v * v;
    sum += v;
    samples++;
    if(t >= chord_on && t < chord_off)
    {
      sumsq_held += v * v;
      samples_held++;
    }
    if(t >= seconds - 0.1)
      peak_tail = std::max(peak_tail, a);
    if(t >= 0.1)
      peak_late = std::max(peak_late, a);
  }

  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    j.num(k("peak"), peak);
    j.num(k("rms"), samples ? std::sqrt(sumsq / samples) : 0.);
    j.num(k("rms_held"), samples_held ? std::sqrt(sumsq_held / samples_held) : 0.);
    j.num(k("dc"), samples ? sum / samples : 0.);
    j.num(k("nonfinite"), double(nonfinite));
    j.num(k("peak_tail"), peak_tail);
    j.num(k("peak_late"), peak_late);
    if(compared_input)
      j.num(k("max_diff_input"), max_diff_input);
  }
};

//! What came out of the read-only parameters of a run, by parameter id
struct control_stats
{
  std::map<clap_id, std::pair<double, double>> range;
  int64_t nonfinite{};
  void add(clap_id id, double v)
  {
    if(!ossia::safe_isfinite(v))
    {
      nonfinite++;
      return;
    }
    auto [it, inserted] = range.try_emplace(id, v, v);
    if(!inserted)
    {
      it->second.first = std::min(it->second.first, v);
      it->second.second = std::max(it->second.second, v);
    }
  }
  int changed() const
  {
    int n = 0;
    for(auto& [_, r] : range)
      n += (r.second - r.first) > 1e-6;
    return n;
  }
};

struct run_stats
{
  audio_stats audio;
  control_stats controls;
  int64_t midi_out{};
  int64_t blocks{};
  double seconds{};
  double wall_seconds{};

  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    audio.write(j, prefix);
    j.num(k("controls_changed"), controls.changed());
    j.num(k("controls_reported"), double(controls.range.size()));
    j.num(k("controls_nonfinite"), double(controls.nonfinite));
    j.num(k("midi_out"), double(midi_out));
    j.num(k("blocks"), double(blocks));
    j.num(k("rt_factor"), seconds > 0 ? wall_seconds / seconds : 0.);
  }
};

//! The value an edited parameter is set to: away from its default, on a step
double edit_target(const clap_param_info_t& p)
{
  double v = p.min_value + 0.37 * (p.max_value - p.min_value);
  if(p.flags & CLAP_PARAM_IS_STEPPED)
    v = std::round(v);
  if(std::abs(v - p.default_value) < 1e-9)
  {
    v = p.min_value + 0.71 * (p.max_value - p.min_value);
    if(p.flags & CLAP_PARAM_IS_STEPPED)
      v = std::round(v);
  }
  return std::clamp(v, p.min_value, p.max_value);
}

bool editable(const clap_param_info_t& p)
{
  return !(p.flags & (CLAP_PARAM_IS_READONLY | CLAP_PARAM_IS_HIDDEN))
         && p.max_value > p.min_value;
}

std::string checker_take(const std::string& checker)
{
  // Loaded by both hosts already: this only finds the same image
  void* lib = dlopen(checker.c_str(), RTLD_LAZY | RTLD_NOLOAD);
  if(!lib)
    lib = dlopen(checker.c_str(), RTLD_LAZY);
  if(!lib)
    return "[]";
  using take_t = int (*)(char*, int);
  auto take = reinterpret_cast<take_t>(dlsym(lib, "clap_checker_take"));
  if(!take)
    return "[]";
  std::vector<char> buf(1 << 20);
  take(buf.data(), int(buf.size()));
  return std::string(buf.data());
}

void checker_set_target(const std::string& checker, const std::string& path)
{
  void* lib = dlopen(checker.c_str(), RTLD_LAZY);
  if(!lib)
    return;
  using set_t = void (*)(const char*);
  if(auto set = reinterpret_cast<set_t>(dlsym(lib, "clap_checker_set_target")))
    set(path.c_str());
}

//! A host written from the specification alone, sharing nothing with
//! score's: the main and the audio thread are this one, taking turns, and
//! say so through thread-check.
struct reference_host
{
  static inline thread_local int t_kind{0}; // 1 main, 2 audio

  struct timer
  {
    clap_id id;
    uint32_t period;
    std::chrono::steady_clock::time_point due;
  };

  clap_host_t host{};
  const clap_plugin_t* plugin{};
  const clap_plugin_timer_support_t* timer_ext{};
  const clap_plugin_posix_fd_support_t* fd_ext{};
  const clap_plugin_params_t* params{};
  std::vector<timer> timers;
  std::map<int, clap_posix_fd_flags_t> fds;
  clap_id next_timer{};
  bool callback_requested{};
  bool flush_requested{};
  bool active{};

  clap_host_thread_check_t thread_check{
      [](const clap_host_t*) { return t_kind == 1; },
      [](const clap_host_t*) { return t_kind == 2; }};
  clap_host_log_t log{[](const clap_host_t*, clap_log_severity, const char*) {}};
  clap_host_params_t host_params{
      [](const clap_host_t*, clap_param_rescan_flags) {},
      [](const clap_host_t*, clap_id, clap_param_clear_flags) {},
      [](const clap_host_t* h) { self(h).flush_requested = true; }};
  clap_host_state_t host_state{[](const clap_host_t*) {}};
  clap_host_latency_t host_latency{[](const clap_host_t*) {}};
  clap_host_tail_t host_tail{[](const clap_host_t*) {}};
  clap_host_audio_ports_t host_audio_ports{
      [](const clap_host_t*, uint32_t) { return false; },
      [](const clap_host_t*, uint32_t) {}};
  clap_host_note_ports_t host_note_ports{
      [](const clap_host_t*) -> uint32_t {
    return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2;
  },
      [](const clap_host_t*, uint32_t) {}};
  clap_host_timer_support_t host_timer{
      [](const clap_host_t* h, uint32_t period, clap_id* id) {
    auto& s = self(h);
    *id = s.next_timer++;
    s.timers.push_back(
        {*id, std::max<uint32_t>(period, 1),
         std::chrono::steady_clock::now() + std::chrono::milliseconds(period)});
    return true;
  },
      [](const clap_host_t* h, clap_id id) {
    auto& t = self(h).timers;
    const auto n = t.size();
    std::erase_if(t, [id](auto& x) { return x.id == id; });
    return t.size() != n;
  }};
  clap_host_posix_fd_support_t host_fd{
      [](const clap_host_t* h, int fd, clap_posix_fd_flags_t f) {
    return self(h).fds.emplace(fd, f).second;
  },
      [](const clap_host_t* h, int fd, clap_posix_fd_flags_t f) {
    auto& m = self(h).fds;
    auto it = m.find(fd);
    if(it == m.end())
      return false;
    it->second = f;
    return true;
  },
      [](const clap_host_t* h, int fd) { return self(h).fds.erase(fd) > 0; }};

  static reference_host& self(const clap_host_t* h)
  {
    return *static_cast<reference_host*>(h->host_data);
  }

  reference_host()
  {
    host.clap_version = CLAP_VERSION;
    host.host_data = this;
    host.name = "clap sweep reference host";
    host.vendor = "ossia.io";
    host.url = "https://ossia.io";
    host.version = "1.0";
    host.get_extension = [](const clap_host_t* h, const char* id) -> const void* {
      auto& s = self(h);
      const std::string_view e{id};
      if(e == CLAP_EXT_THREAD_CHECK)
        return &s.thread_check;
      if(e == CLAP_EXT_LOG)
        return &s.log;
      if(e == CLAP_EXT_PARAMS)
        return &s.host_params;
      if(e == CLAP_EXT_STATE)
        return &s.host_state;
      if(e == CLAP_EXT_LATENCY)
        return &s.host_latency;
      if(e == CLAP_EXT_TAIL)
        return &s.host_tail;
      if(e == CLAP_EXT_AUDIO_PORTS)
        return &s.host_audio_ports;
      if(e == CLAP_EXT_NOTE_PORTS)
        return &s.host_note_ports;
      if(e == CLAP_EXT_TIMER_SUPPORT)
        return &s.host_timer;
      if(e == CLAP_EXT_POSIX_FD_SUPPORT)
        return &s.host_fd;
      return nullptr;
    };
    host.request_restart = [](const clap_host_t*) {};
    host.request_process = [](const clap_host_t*) {};
    host.request_callback
        = [](const clap_host_t* h) { self(h).callback_requested = true; };
  }

  //! What the main thread does between two blocks
  void main_duties()
  {
    t_kind = 1;
    if(!plugin)
      return;
    if(callback_requested)
    {
      callback_requested = false;
      plugin->on_main_thread(plugin);
    }
    if(flush_requested && !active && params && params->flush)
    {
      flush_requested = false;
      clap_input_events_t in{
          nullptr, [](const clap_input_events_t*) -> uint32_t { return 0; },
          [](const clap_input_events_t*, uint32_t) -> const clap_event_header_t* {
        return nullptr;
      }};
      clap_output_events_t out{
          nullptr,
          [](const clap_output_events_t*, const clap_event_header_t*) { return true; }};
      params->flush(plugin, &in, &out);
    }
    const auto now = std::chrono::steady_clock::now();
    if(timer_ext)
    {
      const auto due = timers;
      for(auto& t : due)
        if(t.due <= now)
        {
          for(auto& x : timers)
            if(x.id == t.id)
              x.due = now + std::chrono::milliseconds(t.period);
          timer_ext->on_timer(plugin, t.id);
        }
    }
    if(fd_ext && !fds.empty())
    {
      std::vector<pollfd> p;
      for(auto& [fd, f] : fds)
        p.push_back(
            {fd, short((f & CLAP_POSIX_FD_READ ? POLLIN : 0)
                       | (f & CLAP_POSIX_FD_WRITE ? POLLOUT : 0)),
             0});
      if(::poll(p.data(), p.size(), 0) > 0)
        for(auto& e : p)
        {
          clap_posix_fd_flags_t f{};
          if(e.revents & POLLIN)
            f |= CLAP_POSIX_FD_READ;
          if(e.revents & POLLOUT)
            f |= CLAP_POSIX_FD_WRITE;
          if(e.revents & (POLLERR | POLLHUP))
            f |= CLAP_POSIX_FD_ERROR;
          if(f)
            fd_ext->on_fd(plugin, e.fd, f);
        }
    }
  }

  struct event_list
  {
    std::vector<std::vector<uint8_t>> storage;
    std::vector<const clap_event_header_t*> events;
    template <typename T>
    void push(const T& ev)
    {
      auto& s = storage.emplace_back(sizeof(T));
      std::memcpy(s.data(), &ev, sizeof(T));
    }
    void seal()
    {
      events.clear();
      for(auto& s : storage)
        events.push_back(reinterpret_cast<const clap_event_header_t*>(s.data()));
      std::stable_sort(events.begin(), events.end(), [](auto* a, auto* b) {
        return a->time < b->time;
      });
    }
    clap_input_events_t list()
    {
      return {
          this,
          [](const clap_input_events_t* l) -> uint32_t {
        return static_cast<event_list*>(l->ctx)->events.size();
      },
          [](const clap_input_events_t* l, uint32_t i) -> const clap_event_header_t* {
        auto& e = static_cast<event_list*>(l->ctx)->events;
        return i < e.size() ? e[i] : nullptr;
      }};
    }
  };

  struct out_collector
  {
    run_stats* res{};
    std::set<clap_id>* readonly{};
  };

  //! Values read back after setting the editable parameters, by id
  std::map<clap_id, double> edited;
  //! Parameters whose value changed through a state save and load
  int state_changed{-1};
  std::string error;

  bool run(
      const std::string& checker, const std::string& id, const stimulus& stim, int block,
      run_stats& res)
  {
    t_kind = 1;
    void* lib = dlopen(checker.c_str(), RTLD_LAZY);
    auto* entry
        = lib ? static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry")) : nullptr;
    if(!entry || !entry->init(checker.c_str()))
    {
      error = "entry init failed";
      return false;
    }
    auto* factory = static_cast<const clap_plugin_factory_t*>(
        entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    plugin = factory ? factory->create_plugin(factory, &host, id.c_str()) : nullptr;
    if(!plugin)
    {
      error = "create_plugin failed";
      entry->deinit();
      return false;
    }
    if(!plugin->init(plugin))
    {
      error = "init failed";
      plugin->destroy(plugin);
      plugin = nullptr;
      entry->deinit();
      return false;
    }
    play(stim, block, res);
    t_kind = 1;
    plugin->destroy(plugin);
    plugin = nullptr;
    entry->deinit();
    return error.empty();
  }

  template <typename T>
  const T* ext(const char* id) const
  {
    return static_cast<const T*>(plugin->get_extension(plugin, id));
  }

  void play(const stimulus& stim, int block, run_stats& res)
  {
    auto* aports = ext<clap_plugin_audio_ports_t>(CLAP_EXT_AUDIO_PORTS);
    auto* nports = ext<clap_plugin_note_ports_t>(CLAP_EXT_NOTE_PORTS);
    auto* state = ext<clap_plugin_state_t>(CLAP_EXT_STATE);
    params = ext<clap_plugin_params_t>(CLAP_EXT_PARAMS);
    timer_ext = ext<clap_plugin_timer_support_t>(CLAP_EXT_TIMER_SUPPORT);
    fd_ext = ext<clap_plugin_posix_fd_support_t>(CLAP_EXT_POSIX_FD_SUPPORT);

    std::vector<clap_audio_port_info_t> ains, aouts;
    if(aports)
      for(int dir = 0; dir < 2; dir++)
        for(uint32_t i = 0, n = aports->count(plugin, dir == 0); i < n; i++)
        {
          clap_audio_port_info_t info{};
          if(aports->get(plugin, i, dir == 0, &info))
            (dir == 0 ? ains : aouts).push_back(info);
        }
    std::vector<clap_note_port_info_t> nins;
    uint32_t note_outs = 0;
    if(nports)
    {
      for(uint32_t i = 0, n = nports->count(plugin, true); i < n; i++)
      {
        clap_note_port_info_t info{};
        if(nports->get(plugin, i, true, &info))
          nins.push_back(info);
      }
      note_outs = nports->count(plugin, false);
    }
    (void)note_outs;
    std::vector<clap_param_info_t> pinfo;
    std::set<clap_id> readonly;
    if(params)
      for(uint32_t i = 0, n = params->count(plugin); i < n; i++)
      {
        clap_param_info_t info{};
        if(params->get_info(plugin, i, &info))
        {
          pinfo.push_back(info);
          if(info.flags & CLAP_PARAM_IS_READONLY)
            readonly.insert(info.id);
        }
      }

    for(int i = 0; i < int(settle_ms / 10); i++)
    {
      main_duties();
      QThread::msleep(10);
    }

    clap_output_events_t discard{
        nullptr,
        [](const clap_output_events_t*, const clap_event_header_t*) { return true; }};
    auto flush = [&](event_list& ev) {
      ev.seal();
      auto in = ev.list();
      params->flush(plugin, &in, &discard);
    };
    auto get = [&](clap_id id) {
      double v = NAN;
      if(!params->get_value(plugin, id, &v))
        v = NAN;
      return v;
    };

    const int rate = stim.rate;
    if(!plugin->activate(plugin, rate, 1, block))
    {
      error = "activate failed";
      return;
    }
    active = true;
    t_kind = 2;
    if(!plugin->start_processing(plugin))
    {
      t_kind = 1;
      plugin->deactivate(plugin);
      active = false;
      error = "start_processing failed";
      return;
    }
    t_kind = 1;

    std::vector<std::vector<std::vector<float>>> in_data(ains.size()), out_data(aouts.size());
    std::vector<std::vector<float*>> in_ptrs(ains.size()), out_ptrs(aouts.size());
    std::vector<clap_audio_buffer_t> in_bufs(ains.size()), out_bufs(aouts.size());
    auto setup = [&](auto& ports, auto& data, auto& ptrs, auto& bufs) {
      for(std::size_t i = 0; i < ports.size(); i++)
      {
        data[i].assign(ports[i].channel_count, std::vector<float>(block, 0.f));
        for(auto& c : data[i])
          ptrs[i].push_back(c.data());
        bufs[i] = {};
        bufs[i].channel_count = ports[i].channel_count;
        bufs[i].data32 = ptrs[i].data();
      }
    };
    setup(ains, in_data, in_ptrs, in_bufs);
    setup(aouts, out_data, out_ptrs, out_bufs);

    out_collector collector{&res, &readonly};
    clap_output_events_t out_events{
        &collector,
        [](const clap_output_events_t* l, const clap_event_header_t* h) -> bool {
      auto& c = *static_cast<out_collector*>(l->ctx);
      if(h->space_id != CLAP_CORE_EVENT_SPACE_ID)
        return true;
      switch(h->type)
      {
        case CLAP_EVENT_PARAM_VALUE: {
          auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
          if(c.readonly->contains(e->param_id))
            c.res->controls.add(e->param_id, e->value);
          break;
        }
        case CLAP_EVENT_NOTE_ON:
        case CLAP_EVENT_NOTE_OFF:
        case CLAP_EVENT_MIDI:
        case CLAP_EVENT_MIDI2:
        case CLAP_EVENT_MIDI_SYSEX:
          c.res->midi_out++;
          break;
        default:
          break;
      }
      return true;
    }};

    clap_event_transport_t transport{};
    const int64_t total = int64_t(play_seconds * rate);
    res.audio.seconds = play_seconds;
    const auto t0 = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration main_time{};
    for(int64_t frame = 0; frame < total; frame += block)
    {
      for(std::size_t i = 0; i < in_data.size(); i++)
        for(std::size_t c = 0; c < in_data[i].size(); c++)
          for(int k = 0; k < block; k++)
            in_data[i][c][k] = i == 0 ? float(stim.audio(int(c), frame + k)) : 0.f;

      event_list ev;
      for(std::size_t port = 0; port < nins.size(); port++)
      {
        const auto pref = nins[port].preferred_dialect;
        const auto sup = nins[port].supported_dialects;
        for(auto& n : stim.notes(frame, block))
        {
          if((pref == CLAP_NOTE_DIALECT_CLAP || !(sup & (CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI2)))
             && (sup & CLAP_NOTE_DIALECT_CLAP))
          {
            clap_event_note_t e{};
            e.header = {sizeof(e), uint32_t(n.frame), CLAP_CORE_EVENT_SPACE_ID,
                        uint16_t(n.on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
            e.note_id = -1;
            e.port_index = int16_t(port);
            e.channel = 0;
            e.key = int16_t(n.pitch);
            e.velocity = n.on ? 100. / 127. : 0.;
            ev.push(e);
          }
          else if(pref == CLAP_NOTE_DIALECT_MIDI2 && (sup & CLAP_NOTE_DIALECT_MIDI2))
          {
            clap_event_midi2_t e{};
            e.header = {sizeof(e), uint32_t(n.frame), CLAP_CORE_EVENT_SPACE_ID,
                        CLAP_EVENT_MIDI2, 0};
            e.port_index = uint16_t(port);
            const uint64_t w
                = n.on ? cmidi2_ump_midi2_note_on(0, 0, n.pitch, 0, 100 << 9, 0)
                       : cmidi2_ump_midi2_note_off(0, 0, n.pitch, 0, 0, 0);
            e.data[0] = uint32_t(w >> 32);
            e.data[1] = uint32_t(w & 0xffffffff);
            ev.push(e);
          }
          else if(sup & CLAP_NOTE_DIALECT_MIDI)
          {
            clap_event_midi_t e{};
            e.header = {sizeof(e), uint32_t(n.frame), CLAP_CORE_EVENT_SPACE_ID,
                        CLAP_EVENT_MIDI, 0};
            e.port_index = uint16_t(port);
            e.data[0] = n.on ? 0x90 : 0x80;
            e.data[1] = uint8_t(n.pitch);
            e.data[2] = n.on ? 100 : 0;
            ev.push(e);
          }
        }
      }
      ev.seal();
      auto in_events = ev.list();

      const double beats = double(frame) / rate * 2.;
      transport.header
          = {sizeof(transport), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
      transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE
                        | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE
                        | CLAP_TRANSPORT_HAS_TIME_SIGNATURE | CLAP_TRANSPORT_IS_PLAYING;
      transport.song_pos_beats = clap_beattime(std::round(beats * CLAP_BEATTIME_FACTOR));
      transport.song_pos_seconds = clap_sectime(
          std::round(double(frame) / rate * CLAP_SECTIME_FACTOR));
      transport.tempo = 120.;
      transport.bar_start = clap_beattime(
          std::round(std::floor(beats / 4.) * 4. * CLAP_BEATTIME_FACTOR));
      transport.bar_number = int32_t(beats / 4.);
      transport.tsig_num = 4;
      transport.tsig_denom = 4;

      clap_process_t pr{};
      pr.steady_time = frame;
      pr.frames_count = uint32_t(block);
      pr.transport = &transport;
      pr.audio_inputs = in_bufs.data();
      pr.audio_outputs = out_bufs.data();
      pr.audio_inputs_count = uint32_t(in_bufs.size());
      pr.audio_outputs_count = uint32_t(out_bufs.size());
      pr.in_events = &in_events;
      pr.out_events = &out_events;
      t_kind = 2;
      plugin->process(plugin, &pr);
      t_kind = 1;
      res.blocks++;

      for(auto& port : out_data)
        for(auto& c : port)
          for(int k = 0; k < block; k++)
            res.audio.add(c[k], double(frame + k) / rate);
      if(!in_data.empty() && !in_data[0].empty() && !out_data.empty()
         && !out_data[0].empty())
      {
        res.audio.compared_input = true;
        for(int k = 0; k < block; k++)
          if(std::isfinite(out_data[0][0][k]))
            res.audio.max_diff_input = std::max(
                res.audio.max_diff_input,
                std::abs(double(out_data[0][0][k]) - in_data[0][0][k]));
      }
      const auto m0 = std::chrono::steady_clock::now();
      main_duties();
      main_time += std::chrono::steady_clock::now() - m0;
    }
    res.seconds = double(total) / rate;
    res.wall_seconds
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0 - main_time)
              .count();
    t_kind = 2;
    plugin->stop_processing(plugin);
    t_kind = 1;
    plugin->deactivate(plugin);
    active = false;

    // After the play, as score's side does them
    // Set the editable parameters, read them back, set them back
    if(params && params->flush && params->get_value)
    {
      std::map<clap_id, double> before;
      std::size_t n = 0;
      for(auto& p : pinfo)
      {
        if(!editable(p) || n++ >= max_edited_params)
          continue;
        before[p.id] = get(p.id);
        event_list ev;
        clap_event_param_value_t e{};
        e.header = {sizeof(e), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        e.param_id = p.id;
        e.cookie = p.cookie;
        e.note_id = e.port_index = e.channel = e.key = -1;
        e.value = edit_target(p);
        ev.push(e);
        flush(ev);
        edited[p.id] = get(p.id);
      }
      event_list ev;
      for(auto& p : pinfo)
        if(auto it = before.find(p.id); it != before.end() && std::isfinite(it->second))
        {
          clap_event_param_value_t e{};
          e.header = {sizeof(e), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
          e.param_id = p.id;
          e.cookie = p.cookie;
          e.note_id = e.port_index = e.channel = e.key = -1;
          e.value = it->second;
          ev.push(e);
        }
      if(!ev.storage.empty())
        flush(ev);
    }

    // A state saved and loaded again leaves the parameters as they were
    if(state && params && params->get_value)
    {
      std::vector<uint8_t> blob;
      clap_ostream_t os{
          &blob, [](const clap_ostream_t* s, const void* b, uint64_t n) -> int64_t {
        auto& v = *static_cast<std::vector<uint8_t>*>(s->ctx);
        v.insert(v.end(), (const uint8_t*)b, (const uint8_t*)b + n);
        return int64_t(n);
      }};
      std::map<clap_id, double> before;
      for(auto& p : pinfo)
        before[p.id] = get(p.id);
      if(state->save(plugin, &os))
      {
        struct rd
        {
          std::vector<uint8_t>* v;
          std::size_t pos;
        } r{&blob, 0};
        clap_istream_t is{&r, [](const clap_istream_t* s, void* b, uint64_t n) -> int64_t {
          auto& r = *static_cast<rd*>(s->ctx);
          const auto k = std::min<uint64_t>(n, r.v->size() - r.pos);
          std::memcpy(b, r.v->data() + r.pos, k);
          r.pos += k;
          return int64_t(k);
        }};
        if(state->load(plugin, &is))
        {
          state_changed = 0;
          for(auto& p : pinfo)
          {
            const double a = before[p.id], b = get(p.id);
            const double tol = 1e-5 * std::max(1., p.max_value - p.min_value);
            if(std::isfinite(a) && std::isfinite(b) && std::abs(a - b) > tol)
              state_changed++;
          }
        }
      }
    }

    main_duties();
  }
};

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    if(ms > 0)
      QThread::msleep(2);
  } while(t.elapsed() < ms);
}

//! score's side: the process in a document, its node run the way the graph
//! runs it, a block at a time, the main thread taking its turn in between
struct score_side
{
  const score::GUIApplicationContext& ctx;
  score::Document* doc{};
  Clap::Model* proc{};

  double params_value(const clap_param_info_t& p) const
  {
    auto& h = *proc->handle();
    double v = NAN;
    if(!h.ext_params || !h.ext_params->get_value
       || !h.ext_params->get_value(h.plugin, p.id, &v))
      return NAN;
    return v;
  }

  //! One play and the stop that ends it, as the transport does them.
  //! Returns what kept it from playing, if anything.
  std::string play(const stimulus& stim, double seconds, run_stats& res)
  {
    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(plug);
    if(!plug.baseScenario())
      return "no execution";
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    if(it == procs.end() || !it->second->node)
      return "no node";
    auto node = it->second->node;
    auto& st = *plug.contextData()->execState;
    const int block = st.bufferSize;
    const int rate = st.sampleRate;

    // The node's ports are the process's, in the same order
    std::vector<ossia::audio_port*> audio_ins, audio_outs;
    std::vector<ossia::midi_port*> midi_ins, midi_outs;
    std::vector<std::pair<ossia::value_port*, clap_id>> param_outs;
    {
      std::size_t ctl = 0;
      for(std::size_t i = 0; i < proc->outlets().size(); i++)
      {
        auto* o = node->root_outputs()[i];
        if(auto* a = o->target<ossia::audio_port>())
          audio_outs.push_back(a);
        else if(auto* m = o->target<ossia::midi_port>())
          midi_outs.push_back(m);
        else if(auto* v = o->target<ossia::value_port>())
          param_outs.push_back({v, proc->parameterOutputs()[ctl++].id});
      }
      for(std::size_t i = 0; i < proc->inlets().size(); i++)
      {
        auto* in = node->root_inputs()[i];
        if(auto* a = in->target<ossia::audio_port>())
          audio_ins.push_back(a);
        else if(auto* m = in->target<ossia::midi_port>())
          midi_ins.push_back(m);
      }
    }
    // What score's audio chain carries: stereo, or more if the port has more
    const std::size_t in_channels
        = audio_ins.empty() ? 0
                            : std::max<std::size_t>(2, proc->audioInputs()[0].channel_count);

    const bool trace = qEnvironmentVariableIsSet("SCORE_CLAP_SWEEP_TRACE");
    if(trace)
    {
      std::size_t k = 0;
      for(auto* inlet : proc->inlets())
        if(auto* ctl = qobject_cast<Process::ControlInlet*>(inlet))
        {
          const auto& p = proc->parameterInputs()[k++];
          std::fprintf(
              stderr, "TRACE control %s = %g (plug-in: %g)\n", p.name,
              ossia::convert<double>(ctl->value()), params_value(p));
        }
    }

    const int64_t total = int64_t(seconds * rate);
    const int64_t edit_period = std::max<int64_t>(1, (rate * 32 / 1000) / block);
    res.audio.seconds = seconds;
    const double flicks_per_frame = ossia::flicks_per_second<double> / rate;
    const auto t0 = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration main_time{};
    for(int64_t frame = 0; frame < total; frame += block)
    {
      for(auto* o : node->root_outputs())
        o->visit(ossia::clear_data{});
      if(!audio_ins.empty())
      {
        auto& a = *audio_ins[0];
        a.set_channels(in_channels);
        for(std::size_t c = 0; c < in_channels; c++)
        {
          a.channel(c).resize(block);
          for(int k = 0; k < block; k++)
            a.channel(c)[k] = stim.audio(int(c), frame + k);
        }
      }
      for(auto* m : midi_ins)
      {
        for(auto& n : stim.notes(frame, block))
        {
          const uint64_t w = n.on ? cmidi2_ump_midi2_note_on(0, 0, n.pitch, 0, 100 << 9, 0)
                                  : cmidi2_ump_midi2_note_off(0, 0, n.pitch, 0, 0, 0);
          libremidi::ump u{uint32_t(w >> 32), uint32_t(w & 0xffffffff)};
          u.timestamp = n.frame;
          m->messages.push_back(u);
        }
      }

      ossia::token_request tk{
          ossia::time_value{int64_t(frame * flicks_per_frame)},
          ossia::time_value{int64_t((frame + block) * flicks_per_frame)},
          ossia::time_value{int64_t(seconds * ossia::flicks_per_second<double>)},
          ossia::time_value{0},
          1.,
          ossia::time_signature{4, 4},
          120.};
      tk.start_sample = 0;
      tk.length_sample = block;
      ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
      plug.runAllCommands();
      node->run(tk, ossia::exec_state_facade{&st});
      ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
      res.blocks++;

      if(trace)
      {
        double pk = 0;
        for(auto* a : audio_outs)
          for(std::size_t c = 0; c < a->channels(); c++)
            for(double x : a->channel(c))
              pk = std::max(pk, std::abs(x));
        std::fprintf(stderr, "TRACE frame %lld peak %g\n", (long long)frame, pk);
      }
      for(auto* a : audio_outs)
        for(std::size_t c = 0; c < a->channels(); c++)
        {
          auto& ch = a->channel(c);
          for(std::size_t k = 0; k < ch.size() && k < std::size_t(block); k++)
            res.audio.add(ch[k], double(frame + k) / rate);
        }
      if(!audio_ins.empty() && !audio_outs.empty() && audio_outs[0]->channels() > 0)
      {
        res.audio.compared_input = true;
        auto& ch = audio_outs[0]->channel(0);
        for(std::size_t k = 0; k < ch.size() && k < std::size_t(block); k++)
          if(ossia::safe_isfinite(ch[k]))
            res.audio.max_diff_input = std::max(
                res.audio.max_diff_input,
                std::abs(ch[k] - double(float(stim.audio(0, frame + k)))));
      }
      for(auto* m : midi_outs)
        res.midi_out += m->messages.size();
      for(auto& [v, id] : param_outs)
        for(auto& tv : v->get_data())
          res.controls.add(id, ossia::convert<double>(tv.value));
      for(auto* in : node->root_inputs())
        in->visit(ossia::clear_data{});

      const auto m0 = std::chrono::steady_clock::now();
      QCoreApplication::processEvents();
      // What the execution's 32 ms timer does on the main thread, as often
      // as a real-time play would give it its turn
      if((frame / block) % edit_period == edit_period - 1)
        plug.contextData()->processEditCommands();
      main_time += std::chrono::steady_clock::now() - m0;
    }
    res.seconds = double(total) / rate;
    res.wall_seconds
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0 - main_time)
              .count();
    node.reset();

    // What the clock's stop and ExecutionController::reset_after_stop do: the
    // interval stops on the execution thread, which runs it before the
    // execution is cleared on the main one
    plug.baseScenario()->baseInterval().stop();
    score::test::run_exec(plug);
    plug.clear();
    spin(20);
    return {};
  }
};

std::string role_of(
    const Clap::Model& m, const std::vector<std::string>& features)
{
  auto has = [&](std::string_view f) {
    return std::find(features.begin(), features.end(), f) != features.end();
  };
  const bool ain = !m.audioInputs().empty();
  const bool aout = !m.audioOutputs().empty();
  const bool nin = !m.midiInputs().empty();
  const bool nout = !m.midiOutputs().empty();
  if(aout && nin && (has(CLAP_PLUGIN_FEATURE_INSTRUMENT) || !ain))
    return "instrument";
  if(ain && aout)
    return "effect";
  if(aout)
    return "generator";
  if(ain)
    return "analyzer";
  if(nin && nout)
    return "midi-effect";
  if(nout)
    return "midi-generator";
  if(!m.parameterOutputs().empty())
    return "control";
  return "other";
}

void probe(
    const score::GUIApplicationContext& ctx, results_file& out, const std::string& checker,
    const std::string& path, const std::string& id)
{
  const std::string uri = path + ":::" + id;
  json j;
  j.str("type", "result").str("uri", uri).str("path", path).str("id", id);

  // What the engine runs at, from its settings
  auto& audio = ctx.settings<Audio::Settings::Model>();
  const int rate = audio.getRate();
  const int block = audio.getBufferSize();
  j.num("rate", rate).num("block", block);
  stimulus stim{rate};

  // The reference host first: what it survives, score has to
  out.phase(uri, "reference");
  checker_set_target(checker, path);
  reference_host ref;
  run_stats ref_stats;
  const bool ref_ok = ref.run(checker, id, stim, block, ref_stats);
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  if(ref_ok)
    ref_stats.write(j, "ref_");
  else
    j.str("ref_skipped", ref.error);
  j.raw("ref_violations", checker_take(checker));
  if(ref.state_changed >= 0)
    j.num("ref_state_changed", ref.state_changed);

  out.phase(uri, "load");
  checker_set_target(checker, path);
  const auto t0 = std::chrono::steady_clock::now();
  score_side score{ctx};
  score.doc = score::test::new_document(ctx);
  score.proc = static_cast<Clap::Model*>(score::test::add_process(
      *score.doc, clap_uuid, QString::fromStdString(checker + ":::" + id)));
  j.num(
      "instantiate_ms",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
          .count());
  if(!score.proc || !score.proc->handle() || !score.proc->handle()->plugin)
  {
    j.raw("score_violations", checker_take(checker));
    out.write(j.str("status", "instantiate-failed").done());
    ctx.docManager.forceCloseDocument(ctx, *score.doc);
    return;
  }
  auto& m = *score.proc;
  auto& h = *m.handle();

  std::vector<std::string> features;
  if(h.desc)
  {
    j.str("name", h.desc->name ? h.desc->name : "");
    j.str("vendor", h.desc->vendor ? h.desc->vendor : "");
    if(h.desc->features)
      for(auto f = h.desc->features; *f; ++f)
        features.push_back(*f);
  }
  j.strings("features", features);
  const auto role = role_of(m, features);
  j.str("role", role);
  {
    std::ostringstream ports;
    auto channels = [](auto& v) {
      std::string s = "[";
      for(std::size_t i = 0; i < v.size(); i++)
        s += (i ? "," : "") + std::to_string(v[i].channel_count);
      return s + "]";
    };
    ports << "{\"audio_in\":" << channels(m.audioInputs())
          << ",\"audio_out\":" << channels(m.audioOutputs())
          << ",\"note_in\":" << m.midiInputs().size()
          << ",\"note_out\":" << m.midiOutputs().size()
          << ",\"param_in\":" << m.parameterInputs().size()
          << ",\"param_out\":" << m.parameterOutputs().size()
          << ",\"supports64\":" << (m.supports64() ? "true" : "false") << "}";
    j.raw("ports", ports.str());
  }

  // A process is created some time before it plays
  spin(settle_ms);

  out.phase(uri, "play");
  run_stats first;
  if(auto err = score.play(stim, play_seconds, first); !err.empty())
    j.str("play_error", err);
  first.write(j, "play_");

  out.phase(uri, "replay");
  run_stats second;
  if(auto err = score.play(stim, replay_seconds, second); !err.empty())
    j.str("replay_error", err);
  second.write(j, "replay_");

  // Each control set as the user would, not playing: the plug-in has to hold
  // what the reference host's flush gave it
  out.phase(uri, "edit");
  {
    std::vector<std::string> mismatch;
    std::size_t n = 0, inlet_idx = 0;
    std::vector<std::pair<Process::ControlInlet*, ossia::value>> restore;
    for(auto* inlet : m.inlets())
    {
      auto* ctl = qobject_cast<Process::ControlInlet*>(inlet);
      if(!ctl)
        continue;
      const auto& p = m.parameterInputs()[inlet_idx++];
      if(!editable(p) || n++ >= max_edited_params)
        continue;
      restore.push_back({ctl, ctl->value()});
      ctl->setValue(edit_target(p));
      QCoreApplication::processEvents();
      const double got = score.params_value(p);
      auto it = ref.edited.find(p.id);
      if(it == ref.edited.end() || !std::isfinite(it->second) || !std::isfinite(got))
        continue;
      const double tol = 1e-4 * std::max(1., p.max_value - p.min_value);
      if(std::abs(got - it->second) > tol)
        mismatch.push_back(
            std::string(p.name) + ": " + std::to_string(got) + " in score, "
            + std::to_string(it->second) + " in the reference host");
    }
    for(auto& [ctl, v] : restore)
      ctl->setValue(v);
    spin(250);
    j.strings("edit_mismatch", mismatch);
  }

  // A preset of the process's own state, loaded back
  out.phase(uri, "state");
  {
    std::map<clap_id, double> before;
    for(auto& p : m.parameterInputs())
      before[p.id] = score.params_value(p);
    // Through the base class: Clap::Model is not exported
    auto& pm = static_cast<Process::ProcessModel&>(m);
    const auto preset = pm.savePreset();
    pm.loadPreset(preset);
    spin(20);
    int changed = 0;
    for(auto& p : m.parameterInputs())
    {
      const double a = before[p.id], b = score.params_value(p);
      const double tol = 1e-5 * std::max(1., p.max_value - p.min_value);
      if(std::isfinite(a) && std::isfinite(b) && std::abs(a - b) > tol)
        changed++;
    }
    j.num("state_changed", changed);
  }

  out.phase(uri, "teardown");
  ctx.docManager.forceCloseDocument(ctx, *score.doc);
  spin(20);
  j.raw("score_violations", checker_take(checker));
  out.write(j.str("status", "ran").done());
}

void probe_live(
    const score::GUIApplicationContext& ctx, results_file& out, const std::string& checker,
    const std::string& path, const std::string& id)
{
  const std::string uri = path + ":::" + id;
  json j;
  j.str("type", "result").str("uri", uri).str("path", path).str("id", id);
  j.str("mode", "live");

  out.phase(uri, "load");
  checker_set_target(checker, path);
  score::test::live::options opt;
  opt.what = {
      UuidKey<Process::ProcessModel>::fromString(clap_uuid), {},
      QString::fromStdString(checker + ":::" + id)};
  opt.only = score::test::live::requested_phases();
  score::test::live::verdicts res;
  score::test::live::recorder rec;
  rec.phase = [&](std::string_view phase) { out.phase(uri, std::string{phase}.c_str()); };
  rec.result = [&](std::string_view phase, std::string_view verdict, std::string_view detail) {
    res.add(phase, verdict, detail);
  };
  score::test::live::run(ctx, opt, rec);
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  j.raw("live", res.json());
  j.raw("score_violations", checker_take(checker));
  out.write(j.str("status", "ran").done());
}

void list_plugins(const std::string& file, const QString& out_path)
{
  std::ofstream f{out_path.toStdString()};
  void* lib = dlopen(file.c_str(), RTLD_LAZY | RTLD_LOCAL);
  if(!lib)
    return;
  auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
  // clap/entry.h: on macOS init() takes the bundle, not the binary in it
  std::string init_path = file;
#if defined(__APPLE__)
  if(auto p = file.rfind(".clap/Contents/MacOS/"); p != std::string::npos)
    init_path = file.substr(0, p + 5);
#endif
  if(!entry || !entry->init(init_path.c_str()))
    return;
  if(auto* factory = static_cast<const clap_plugin_factory_t*>(
         entry->get_factory(CLAP_PLUGIN_FACTORY_ID)))
  {
    auto clean = [](const char* s) {
      std::string r = s ? s : "";
      for(auto& c : r)
        if(c == '\t' || c == '\n' || c == '\r')
          c = ' ';
      return r;
    };
    for(uint32_t i = 0, n = factory->get_plugin_count(factory); i < n; i++)
    {
      auto* d = factory->get_plugin_descriptor(factory, i);
      if(!d || !d->id)
        continue;
      std::string feats;
      if(d->features)
        for(auto ft = d->features; *ft; ++ft)
          feats += (feats.empty() ? "" : ";") + clean(*ft);
      f << file << '\t' << clean(d->id) << '\t' << clean(d->name) << '\t'
        << clean(d->vendor) << '\t' << clean(d->version) << '\t' << feats << '\n';
    }
  }
  f.flush();
  entry->deinit();
}
}

TEST_CASE("CLAP system sweep worker", "[clap][sweep]")
{
  install_crash_handler();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  const QString list = qEnvironmentVariable("SCORE_CLAP_SWEEP_LIST");
  if(!list.isEmpty())
  {
    list_plugins(qEnvironmentVariable("SCORE_CLAP_SWEEP_FILE").toStdString(), list);
    return;
  }

  const QString jobs = qEnvironmentVariable("SCORE_CLAP_SWEEP_JOBS");
  const QString results = qEnvironmentVariable("SCORE_CLAP_SWEEP_RESULTS");
  const std::string checker = qEnvironmentVariable("SCORE_CLAP_SWEEP_CHECKER").toStdString();
  if(jobs.isEmpty() || results.isEmpty() || checker.empty())
    return;

  QFile f{jobs};
  REQUIRE(f.open(QIODevice::ReadOnly));
  std::vector<std::pair<std::string, std::string>> todo;
  for(const auto& line : QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts))
  {
    const auto parts = line.split('\t');
    if(parts.size() >= 2)
      todo.emplace_back(parts[0].toStdString(), parts[1].toStdString());
  }

  // No scanner processes: the plug-ins are loaded one by one
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    results_file out{results};
    const bool live = score::test::live::requested();
    for(auto& [path, id] : todo)
    {
      if(live)
        probe_live(ctx, out, checker, path, id);
      else
        probe(ctx, out, checker, path, id);
    }
  });
}
