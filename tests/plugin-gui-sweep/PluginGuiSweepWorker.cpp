// The worker of the plug-in editor sweep (plugin_gui_sweep.py drives it):
// every installed plug-in with an editor (VST 2, VST3, CLAP, LV2, and the
// JSFX effects with a @gfx section) has its editor opened through score's own
// windows, the way the process's "show UI" button does it, on the X server of
// $DISPLAY (a private Xvfb: the driver never runs this on a real display).
//
// For each plug-in, in a document of its own:
//   open       the editor maps a window of a sane size, which renders
//   resize     a window resized from outside, as a window manager does, when
//              the plug-in says it can be
//   interact   the pointer drags and scrolls over the editor (xdotool) until
//              a control of the process changes: what changed, how many undo
//              steps it made, the gestures the plug-in reported
//   host       a control changed in score reaches the plug-in and its editor
//   close      closing removes the window; opening again works
//   play       the same while the transport plays
//   persist    a change made in the editor survives a save and a reload
//   close-doc  a document closed while the editor is open
//
// The worker measures and the driver judges: thresholds live in the driver.
//
//  SCORE_GUI_SWEEP_LIST=<file> SCORE_GUI_SWEEP_LIST_INPUT=<file>
//      the plug-ins of the files of the input (format \t path lines; lv2 is
//      one line with no path), one JSON object per line:
//      {"format", "key", "file", "name"}
//  SCORE_GUI_SWEEP_JOBS=<file> SCORE_GUI_SWEEP_RESULTS=<file>
//  SCORE_GUI_SWEEP_SHOTS=<dir>
//      probe the plug-ins of these lines (format \t key \t file), appending
//      one JSON object per line: a "phase" record before each step, so that a
//      crash or a hang can be told where it happened, then a "result".
//      Screenshots go to <dir>/<plug-in>/<step>.png.
//  SCORE_GUI_SWEEP_CLAP_GESTURES=1
//      CLAP: a control changed in score reaches the plug-in wrapped in a
//      gesture, as score's host did up to a04edef446: PARAM_GESTURE_BEGIN
//      before the first value, PARAM_GESTURE_END through flush() once no
//      value came for 200 ms. Done here, between score and the plug-in, so
//      that both behaviours run on the same build.
//  SCORE_GUI_SWEEP_STEPS=open,resize,...
//      only these steps (open is always done)
//  SCORE_GUI_SWEEP_TRACE=1
//      print each drag and scroll, and what it changed, on stderr
//  SCORE_GUI_SWEEP_HOLD=<ms>
//      keep each editor open that long once it is shown, to look at it by
//      hand (its window id is printed on stderr)
//  SCORE_SWEEP_LIVE=1
//      instead, add each plug-in to a playing document as a user does, and
//      open its editor right after the drop (score_test/LivePlugin.hpp): the
//      phases of SCORE_SWEEP_LIVE_PHASES, or those about the editor
//
// Neither set, the test case has nothing to do and passes: ctest runs the
// driver, not this.

#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/PluginWindow.hpp>

#include <Clap/EffectModel.hpp>
#include <Effect/EffectLayer.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <LV2/Window.hpp>
#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Loader.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/EditHandler.hpp>
#include <Vst3/EffectModel.hpp>
#include <YSFX/ProcessModel.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <QtGui/qguiapplication_platform.h>

#include <clap/all.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <xcb/xcb.h>
#if __has_include(<ysfx-s.h>)
#include <ysfx-s.h>
#else
#include <ysfx.h>
#endif

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <execinfo.h>
#include <unistd.h>

namespace
{
using clk = std::chrono::steady_clock;

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
    if(std::isfinite(v))
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
    json j;
    write(j.str("type", "phase").str("uri", uri).str("phase", phase).done());
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

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    usleep(2000);
  }
}

std::vector<QStringList> read_lines(const QString& path)
{
  std::vector<QStringList> r;
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return r;
  for(auto& line : QString::fromUtf8(f.readAll()).split('\n'))
    if(!line.trimmed().isEmpty())
      r.push_back(line.split('\t'));
  return r;
}

// ---------------------------------------------------------------------------
// The X server

xcb_connection_t* xconn()
{
  if(auto x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
    return x11->connection();
  return nullptr;
}

struct xinfo
{
  bool exists{};
  bool viewable{};
  int w{}, h{};
  //! Mapped windows inside it, at any depth
  int descendants{};
  //! The largest of them
  int child_w{}, child_h{};
};

void walk_children(xcb_connection_t* c, xcb_window_t w, xinfo& info, int depth)
{
  if(depth > 6)
    return;
  auto tr = xcb_query_tree_reply(c, xcb_query_tree(c, w), nullptr);
  if(!tr)
    return;
  auto* kids = xcb_query_tree_children(tr);
  const int n = xcb_query_tree_children_length(tr);
  for(int i = 0; i < n; i++)
  {
    auto ar = xcb_get_window_attributes_reply(
        c, xcb_get_window_attributes(c, kids[i]), nullptr);
    const bool viewable = ar && ar->map_state == XCB_MAP_STATE_VIEWABLE;
    free(ar);
    if(!viewable)
      continue;
    auto gr = xcb_get_geometry_reply(c, xcb_get_geometry(c, kids[i]), nullptr);
    if(gr)
    {
      info.descendants++;
      if(gr->width * gr->height > info.child_w * info.child_h)
      {
        info.child_w = gr->width;
        info.child_h = gr->height;
      }
      free(gr);
    }
    walk_children(c, kids[i], info, depth + 1);
  }
  free(tr);
}

xinfo query_window(WId wid)
{
  xinfo info;
  auto c = xconn();
  if(!c || !wid)
    return info;
  auto ar = xcb_get_window_attributes_reply(
      c, xcb_get_window_attributes(c, xcb_window_t(wid)), nullptr);
  if(!ar)
    return info;
  info.exists = true;
  info.viewable = ar->map_state == XCB_MAP_STATE_VIEWABLE;
  free(ar);
  if(auto gr = xcb_get_geometry_reply(c, xcb_get_geometry(c, xcb_window_t(wid)), nullptr))
  {
    info.w = gr->width;
    info.h = gr->height;
    free(gr);
  }
  walk_children(c, xcb_window_t(wid), info, 0);
  return info;
}

//! The mapped top-level windows of the screen
std::set<xcb_window_t> toplevels()
{
  std::set<xcb_window_t> r;
  auto c = xconn();
  if(!c)
    return r;
  auto screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
  auto tr = xcb_query_tree_reply(c, xcb_query_tree(c, screen->root), nullptr);
  if(!tr)
    return r;
  auto* kids = xcb_query_tree_children(tr);
  const int n = xcb_query_tree_children_length(tr);
  for(int i = 0; i < n; i++)
  {
    auto ar = xcb_get_window_attributes_reply(
        c, xcb_get_window_attributes(c, kids[i]), nullptr);
    if(ar && ar->map_state == XCB_MAP_STATE_VIEWABLE && !ar->override_redirect)
      r.insert(kids[i]);
    free(ar);
  }
  free(tr);
  return r;
}

void write_xinfo(json& j, std::string_view prefix, const xinfo& x)
{
  const auto k = [&](const char* s) { return std::string(prefix) + s; };
  j.boolean(k("exists"), x.exists);
  j.boolean(k("viewable"), x.viewable);
  j.num(k("w"), x.w).num(k("h"), x.h);
  j.num(k("descendants"), x.descendants);
  j.num(k("child_w"), x.child_w).num(k("child_h"), x.child_h);
}

//! What a screenshot shows, roughly
struct shot
{
  QImage img;
  double mean{}, stddev{};
  int colors{};
  double black{};
  //! Uniform: a text-only editor has two colours and is not blank
  bool blank() const noexcept { return img.isNull() || stddev < 2.0 || colors < 2; }
  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    j.num(k("w"), img.width()).num(k("h"), img.height());
    j.num(k("mean"), mean).num(k("stddev"), stddev);
    j.num(k("colors"), colors).num(k("black"), black);
    j.boolean(k("blank"), blank());
  }
};

shot grab(WId wid)
{
  shot s;
  if(!wid)
    return s;
  auto* screen = QGuiApplication::primaryScreen();
  if(!screen)
    return s;
  s.img = screen->grabWindow(wid).toImage().convertToFormat(QImage::Format_RGB32);
  if(s.img.isNull() || s.img.width() == 0)
    return s;
  double sum = 0, sumsq = 0;
  std::size_t n = 0, dark = 0;
  std::set<uint32_t> colors;
  for(int y = 0; y < s.img.height(); y++)
  {
    auto line = reinterpret_cast<const QRgb*>(s.img.constScanLine(y));
    for(int x = 0; x < s.img.width(); x++)
    {
      const QRgb p = line[x];
      const double l = 0.299 * qRed(p) + 0.587 * qGreen(p) + 0.114 * qBlue(p);
      sum += l;
      sumsq += l * l;
      n++;
      dark += l < 8;
      if(colors.size() < 4096)
        colors.insert(p & 0xffffff);
    }
  }
  s.mean = sum / n;
  s.stddev = std::sqrt(std::max(0., sumsq / n - s.mean * s.mean));
  s.colors = colors.size();
  s.black = double(dark) / n;
  return s;
}

//! Pixels that differ between two screenshots of the same window
int diff_pixels(const shot& a, const shot& b)
{
  if(a.img.isNull() || b.img.isNull())
    return -1;
  if(a.img.size() != b.img.size())
    return a.img.width() * a.img.height();
  int n = 0;
  for(int y = 0; y < a.img.height(); y++)
  {
    auto la = reinterpret_cast<const QRgb*>(a.img.constScanLine(y));
    auto lb = reinterpret_cast<const QRgb*>(b.img.constScanLine(y));
    for(int x = 0; x < a.img.width(); x++)
    {
      const QRgb p = la[x], q = lb[x];
      if(std::abs(qRed(p) - qRed(q)) + std::abs(qGreen(p) - qGreen(q))
             + std::abs(qBlue(p) - qBlue(q))
         > 24)
        n++;
    }
  }
  return n;
}

//! xdotool, with the event loop running meanwhile: the plug-ins whose editor
//! is driven by the host's timers or its connection to the X server need it
bool xdo(const QStringList& args)
{
  QProcess p;
  p.start(QStringLiteral("xdotool"), args);
  if(!p.waitForStarted(2000))
    return false;
  QElapsedTimer t;
  t.start();
  while(p.state() != QProcess::NotRunning && t.elapsed() < 5000)
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
    p.waitForFinished(5);
  }
  if(p.state() != QProcess::NotRunning)
  {
    p.kill();
    p.waitForFinished(500);
    return false;
  }
  return p.exitCode() == 0;
}

// ---------------------------------------------------------------------------
// CLAP: what goes between score and the plug-in, seen from the plug-in side.
// The plug-in's clap_plugin_t is patched so that score reaches its params
// extension and its process() through the functions below, which count the
// gestures and values the plug-in sends, and, for SCORE_GUI_SWEEP_CLAP_GESTURES,
// add the gestures score's host sent up to a04edef446.

namespace clap_spy
{
struct counters
{
  std::atomic<int> flushes{};
  std::atomic<int> host_values{};
  std::atomic<int> injected_begin{}, injected_end{};
  std::atomic<int> main_begin{}, main_end{}, main_values{};
  std::atomic<int> audio_begin{}, audio_end{}, audio_values{};
  std::atomic<int> processes{};
  void reset()
  {
    for(auto* a :
        {&flushes, &host_values, &injected_begin, &injected_end, &main_begin, &main_end,
         &main_values, &audio_begin, &audio_end, &audio_values, &processes})
      a->store(0);
  }
  std::string json_str() const
  {
    json j;
    j.num("flushes", flushes).num("host_values", host_values);
    j.num("injected_begin", injected_begin).num("injected_end", injected_end);
    j.num("main_begin", main_begin).num("main_end", main_end);
    j.num("main_values", main_values);
    j.num("audio_begin", audio_begin).num("audio_end", audio_end);
    j.num("audio_values", audio_values).num("processes", processes);
    return j.done();
  }
};

struct gesture
{
  bool active{};
  clk::time_point last{};
};

struct state
{
  const clap_plugin_t* plugin{};
  const void*(CLAP_ABI* orig_get_extension)(const clap_plugin_t*, const char*){};
  clap_process_status(CLAP_ABI* orig_process)(const clap_plugin_t*, const clap_process_t*){};
  const clap_plugin_params_t* orig_params{};
  clap_plugin_params_t params{};
  bool inject{};
  //! Its process is gone: no more gesture ends to send
  std::atomic_bool dead{};
  std::thread::id main_thread{std::this_thread::get_id()};
  std::map<clap_id, gesture> gestures;
  counters c;
};

std::map<const clap_plugin_t*, state*> g_states;

state* find(const clap_plugin_t* p)
{
  auto it = g_states.find(p);
  return it != g_states.end() ? it->second : nullptr;
}

struct out_wrap
{
  const clap_output_events_t* orig{};
  state* s{};
  bool audio{};
};

bool CLAP_ABI wrapped_try_push(const clap_output_events_t* l, const clap_event_header_t* h)
{
  auto& w = *static_cast<out_wrap*>(l->ctx);
  if(h && h->space_id == CLAP_CORE_EVENT_SPACE_ID)
  {
    auto& c = w.s->c;
    switch(h->type)
    {
      case CLAP_EVENT_PARAM_GESTURE_BEGIN:
        (w.audio ? c.audio_begin : c.main_begin)++;
        break;
      case CLAP_EVENT_PARAM_GESTURE_END:
        (w.audio ? c.audio_end : c.main_end)++;
        break;
      case CLAP_EVENT_PARAM_VALUE:
        (w.audio ? c.audio_values : c.main_values)++;
        break;
      default:
        break;
    }
  }
  return w.orig ? w.orig->try_push(w.orig, h) : false;
}

struct in_list
{
  std::vector<const clap_event_header_t*> events;
};

uint32_t CLAP_ABI in_size(const clap_input_events_t* l)
{
  return static_cast<in_list*>(l->ctx)->events.size();
}
const clap_event_header_t* CLAP_ABI in_get(const clap_input_events_t* l, uint32_t i)
{
  auto& e = static_cast<in_list*>(l->ctx)->events;
  return i < e.size() ? e[i] : nullptr;
}

clap_event_param_gesture_t make_gesture(clap_id id, uint32_t time, uint16_t type)
{
  clap_event_param_gesture_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.time = time;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = type;
  ev.header.flags = type == CLAP_EVENT_PARAM_GESTURE_BEGIN ? CLAP_EVENT_IS_LIVE : 0;
  ev.param_id = id;
  return ev;
}

void CLAP_ABI
flush(const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out)
{
  auto* s = find(p);
  if(!s)
    return;
  s->c.flushes++;
  in_list list;
  std::vector<clap_event_param_gesture_t> added;
  const uint32_t n = in ? in->size(in) : 0;
  added.reserve(n);
  for(uint32_t i = 0; i < n; i++)
  {
    auto* h = in->get(in, i);
    if(!h)
      continue;
    if(h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE)
    {
      s->c.host_values++;
      // Only where score's host sent them: a control changed on the main
      // thread while not playing, not the values the executor gives a
      // plug-in it activates
      if(s->inject && std::this_thread::get_id() == s->main_thread)
      {
        auto* pv = reinterpret_cast<const clap_event_param_value_t*>(h);
        auto& g = s->gestures[pv->param_id];
        if(!g.active)
        {
          added.push_back(make_gesture(pv->param_id, h->time, CLAP_EVENT_PARAM_GESTURE_BEGIN));
          list.events.push_back(&added.back().header);
          g.active = true;
          s->c.injected_begin++;
        }
        g.last = clk::now();
      }
    }
    list.events.push_back(h);
  }
  clap_input_events_t win{&list, in_size, in_get};
  out_wrap ow{out, s, false};
  clap_output_events_t wout{&ow, wrapped_try_push};
  s->orig_params->flush(p, &win, &wout);
}

//! The gesture ends score's host sent 200 ms after the last value, through
//! flush() with an output list that takes nothing
void tick()
{
  const auto now = clk::now();
  for(auto& [p, s] : g_states)
  {
    if(s->dead || !s->inject || !s->orig_params || !s->orig_params->flush)
      continue;
    for(auto& [id, g] : s->gestures)
    {
      if(!g.active || now - g.last < std::chrono::milliseconds(200))
        continue;
      g.active = false;
      auto ev = make_gesture(id, 0, CLAP_EVENT_PARAM_GESTURE_END);
      in_list list;
      list.events.push_back(&ev.header);
      clap_input_events_t win{&list, in_size, in_get};
      clap_output_events_t wout{
          nullptr, [](const clap_output_events_t*, const clap_event_header_t*) {
        return false;
      }};
      s->c.injected_end++;
      s->orig_params->flush(p, &win, &wout);
    }
  }
}

clap_process_status CLAP_ABI process(const clap_plugin_t* p, const clap_process_t* proc)
{
  auto* s = find(p);
  s->c.processes++;
  clap_process_t copy = *proc;
  out_wrap ow{proc->out_events, s, true};
  clap_output_events_t wout{&ow, wrapped_try_push};
  copy.out_events = &wout;
  return s->orig_process(p, &copy);
}

const void* CLAP_ABI get_extension(const clap_plugin_t* p, const char* id)
{
  auto* s = find(p);
  if(!s)
    return nullptr;
  if(std::strcmp(id, CLAP_EXT_PARAMS) == 0 && s->orig_params)
    return &s->params;
  return s->orig_get_extension(p, id);
}

//! Whether the memory at p can be written to: a plug-in may keep its
//! clap_plugin_t in read-only data
bool writable(const void* p)
{
  std::ifstream maps{"/proc/self/maps"};
  std::string line;
  const auto a = reinterpret_cast<uintptr_t>(p);
  while(std::getline(maps, line))
  {
    uintptr_t lo{}, hi{};
    char perms[8]{};
    if(std::sscanf(line.c_str(), "%lx-%lx %7s", &lo, &hi, perms) == 3 && a >= lo
       && a + sizeof(clap_plugin_t) <= hi)
      return perms[1] == 'w';
  }
  return false;
}

state* attach(const clap_plugin_t* plugin, bool inject)
{
  if(!plugin)
    return nullptr;
  // A plug-in made where a destroyed one was gets a state of its own
  if(auto s = find(plugin); s && !s->dead)
    return s;
  if(!writable(plugin))
    return nullptr;
  auto* s = new state;
  s->plugin = plugin;
  s->inject = inject;
  s->orig_get_extension = plugin->get_extension;
  s->orig_process = plugin->process;
  s->orig_params = static_cast<const clap_plugin_params_t*>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  if(s->orig_params)
  {
    s->params = *s->orig_params;
    if(s->orig_params->flush)
      s->params.flush = flush;
  }
  g_states[plugin] = s;
  auto* w = const_cast<clap_plugin_t*>(plugin);
  w->get_extension = get_extension;
  w->process = process;
  return s;
}
}

// ---------------------------------------------------------------------------
// VST3: score's IComponentHandler, seen through one that counts and forwards

struct vst3_handler_spy final : public Steinberg::Vst::IComponentHandler
{
  Steinberg::Vst::IComponentHandler* orig{};
  std::atomic<int> begin{}, perform{}, end{}, restart{};
  std::atomic<int> nonmain{};
  std::thread::id main{std::this_thread::get_id()};

  void count_thread()
  {
    if(std::this_thread::get_id() != main)
      nonmain++;
  }
  Steinberg::tresult PLUGIN_API
  queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    return orig->queryInterface(iid, obj);
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }
  Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID id) override
  {
    count_thread();
    begin++;
    return orig->beginEdit(id);
  }
  Steinberg::tresult PLUGIN_API
  performEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue v) override
  {
    count_thread();
    perform++;
    return orig->performEdit(id, v);
  }
  Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID id) override
  {
    count_thread();
    end++;
    return orig->endEdit(id);
  }
  Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override
  {
    restart++;
    return orig->restartComponent(flags);
  }
  void reset()
  {
    begin = 0;
    perform = 0;
    end = 0;
    restart = 0;
    nonmain = 0;
  }
  std::string json_str() const
  {
    json j;
    j.num("begin", begin).num("perform", perform).num("end", end);
    j.num("restart", restart).num("not_main_thread", nonmain);
    return j.done();
  }
};

// ---------------------------------------------------------------------------
// One process, whatever its format

struct target
{
  std::string format;
  Process::ProcessModel* proc{};
  clap_spy::state* clap{};
  vst3_handler_spy* vst3{};

  bool is(const char* f) const noexcept { return format == f; }

  //! The value the plug-in itself has for the parameter of a control, NaN when
  //! it cannot be asked
  double plugin_value(Process::ControlInlet* c) const
  {
    if(is("clap"))
    {
      auto& m = *static_cast<Clap::Model*>(proc);
      auto& h = *m.handle();
      std::size_t k = 0;
      for(auto* in : m.inlets())
        if(auto* ctl = qobject_cast<Process::ControlInlet*>(in))
        {
          if(ctl == c)
          {
            if(k >= m.parameterInputs().size())
              return NAN;
            auto* params = static_cast<const clap_plugin_params_t*>(
                h.plugin->get_extension(h.plugin, CLAP_EXT_PARAMS));
            double v{};
            if(params && params->get_value
               && params->get_value(h.plugin, m.parameterInputs()[k].id, &v))
              return v;
            return NAN;
          }
          k++;
        }
      return NAN;
    }
    if(is("vst3"))
    {
      auto& m = *static_cast<vst3::Model*>(proc);
      if(!m.fx.controller)
        return NAN;
      return m.fx.controller->getParamNormalized(static_cast<vst3::ControlInlet*>(c)->fxNum);
    }
    if(is("vst"))
    {
      auto& m = *static_cast<vst::Model*>(proc);
      if(!m.fx || !m.fx->fx)
        return NAN;
      auto* vc = qobject_cast<vst::ControlInlet*>(c);
      if(!vc)
        return NAN;
      return m.fx->fx->getParameter(m.fx->fx, vc->fxNum);
    }
    if(is("jsfx"))
    {
      auto& m = *static_cast<YSFX::ProcessModel*>(proc);
      const int i = c->id().val() - 4;
      if(!m.fx || i < 0 || i >= int(ysfx_max_sliders))
        return NAN;
      return ysfx_slider_get_value(m.fx.get(), i);
    }
    return NAN;
  }

  //! Whether the plug-in says its editor can be resized; -1 when it cannot say
  int resizable() const
  {
    if(is("clap"))
    {
      auto& h = *static_cast<Clap::Model*>(proc)->handle();
      auto* gui = static_cast<const clap_plugin_gui_t*>(
          h.plugin->get_extension(h.plugin, CLAP_EXT_GUI));
      return gui && gui->can_resize ? int(gui->can_resize(h.plugin)) : -1;
    }
    if(is("vst3"))
    {
      auto& m = *static_cast<vst3::Model*>(proc);
      return m.fx.view ? int(m.fx.view->canResize() == Steinberg::kResultTrue) : -1;
    }
    if(is("lv2"))
    {
      auto& m = *static_cast<LV2::Model*>(proc);
      auto& p = score::GUIAppContext().applicationPlugin<LV2::ApplicationPlugin>();
      if(!m.effectContext.ui)
        return -1;
      // What LV2::Window::is_resizable reads
      auto* world = p.lilv.me;
      auto* opt = lilv_new_uri(world, "http://lv2plug.in/ns/lv2core#optionalFeature");
      auto* fixed = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#fixedSize");
      auto* nores = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#noUserResize");
      auto* ui = lilv_ui_get_uri(m.effectContext.ui);
      bool resizable = true;
      for(auto* f : {fixed, nores})
        if(auto* n = lilv_world_find_nodes(world, ui, opt, f))
        {
          resizable = false;
          lilv_nodes_free(n);
        }
      lilv_node_free(opt);
      lilv_node_free(fixed);
      lilv_node_free(nores);
      return resizable;
    }
    if(is("jsfx"))
      return 1;
    return 0;
  }

  //! The size the plug-in says its editor has
  QSize plugin_size() const
  {
    if(is("clap"))
    {
      auto& h = *static_cast<Clap::Model*>(proc)->handle();
      auto* gui = static_cast<const clap_plugin_gui_t*>(
          h.plugin->get_extension(h.plugin, CLAP_EXT_GUI));
      uint32_t w{}, hh{};
      if(gui && gui->get_size && static_cast<Clap::Model*>(proc)->window
         && gui->get_size(h.plugin, &w, &hh))
        return QSize(w, hh);
      return {};
    }
    if(is("vst3"))
    {
      auto& m = *static_cast<vst3::Model*>(proc);
      Steinberg::ViewRect r;
      if(m.fx.view && m.fx.view->getSize(&r) == Steinberg::kResultOk)
        return QSize(r.getWidth(), r.getHeight());
      return {};
    }
    if(is("vst"))
    {
      auto& m = *static_cast<vst::Model*>(proc);
      if(!m.fx || !m.fx->fx || !proc->externalUI)
        return {};
      ERect* r{};
      m.fx->fx->dispatcher(m.fx->fx, effEditGetRect, 0, 0, &r, 0.f);
      if(!r)
        return {};
      return QSize(r->right - r->left, r->bottom - r->top);
    }
    return {};
  }

  std::string gestures() const
  {
    if(clap)
      return clap->c.json_str();
    if(vst3)
      return vst3->json_str();
    return "null";
  }
  void reset_gestures()
  {
    if(clap)
      clap->c.reset();
    if(vst3)
      vst3->reset();
  }
};

std::vector<Process::ControlInlet*> controls_of(Process::ProcessModel& p)
{
  std::vector<Process::ControlInlet*> r;
  for(auto* in : p.inlets())
    if(auto* c = qobject_cast<Process::ControlInlet*>(in))
      r.push_back(c);
  return r;
}

double as_double(const ossia::value& v)
{
  return v.valid() ? ossia::convert<double>(v) : NAN;
}

std::pair<double, double> range_of(Process::ControlInlet& c)
{
  const auto& d = c.domain().get();
  double lo = as_double(ossia::get_min(d));
  double hi = as_double(ossia::get_max(d));
  if(!std::isfinite(lo) || !std::isfinite(hi) || hi <= lo)
    return {0., 1.};
  return {lo, hi};
}

//! The changes of the controls of a process, since it was attached
struct watch
{
  std::map<Process::ControlInlet*, int> changes, exec_changes;
  std::map<Process::ControlInlet*, double> first;
  std::vector<QMetaObject::Connection> cons;
  std::set<Process::ControlInlet*> known;

  void attach(Process::ProcessModel& p)
  {
    for(auto* c : controls_of(p))
    {
      if(known.contains(c))
        continue;
      known.insert(c);
      first[c] = as_double(c->value());
      cons.push_back(QObject::connect(
          c, &Process::ControlInlet::valueChanged, c, [this, c] { changes[c]++; }));
      cons.push_back(QObject::connect(
          c, &Process::ControlInlet::executionValueChanged, c,
          [this, c] { exec_changes[c]++; }));
    }
  }
  void clear()
  {
    changes.clear();
    exec_changes.clear();
  }
  ~watch()
  {
    for(auto& c : cons)
      QObject::disconnect(c);
  }
  int total() const
  {
    int n = 0;
    for(auto& [_, v] : changes)
      n += v;
    return n;
  }
  int exec_total() const
  {
    int n = 0;
    for(auto& [_, v] : exec_changes)
      n += v;
    return n;
  }
};

std::string safe_name(std::string s)
{
  for(auto& c : s)
    if(!std::isalnum((unsigned char)c) && c != '-' && c != '_' && c != '.')
      c = '_';
  if(s.size() > 120)
    s = s.substr(s.size() - 120);
  return s;
}

//! Closes the menus and dialogs a plug-in's editor opens on a click: a menu's
//! exec() would block the worker until someone picks something
struct popup_closer
{
  QTimer timer;
  int closed{};
  popup_closer()
  {
    timer.setInterval(300);
    QObject::connect(&timer, &QTimer::timeout, [this] {
      if(auto* w = QApplication::activePopupWidget())
      {
        w->close();
        closed++;
      }
      if(auto* w = QApplication::activeModalWidget())
      {
        if(auto* d = qobject_cast<QDialog*>(w); d && !dynamic_cast<score::PluginWindow*>(d))
        {
          d->reject();
          closed++;
        }
      }
    });
    timer.start();
  }
};

// ---------------------------------------------------------------------------

struct probe_context
{
  const score::GUIApplicationContext& ctx;
  results_file& out;
  QString shots_root;
  std::set<std::string> steps;
  bool inject_gestures{};

  bool wants(const char* step) const { return steps.empty() || steps.contains(step); }
};

struct job
{
  std::string format, key, file;
  std::string uri() const { return format + "|" + key; }
};

//! Where the drags and scrolls go: the middle of the window, then the
//! places of the editor with the most contrast, where knobs, handles and
//! buttons are drawn, as fractions of the window
std::vector<std::pair<double, double>> salient_points(const shot& s, std::size_t count)
{
  std::vector<std::pair<double, double>> r{{0.5, 0.5}};
  const QImage& img = s.img;
  if(img.isNull() || img.width() < 16 || img.height() < 16)
    return r;
  constexpr int cols = 12, rows = 10;
  struct cell
  {
    double score;
    int cx, cy;
  };
  std::vector<cell> cells;
  for(int cy = 0; cy < rows; cy++)
    for(int cx = 0; cx < cols; cx++)
    {
      const int x0 = cx * img.width() / cols, x1 = (cx + 1) * img.width() / cols;
      const int y0 = cy * img.height() / rows, y1 = (cy + 1) * img.height() / rows;
      double sum = 0, sumsq = 0;
      int n = 0;
      for(int y = y0; y < y1; y += 2)
      {
        auto line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for(int x = x0; x < x1; x += 2)
        {
          const double l = qGray(line[x]);
          sum += l;
          sumsq += l * l;
          n++;
        }
      }
      if(n > 0)
      {
        const double m = sum / n;
        cells.push_back({sumsq / n - m * m, cx, cy});
      }
    }
  std::sort(cells.begin(), cells.end(), [](auto& a, auto& b) { return a.score > b.score; });
  std::vector<std::pair<int, int>> taken;
  for(auto& c : cells)
  {
    if(r.size() >= count || c.score < 25)
      break;
    // One point per neighbourhood: the same knob is not tried twice
    if(std::any_of(taken.begin(), taken.end(), [&](auto& t) {
         return std::abs(t.first - c.cx) <= 1 && std::abs(t.second - c.cy) <= 1;
       }))
      continue;
    taken.push_back({c.cx, c.cy});
    r.push_back({(c.cx + 0.5) / cols, (c.cy + 0.5) / rows});
  }
  return r;
}

struct interaction
{
  bool changed{};
  int point{-1};
  std::string kind;
  int value_changes{}, exec_changes{};
  int undo_steps{};
  std::vector<std::string> commands;
  std::vector<Process::ControlInlet*> controls;
  int new_controls{};
  int gui_diff{};
  std::string gestures{"null"};
  int tried{};
};

void write_interaction(json& j, std::string_view prefix, const interaction& r)
{
  const auto k = [&](const char* s) { return std::string(prefix) + s; };
  j.boolean(k("changed"), r.changed);
  j.num(k("tried"), r.tried);
  j.num(k("point"), r.point);
  j.str(k("kind"), r.kind);
  j.num(k("value_changes"), r.value_changes);
  j.num(k("exec_changes"), r.exec_changes);
  j.num(k("undo_steps"), r.undo_steps);
  j.strings(k("commands"), r.commands);
  std::vector<std::string> names;
  for(auto* c : r.controls)
    names.push_back(c->name().toStdString());
  j.strings(k("controls"), names);
  j.num(k("new_controls"), r.new_controls);
  j.num(k("gui_diff"), r.gui_diff);
  j.raw(k("gestures"), r.gestures);
}

struct editor
{
  QPointer<QWidget> win;
  WId wid{};
  std::set<xcb_window_t> floating;
};

class prober
{
public:
  probe_context& pc;
  job jb;
  std::string uri;
  QString shots;
  score::Document* doc{};
  target t;
  json j;
  std::vector<std::string> notes;
  //! Where the editor is driven, found on its first interaction
  std::vector<std::pair<double, double>> points;

  prober(probe_context& pc, job jb)
      : pc{pc}
      , jb{std::move(jb)}
      , uri{this->jb.uri()}
  {
    shots = pc.shots_root + "/" + QString::fromStdString(safe_name(this->jb.format + "-" + this->jb.key));
    QDir{}.mkpath(shots);
    j.str("type", "result").str("uri", uri).str("format", this->jb.format);
    j.str("key", this->jb.key).str("file", this->jb.file);
    j.str("shots", shots.toStdString());
    j.boolean("clap_gestures_sent", pc.inject_gestures);
  }

  void phase(const char* p) { pc.out.phase(uri, p); }

  void save(const shot& s, const char* name)
  {
    if(!s.img.isNull())
      s.img.save(shots + "/" + name + ".png");
  }

  int undo_index() { return doc->commandStack().currentIndex(); }

  std::vector<std::string> commands_since(int idx)
  {
    std::vector<std::string> r;
    auto& st = doc->commandStack();
    for(int i = std::max(0, idx); i < st.currentIndex() && i < st.size(); i++)
      if(auto* c = st.command(i))
        r.push_back(c->description().toStdString());
    return r;
  }

  //! What the library drags for the plug-in, once score knows it as a scan
  //! would have listed it; empty on success, else the status
  std::string prepare(
      std::unique_ptr<vst::Module>& vst_module, UuidKey<Process::ProcessModel>& key,
      QString& data)
  {
    auto& ctx = pc.ctx;
    if(jb.format == "clap")
    {
      key = Metadata<ConcreteKey_k, Clap::Model>::get();
      data = QString::fromStdString(jb.key);
    }
    else if(jb.format == "vst3")
    {
      auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
      auto mdl = plug.getModule(jb.file);
      if(!mdl)
        return "not-found";
      vst3::AvailablePlugin p;
      p.path = QString::fromStdString(jb.file);
      p.name = QString::fromStdString(mdl->getName());
      for(const auto& cls : mdl->getFactory().classInfos())
        if(cls.category() == kVstAudioEffectClass)
          p.classInfo.push_back(cls);
      p.isValid = !p.classInfo.empty();
      plug.vst_infos = {p};
      key = Metadata<ConcreteKey_k, vst3::Model>::get();
      data = QString::fromStdString(jb.key);
    }
    else if(jb.format == "vst")
    {
      try
      {
        vst_module = std::make_unique<vst::Module>(jb.file);
      }
      catch(...)
      {
        return "not-a-plugin";
      }
      auto main = vst_module->getMain();
      if(!main)
        return "not-a-plugin";
      AEffect* fx = main([](AEffect*, int32_t op, int32_t, intptr_t, void*, float) -> intptr_t {
        return op == audioMasterVersion ? 2400 : 0;
      });
      if(!fx || fx->magic != kEffectMagic)
        return "not-a-plugin";
      const auto uid = fx->uniqueID;
      fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
      auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
      vst::VSTInfo info;
      info.path = QString::fromStdString(jb.file);
      info.prettyName = QFileInfo(info.path).completeBaseName();
      info.uniqueID = uid;
      info.isValid = true;
      app.vst_infos = {info};
      app.vst_modules.erase(uid);
      app.vst_modules.insert({uid, nullptr});
      key = Metadata<ConcreteKey_k, vst::Model>::get();
      data = QString::number(uid);
    }
    else if(jb.format == "lv2")
    {
      key = Metadata<ConcreteKey_k, LV2::Model>::get();
      data = QString::fromStdString(jb.key);
    }
    else if(jb.format == "jsfx")
    {
      key = Metadata<ConcreteKey_k, YSFX::ProcessModel>::get();
      data = QString::fromStdString(jb.key);
    }
    else
      return "unknown-format";
    return {};
  }

  //! Puts the plug-in in a new document; empty on success, else the status
  std::string create(std::unique_ptr<vst::Module>& vst_module)
  {
    UuidKey<Process::ProcessModel> key;
    QString data;
    if(auto err = prepare(vst_module, key, data); !err.empty())
      return err;
    doc = score::test::new_document(pc.ctx);
    if(!doc)
      return "no-document";

    auto& interval = score::test::base_interval(*doc);
    auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
        interval, key, data, QPointF{}};
    CommandDispatcher<>{doc->context().commandStack}.submit(cmd);
    auto it = interval.processes.find(cmd->processId());
    if(it == interval.processes.end())
      return "instantiate-failed";
    t = target{jb.format, &*it};
    if(!loaded(*t.proc))
      return "instantiate-failed";
    instrument(t);
    spin(200);
    return {};
  }

  bool loaded(Process::ProcessModel& p)
  {
    if(jb.format == "clap")
    {
      auto& m = static_cast<Clap::Model&>(p);
      return m.handle() && m.handle()->plugin;
    }
    if(jb.format == "vst3")
      return bool(static_cast<vst3::Model&>(p).fx);
    if(jb.format == "vst")
    {
      auto& m = static_cast<vst::Model&>(p);
      return m.fx && m.fx->fx;
    }
    if(jb.format == "lv2")
    {
      auto& m = static_cast<LV2::Model&>(p);
      return m.plugin && m.effectContext.instance;
    }
    if(jb.format == "jsfx")
      return bool(static_cast<YSFX::ProcessModel&>(p).fx);
    return false;
  }

  void instrument(target& tg)
  {
    if(tg.is("clap"))
    {
      auto* plugin = static_cast<Clap::Model*>(tg.proc)->handle()->plugin;
      tg.clap = clap_spy::attach(plugin, pc.inject_gestures);
      // The plug-in goes with its process: nothing may reach it afterwards
      QObject::connect(tg.proc, &QObject::destroyed, [plugin] {
        if(auto* s = clap_spy::find(plugin))
          s->dead = true;
      });
      if(!tg.clap)
        notes.push_back("clap_plugin_t is read-only: gestures not counted");
    }
    else if(tg.is("vst3"))
    {
      auto& m = *static_cast<vst3::Model*>(tg.proc);
      if(m.fx.controller && m.fx.componentHandler)
      {
        tg.vst3 = new vst3_handler_spy;
        tg.vst3->orig
            = static_cast<Steinberg::Vst::IComponentHandler*>(m.fx.componentHandler);
        m.fx.controller->setComponentHandler(tg.vst3);
      }
    }
  }

  bool has_editor()
  {
    auto& fact = pc.ctx.interfaces<Process::LayerFactoryList>();
    auto* f = fact.findDefaultFactory(*t.proc);
    return f && f->hasExternalUI(*t.proc, doc->context());
  }

  editor open(target& tg, score::Document& d)
  {
    editor e;
    const auto before = toplevels();
    Process::setupExternalUI(*tg.proc, d.context(), true);
    spin(1500);
    e.win = tg.proc->externalUI;
    if(e.win)
      e.wid = e.win->winId();
    for(auto w : toplevels())
      if(!before.contains(w) && w != xcb_window_t(e.wid))
        e.floating.insert(w);
    return e;
  }

  void close(target& tg, score::Document& d)
  {
    Process::setupExternalUI(*tg.proc, d.context(), false);
    spin(400);
  }

  //! A screenshot of a rendered editor: plug-ins that draw from a timer of
  //! their own take a while for the first frame
  shot settled_shot(const editor& e, bool& late)
  {
    late = false;
    const WId w = e.floating.empty() ? e.wid : WId(*e.floating.begin());
    shot s = grab(w);
    if(s.blank())
    {
      spin(2000);
      s = grab(w);
      late = !s.blank();
    }
    return s;
  }

  WId interaction_window(const editor& e) const
  {
    return e.floating.empty() ? e.wid : WId(*e.floating.begin());
  }

  //! Drags then scrolls over the points until a control of the process
  //! changes
  //! up: drags up and scrolls up, else down: a control moved to its end by
  //! one interaction is moved back by the next
  interaction interact(target& tg, const editor& e, int only_point = -1, bool up = true)
  {
    interaction r;
    const WId wid = interaction_window(e);
    const auto xi = query_window(wid);
    if(!xi.viewable || xi.w < 8 || xi.h < 8)
      return r;

    watch w;
    w.attach(*tg.proc);
    // What changes by itself is not the editor's doing
    spin(600);
    std::set<Process::ControlInlet*> idle;
    for(auto& [c, n] : w.changes)
      idle.insert(c);
    w.clear();
    const auto initial_controls = controls_of(*tg.proc).size();

    const QString win = QString::number(wid);
    if(points.empty())
      points = salient_points(grab(wid), 8);
    for(int i = 0; i < int(points.size()); i++)
    {
      if(only_point >= 0 && i != only_point)
        continue;
      const int x = points[i].first * xi.w, y = points[i].second * xi.h;
      for(const char* kind : {"drag", "wheel"})
      {
        r.tried++;
        tg.reset_gestures();
        w.clear();
        const shot before = grab(wid);
        const int idx = undo_index();
        xdo({"mousemove", "--window", win, QString::number(x), QString::number(y)});
        spin(60);
        if(kind == std::string_view{"drag"})
        {
          xdo({"mousedown", "1"});
          spin(60);
          for(int k = 1; k <= 8; k++)
          {
            xdo({"mousemove", "--window", win, QString::number(x + k),
                 QString::number(up ? y - 8 * k : y + 8 * k)});
            spin(30);
          }
          xdo({"mouseup", "1"});
        }
        else
        {
          for(int k = 0; k < 3; k++)
          {
            xdo({"click", up ? "4" : "5"});
            spin(40);
          }
        }
        // Gestures end, values get flushed, debounces run out
        spin(500);
        if(tg.clap)
        {
          clap_spy::tick();
          spin(100);
        }
        w.attach(*tg.proc);
        if(qEnvironmentVariableIsSet("SCORE_GUI_SWEEP_TRACE"))
          std::fprintf(
              stderr, "TRACE %s at %d,%d: %d pixels changed, %d values\n", kind, x, y,
              diff_pixels(before, grab(wid)), w.total());
        std::vector<Process::ControlInlet*> changed;
        for(auto& [c, n] : w.changes)
          if(n > 0 && !idle.contains(c))
            changed.push_back(c);
        for(auto& [c, n] : w.exec_changes)
          if(n > 0 && !idle.contains(c) && !ossia::contains(changed, c))
            changed.push_back(c);
        const int created = int(controls_of(*tg.proc).size()) - int(initial_controls);
        if(!changed.empty() || created > 0)
        {
          r.changed = true;
          r.point = i;
          r.kind = kind;
          r.controls = changed;
          r.value_changes = w.total();
          r.exec_changes = w.exec_total();
          r.undo_steps = undo_index() - idx;
          r.commands = commands_since(idx);
          r.new_controls = created;
          r.gui_diff = diff_pixels(before, grab(wid));
          r.gestures = tg.gestures();
          return r;
        }
      }
    }
    r.gestures = tg.gestures();
    return r;
  }

  //! The control of the process the editor changed, or the first one
  Process::ControlInlet* pick_control(const interaction& r)
  {
    if(!r.controls.empty())
      return r.controls.front();
    // One with a range: a value of the middle of it means something
    for(auto* c : controls_of(*t.proc))
    {
      const auto& d = c->domain().get();
      if(std::isfinite(as_double(ossia::get_min(d)))
         && std::isfinite(as_double(ossia::get_max(d))))
        return c;
    }
    auto c = controls_of(*t.proc);
    return c.empty() ? nullptr : c.front();
  }

  void host_to_gui(const editor& e, Process::ControlInlet* c)
  {
    phase("host");
    json h;
    if(!c)
    {
      j.raw("host", h.str("skipped", "no control").done());
      return;
    }
    const WId wid = interaction_window(e);
    h.str("control", c->name().toStdString());
    const auto [lo, hi] = range_of(*c);
    const double cur = as_double(c->value());
    const double to = std::isfinite(cur) && cur > (lo + hi) / 2 ? lo + 0.15 * (hi - lo)
                                                                : lo + 0.85 * (hi - lo);
    h.num("from", cur).num("to", to).num("lo", lo).num("hi", hi);
    h.num("plugin_before", t.plugin_value(c));
    const shot a = grab(wid);
    spin(400);
    const shot b = grab(wid);
    t.reset_gestures();
    const int idx = undo_index();
    // What score's slider does once released
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Process::SetControlValue{*c, to});
    spin(800);
    if(t.clap)
    {
      clap_spy::tick();
      spin(300);
    }
    const shot after = grab(wid);
    save(after, "host-change");
    const double pv = t.plugin_value(c);
    h.num("plugin_after", pv);
    h.num("score_after", as_double(c->value()));
    h.num("noise", diff_pixels(a, b));
    h.num("diff", diff_pixels(b, after));
    h.num("undo_steps", undo_index() - idx);
    h.raw("gestures", t.gestures());
    j.raw("host", h.done());
  }

  //! Through the phases of score_test/LivePlugin.hpp that involve the
  //! editor, which opens right after the drop, while the document plays
  void run_live()
  {
    std::unique_ptr<vst::Module> vst_module;
    j.str("mode", "live");
    phase("load");
    UuidKey<Process::ProcessModel> key;
    QString data;
    if(auto err = prepare(vst_module, key, data); !err.empty())
    {
      j.str("status", err);
      pc.out.write(j.done());
      return;
    }

    score::test::live::options opt;
    opt.what = {key, {}, data};
    opt.only = score::test::live::requested_phases();
    if(opt.only.empty())
      opt.only = {
          "live-drop-interval", "live-editor", "live-preset-change",
          "live-remove-undo-redo"};
    opt.open_editor = [this](Process::ProcessModel& proc) {
      auto& ctx = score::IDocument::documentContext(proc);
      auto& fact = pc.ctx.interfaces<Process::LayerFactoryList>();
      auto* f = fact.findDefaultFactory(proc);
      if(!f || !f->hasExternalUI(proc, ctx))
        throw score::test::live::skip{"no editor"};
      Process::setupExternalUI(proc, ctx, true);
      score::test::live::spin(1500);
      QPointer<QWidget> win = proc.externalUI;
      score::test::live::expect(win, "the editor did not open");
      const auto x = query_window(win->winId());
      score::test::live::expect(x.exists && x.viewable, "the editor window is not mapped");
      score::test::live::expect(x.w > 0 && x.h > 0, "the editor window has no size");
    };
    opt.close_editor = [](Process::ProcessModel& proc) {
      if(proc.externalUI)
        Process::setupExternalUI(proc, score::IDocument::documentContext(proc), false);
      score::test::live::spin(400);
      score::test::live::expect(!proc.externalUI, "the editor did not close");
    };

    score::test::live::verdicts res;
    score::test::live::recorder rec;
    rec.phase = [&](std::string_view ph) { phase(std::string{ph}.c_str()); };
    rec.result = [&](std::string_view ph, std::string_view verdict, std::string_view detail) {
      res.add(ph, verdict, detail);
    };
    score::test::live::run(pc.ctx, opt, rec);
    j.raw("live", res.json());
    j.str("status", "ran");
    pc.out.write(j.done());
    phase("teardown");
    while(!pc.ctx.docManager.documents().empty())
      pc.ctx.docManager.forceCloseDocument(pc.ctx, *pc.ctx.docManager.documents().front());
    spin(300);
    pc.out.phase(uri, "done");
  }

  void run()
  {
    std::unique_ptr<vst::Module> vst_module;
    phase("load");
    if(auto err = create(vst_module); !err.empty())
    {
      finish(err);
      return;
    }
    {
      std::vector<std::string> names;
      for(auto* c : controls_of(*t.proc))
        names.push_back(c->name().toStdString());
      j.num("controls", names.size());
      j.str("name", t.proc->prettyName().toStdString());
    }
    if(!has_editor())
    {
      finish("no-editor");
      return;
    }

    // --- open
    phase("open");
    editor e = open(t, *doc);
    {
      json o;
      o.boolean("created", bool(e.win));
      o.num("floating", e.floating.size());
      if(e.win)
      {
        o.boolean("visible", e.win->isVisible());
        o.num("qt_w", e.win->width()).num("qt_h", e.win->height());
        // What a window manager reads: a window whose smallest and largest
        // sizes agree cannot be resized by the user
        o.boolean("qt_fixed", e.win->minimumSize() == e.win->maximumSize());
        write_xinfo(o, "x_", query_window(e.wid));
        const auto ps = t.plugin_size();
        o.num("plugin_w", ps.width()).num("plugin_h", ps.height());
        o.num("resizable", t.resizable());
        if(t.is("clap"))
        {
          auto& m = *static_cast<Clap::Model*>(t.proc);
          o.num("clap_timers", m.timers.size());
          o.num("clap_fds", m.fd_notifiers.size());
          o.boolean("clap_timer_ext", m.handle()->ext_timer_support != nullptr);
        }
      }
      bool late{};
      const shot s = settled_shot(e, late);
      save(s, "open");
      s.write(o, "shot_");
      o.boolean("late", late);
      j.raw("open", o.done());
    }
    if(!e.win)
    {
      finish("open-failed");
      return;
    }
    if(const int hold = qEnvironmentVariableIntValue("SCORE_GUI_SWEEP_HOLD"); hold > 0)
    {
      std::fprintf(stderr, "HOLD window %lu\n", (unsigned long)e.wid);
      spin(hold);
    }

    // --- resize
    if(pc.wants("resize") && e.floating.empty() && t.resizable() == 1)
    {
      phase("resize");
      json r;
      const auto before = query_window(e.wid);
      const int w = before.w + 120, h = before.h + 90;
      r.num("from_w", before.w).num("from_h", before.h);
      r.num("to_w", w).num("to_h", h);
      xdo({"windowsize", QString::number(e.wid), QString::number(w), QString::number(h)});
      spin(1000);
      if(e.win)
      {
        const auto after = query_window(e.wid);
        write_xinfo(r, "x_", after);
        r.num("qt_w", e.win->width()).num("qt_h", e.win->height());
        const auto ps = t.plugin_size();
        r.num("plugin_w", ps.width()).num("plugin_h", ps.height());
        const shot s = grab(e.wid);
        save(s, "resized");
        s.write(r, "shot_");
      }
      r.boolean("alive", bool(e.win));
      j.raw("resize", r.done());
    }

    // --- interact
    interaction first;
    // A VST 2 plug-in with many parameters starts without controls: the user
    // has score create them for the parameters its editor changes
    if(t.is("vst") && (t.proc->flags() & Process::CanCreateControls))
    {
      t.proc->setCreatingControls(true);
      j.boolean("creating_controls", true);
    }
    if(e.win && pc.wants("interact"))
    {
      phase("interact");
      first = interact(t, e);
      json ij;
      write_interaction(ij, "", first);
      j.raw("interact", ij.done());
      save(grab(interaction_window(e)), "interacted");
    }

    // --- host to editor
    Process::ControlInlet* ctl = pick_control(first);
    if(e.win && pc.wants("host"))
      host_to_gui(e, ctl);

    // --- close, open again
    if(pc.wants("close"))
    {
      phase("close");
      json c;
      const WId old = e.wid;
      close(t, *doc);
      c.boolean("ui_reset", t.proc->externalUI == nullptr);
      c.boolean("window_gone", !e.win);
      const auto xi = query_window(old);
      c.boolean("x_gone", !xi.exists || !xi.viewable);
      phase("reopen");
      editor e2 = open(t, *doc);
      c.boolean("reopened", bool(e2.win));
      if(e2.win)
      {
        bool late{};
        const shot s = settled_shot(e2, late);
        save(s, "reopen");
        s.write(c, "shot_");
        write_xinfo(c, "x_", query_window(e2.wid));
      }
      phase("reclose");
      close(t, *doc);
      c.boolean("reclosed", t.proc->externalUI == nullptr);
      j.raw("close", c.done());
    }
    else
      close(t, *doc);

    // --- while playing
    if(pc.wants("play"))
    {
      phase("play");
      json p;
      auto& exec = pc.ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
      pc.ctx.docManager.setCurrentDocument(pc.ctx, doc);
      exec.request_play_global(true);
      spin(500);
      phase("play-open");
      editor ep = open(t, *doc);
      p.boolean("opened", bool(ep.win));
      if(ep.win)
      {
        bool late{};
        const shot s = settled_shot(ep, late);
        save(s, "play-open");
        s.write(p, "shot_");
        if(pc.wants("interact"))
        {
          phase("play-interact");
          const auto r = interact(t, ep, first.changed ? first.point : -1, false);
          write_interaction(p, "interact_", r);
        }
        phase("play-close");
        close(t, *doc);
        p.boolean("closed", t.proc->externalUI == nullptr);
      }
      phase("play-stop");
      exec.request_stop();
      spin(500);
      j.raw("play", p.done());
    }

    // --- a change in the editor kept through save and load
    if(pc.wants("persist") && first.changed && ctl)
    {
      phase("persist");
      json p;
      editor ep = open(t, *doc);
      interaction r;
      if(ep.win)
        r = interact(t, ep, first.point, true);
      if(ep.win && !r.changed)
        r = interact(t, ep, first.point, false);
      p.boolean("changed", r.changed);
      auto* c = r.changed ? pick_control(r) : ctl;
      const double score_before = as_double(c->value());
      const double plugin_before = t.plugin_value(c);
      p.str("control", c->name().toStdString());
      {
        const auto [lo, hi] = range_of(*c);
        p.num("lo", lo).num("hi", hi);
      }
      p.num("score_before", score_before).num("plugin_before", plugin_before);
      const shot before = ep.win ? grab(interaction_window(ep)) : shot{};
      save(before, "persist-before");
      const auto port_id = c->id();
      const auto proc_id = t.proc->id();
      close(t, *doc);

      phase("persist-reload");
      auto* doc2 = score::test::reload_via_json(pc.ctx, *doc);
      p.boolean("reloaded", doc2 != nullptr);
      if(doc2)
      {
        auto& itv = score::test::base_interval(*doc2);
        auto it = itv.processes.find(proc_id);
        if(it != itv.processes.end() && loaded(*it))
        {
          target t2{jb.format, &*it};
          instrument(t2);
          auto* c2 = qobject_cast<Process::ControlInlet*>(t2.proc->inlet(port_id));
          if(c2)
          {
            p.num("score_after", as_double(c2->value()));
            p.num("plugin_after", t2.plugin_value(c2));
          }
          phase("persist-open");
          editor e3 = open(t2, *doc2);
          p.boolean("reopened", bool(e3.win));
          if(e3.win)
          {
            bool late{};
            const shot s = settled_shot(e3, late);
            save(s, "persist-after");
            s.write(p, "shot_");
            p.num("diff", diff_pixels(before, s));
          }
          // A document closed with its editor open
          phase("close-doc");
          pc.ctx.docManager.forceCloseDocument(pc.ctx, *doc2);
          spin(500);
          p.boolean("closed_with_editor", true);
        }
        else
          p.boolean("process_found", false);
      }
      j.raw("persist", p.done());
    }

    finish("ok");
  }

  void finish(const std::string& status)
  {
    j.str("status", status);
    j.strings("notes", notes);
    pc.out.write(j.done());
    phase("teardown");
    if(doc)
    {
      if(t.proc && t.proc->externalUI)
        close(t, *doc);
      pc.ctx.docManager.forceCloseDocument(pc.ctx, *doc);
      doc = nullptr;
    }
    // Other documents (a reload whose process went missing)
    while(!pc.ctx.docManager.documents().empty())
      pc.ctx.docManager.forceCloseDocument(pc.ctx, *pc.ctx.docManager.documents().front());
    spin(300);
    pc.out.phase(uri, "done");
  }
};

}

namespace
{
// ---------------------------------------------------------------------------
// Listing

void list_clap(results_file& out, const std::string& path)
{
  void* lib = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
  if(!lib)
    return;
  auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
  if(!entry || !entry->init(path.c_str()))
    return;
  auto* f = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  if(f)
  {
    for(uint32_t i = 0; i < f->get_plugin_count(f); i++)
    {
      auto* d = f->get_plugin_descriptor(f, i);
      if(!d || !d->id)
        continue;
      json j;
      out.write(j.str("format", "clap")
                    .str("key", path + ":::" + d->id)
                    .str("file", path)
                    .str("name", d->name ? d->name : d->id)
                    .done());
    }
  }
  entry->deinit();
}

void list_vst3(
    const score::GUIApplicationContext& ctx, results_file& out, const std::string& path)
{
  auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
  auto mdl = plug.getModule(path);
  if(!mdl)
    return;
  for(const auto& cls : mdl->getFactory().classInfos())
  {
    if(cls.category() != kVstAudioEffectClass)
      continue;
    json j;
    out.write(j.str("format", "vst3")
                  .str("key", cls.ID().toString())
                  .str("file", path)
                  .str("name", cls.name())
                  .done());
  }
}

void list_vst(results_file& out, const std::string& path)
{
  std::unique_ptr<vst::Module> m;
  try
  {
    m = std::make_unique<vst::Module>(path);
  }
  catch(...)
  {
    return;
  }
  auto main = m->getMain();
  if(!main)
    return;
  AEffect* fx = main([](AEffect*, int32_t op, int32_t, intptr_t, void*, float) -> intptr_t {
    return op == audioMasterVersion ? 2400 : 0;
  });
  if(!fx || fx->magic != kEffectMagic)
    return;
  fx->dispatcher(fx, effOpen, 0, 0, nullptr, 0.f);
  const auto cat = fx->dispatcher(fx, effGetPlugCategory, 0, 0, nullptr, 0.f);
  char name[256]{};
  fx->dispatcher(fx, effGetEffectName, 0, 0, name, 0.f);
  json j;
  j.str("format", "vst").str("key", path).str("file", path);
  j.str("name", name[0] ? name : QFileInfo(QString::fromStdString(path)).completeBaseName().toStdString());
  j.boolean("editor", fx->flags & effFlagsHasEditor);
  j.boolean("shell", cat == kPlugCategShell);
  out.write(j.done());
  fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
  // Some plug-ins tear threads down late: the library stays loaded
  (void)m.release();
}

void list_lv2(const score::GUIApplicationContext& ctx, results_file& out)
{
  auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
  auto* world = plug.lilv.me;
  auto* native = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#Qt6UI");
  auto* all = lilv_world_get_all_plugins(world);
  LILV_FOREACH(plugins, it, all)
  {
    auto* p = lilv_plugins_get(all, it);
    auto* uis = lilv_plugin_get_uis(p);
    bool ok = false;
    LILV_FOREACH(uis, u, uis)
    {
      const LilvNode* type{};
      if(lilv_ui_is_supported(lilv_uis_get(uis, u), plug.suil.ui_supported, native, &type))
        ok = true;
    }
    lilv_uis_free(uis);
    if(!ok)
      continue;
    auto* name = lilv_plugin_get_name(p);
    json j;
    out.write(j.str("format", "lv2")
                  .str("key", lilv_node_as_uri(lilv_plugin_get_uri(p)))
                  .str("file", lilv_node_as_uri(lilv_plugin_get_bundle_uri(p)))
                  .str("name", name ? lilv_node_as_string(name) : "")
                  .done());
    lilv_node_free(name);
  }
  lilv_node_free(native);
}

//! What score's LV2 host sets up when plug-ins are not disabled, for the UIs
void setup_lv2(const score::GUIApplicationContext& ctx)
{
  auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
  plug.lv2_context->loadPlugins();
  // As discovery does when it is not disabled: every bundle of LV2_PATH
  lilv_world_load_all(plug.lilv.me);
  if(!plug.lv2_context->ui_host)
    plug.lv2_context->ui_host
        = plug.suil.host_new(LV2::on_uiMessage, LV2::port_index, nullptr, nullptr);
}
}

TEST_CASE("Plug-in editor sweep worker", "[plugin][gui][sweep]")
{
  const QString list = qEnvironmentVariable("SCORE_GUI_SWEEP_LIST");
  const QString jobs = qEnvironmentVariable("SCORE_GUI_SWEEP_JOBS");
  if(list.isEmpty() && jobs.isEmpty())
    return;
  install_crash_handler();

  // No scanner processes: the plug-ins are loaded one by one
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    install_crash_handler();
    setup_lv2(ctx);
    if(!list.isEmpty())
    {
      results_file out{list};
      for(auto& l : read_lines(qEnvironmentVariable("SCORE_GUI_SWEEP_LIST_INPUT")))
      {
        const auto fmt = l[0].toStdString();
        const auto path = l.size() > 1 ? l[1].toStdString() : std::string{};
        json ph;
        out.write(ph.str("type", "phase").str("file", path).done());
        if(fmt == "clap")
          list_clap(out, path);
        else if(fmt == "vst3")
          list_vst3(ctx, out, path);
        else if(fmt == "vst")
          list_vst(out, path);
        else if(fmt == "lv2")
          list_lv2(ctx, out);
        json d;
        out.write(d.str("type", "listed").str("file", path).done());
      }
      return;
    }

    results_file out{qEnvironmentVariable("SCORE_GUI_SWEEP_RESULTS")};
    probe_context pc{ctx, out, qEnvironmentVariable("SCORE_GUI_SWEEP_SHOTS")};
    pc.inject_gestures = qEnvironmentVariableIntValue("SCORE_GUI_SWEEP_CLAP_GESTURES") == 1;
    for(auto& s : qEnvironmentVariable("SCORE_GUI_SWEEP_STEPS").split(',', Qt::SkipEmptyParts))
      pc.steps.insert(s.toStdString());
    if(pc.shots_root.isEmpty())
      pc.shots_root = QDir::tempPath() + "/gui-sweep-shots";

    popup_closer closer;
    QTimer clap_ticks;
    QObject::connect(&clap_ticks, &QTimer::timeout, [] { clap_spy::tick(); });
    clap_ticks.start(20);

    for(auto& l : read_lines(jobs))
    {
      if(l.size() < 2)
        continue;
      job jb{l[0].toStdString(), l[1].toStdString(), l.size() > 2 ? l[2].toStdString() : ""};
      prober p{pc, jb};
      if(score::test::live::requested())
        p.run_live();
      else
        p.run();
    }
  });
}
