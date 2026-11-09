// The worker of the JSFX system sweep (jsfx_sweep.py drives it): every JSFX
// effect installed on the machine, run through score's host (the YSFX process,
// its execution component and its node, as a document plays them), then
// through the ysfx library used directly and minimally: the same engine
// without score's wrapper, so that a difference points at the wrapper.
//
// The worker measures and the driver judges: thresholds live in jsfx_sweep.py.
//
//  SCORE_JSFX_SWEEP_JOBS=<file>    probe the effects of these lines (one path
//                                  per line), appending one JSON object per
//                                  line to SCORE_JSFX_SWEEP_RESULTS: a "phase"
//                                  record before each step, so that a crash or
//                                  a hang can be told where it happened, then a
//                                  "result" record.
//
// Not set, the test case has nothing to do and passes: ctest runs the driver,
// not this.

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <YSFX/ApplicationPlugin.hpp>
#include <YSFX/ProcessModel.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QFile>

#include <libremidi/detail/conversion.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

constexpr double play_seconds = 2.0;
constexpr double replay_seconds = 1.0;
constexpr double reload_seconds = replay_seconds;
//! Where a block is cut in two ticks, as a loop or a start inside the buffer
//! cuts it in the engine
constexpr double split_at = 0.4;

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
  void phase(const std::string& path, const char* phase)
  {
    write(json{}.str("type", "phase").str("path", path).str("phase", phase).done());
  }
};

//! The same signal for both hosts: three partials and a little noise, each
//! channel a little apart from the others, and a chord then a lead note
struct stimulus
{
  int rate{};
  //! Nothing in: what a slider holds once set is compared on silence
  bool silent{};
  double audio(int channel, int64_t frame) const noexcept
  {
    if(silent)
      return 0.;
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
  std::vector<note> notes(int64_t start, int64_t n) const
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
    if(silent)
      return res;
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
  double max_diff_input{};
  bool compared_input{};

  void add(double v)
  {
    if(!ossia::safe_isfinite(v))
    {
      nonfinite++;
      return;
    }
    peak = std::max(peak, std::abs(v));
    sumsq += v * v;
    sum += v;
    samples++;
  }

  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    j.num(k("peak"), peak);
    j.num(k("rms"), samples ? std::sqrt(sumsq / samples) : 0.);
    j.num(k("dc"), samples ? sum / samples : 0.);
    j.num(k("nonfinite"), double(nonfinite));
    if(compared_input)
      j.num(k("max_diff_input"), max_diff_input);
  }
};

struct run_stats
{
  audio_stats audio;
  int64_t midi_out{};
  int64_t notes_out{};
  int64_t out_channels{};
  double seconds{};
  double wall_seconds{};
  //! Blocks cut in two ticks: those whose first part is silent while the
  //! second is not, of those that are not silent
  int64_t split_blocks{};
  int64_t split_first_silent{};

  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    audio.write(j, prefix);
    j.num(k("midi_out"), double(midi_out));
    j.num(k("notes_out"), double(notes_out));
    j.num(k("out_channels"), double(out_channels));
    j.num(k("rt_factor"), seconds > 0 ? wall_seconds / seconds : 0.);
    if(split_blocks > 0)
    {
      j.num(k("split_blocks"), double(split_blocks));
      j.num(k("split_first_silent"), double(split_first_silent));
    }
  }
};

std::vector<std::string> g_ref_log;
void ref_logger(intptr_t, ysfx_log_level level, const char* message)
{
  if(level == ysfx_log_error && g_ref_log.size() < 8)
    g_ref_log.emplace_back(message ? message : "");
}

struct slider_info
{
  uint32_t index{};
  std::string name;
  ysfx_slider_range_t range{};
  bool is_enum{};
  bool is_path{};
  bool visible{};
  uint32_t enum_size{};
};

std::vector<slider_info> sliders_of(ysfx_t* fx)
{
  std::vector<slider_info> res;
  for(uint32_t i = 0; i < ysfx_max_sliders; i++)
  {
    if(!ysfx_slider_exists(fx, i))
      continue;
    slider_info s;
    s.index = i;
    s.name = ysfx_slider_get_name(fx, i);
    ysfx_slider_get_range(fx, i, &s.range);
    s.is_enum = ysfx_slider_is_enum(fx, i);
    s.is_path = ysfx_slider_is_path(fx, i);
    s.visible = ysfx_slider_is_initially_visible(fx, i);
    s.enum_size = ysfx_slider_get_enum_size(fx, i);
    res.push_back(std::move(s));
  }
  return res;
}

//! A value of the slider other than its default, within its range
double other_value(const slider_info& s, int which = 0)
{
  if(s.is_enum || s.is_path)
  {
    if(s.enum_size < 2)
      return s.range.def;
    return double((int64_t(s.range.def) + 1 + which) % s.enum_size);
  }
  return s.range.min + (which ? 0.71 : 0.37) * (s.range.max - s.range.min);
}

bool close_enough(const slider_info& s, double a, double b)
{
  const double span = std::max(1e-9, std::abs(s.range.max - s.range.min));
  return std::abs(a - b) <= 1e-4 * span + 1e-6;
}

//! The engine used directly: the configuration score gives it (its import and
//! data roots guessed from the effect's path, the builtin audio formats), the
//! same rate and block size, the same stimulus, no wrapper
struct reference_host
{
  std::string path;
  int rate{};
  int block{};

  ysfx_config_u config{ysfx_config_new()};
  ysfx_u fx;
  bool loaded{};
  bool compiled{};

  bool open()
  {
    g_ref_log.clear();
    ysfx_register_builtin_audio_formats(config.get());
    ysfx_set_log_reporter(config.get(), &ref_logger);
    ysfx_guess_file_roots(config.get(), path.c_str());
    fx.reset(ysfx_new(config.get()));
    loaded = ysfx_load_file(fx.get(), path.c_str(), 0);
    if(!loaded)
      return false;
    compiled = ysfx_compile(fx.get(), 0);
    return compiled;
  }

  void prepare()
  {
    auto* y = fx.get();
    ysfx_set_sample_rate(y, rate);
    ysfx_set_block_size(y, block);
    ysfx_set_midi_capacity(y, 512, true);
    ysfx_init(y);
  }

  //! The sliders the effect writes itself (meters, values it derives): what
  //! they hold is not the host's to set
  std::vector<bool> written(const stimulus& stim)
  {
    std::vector<bool> res(ysfx_max_sliders);
    auto* y = fx.get();
    std::vector<double> before(ysfx_max_sliders);
    for(uint32_t i = 0; i < ysfx_max_sliders; i++)
      before[i] = ysfx_slider_get_value(y, i);
    for(uint8_t g = 0; g < ysfx_max_slider_groups; g++)
      ysfx_fetch_slider_changes(y, g);
    run_stats r;
    run(stim, 0.5, r);
    for(uint8_t g = 0; g < ysfx_max_slider_groups; g++)
    {
      const uint64_t m = ysfx_fetch_slider_changes(y, g);
      for(int i = 0; i < 64; i++)
        if(m & (uint64_t(1) << i))
          res[g * 64 + i] = true;
    }
    for(uint32_t i = 0; i < ysfx_max_sliders; i++)
      if(ysfx_slider_get_value(y, i) != before[i])
        res[i] = true;
    return res;
  }

  //! What the slider holds once the effect saw it set to v: @slider may round
  //! it, or bound it
  double settle(uint32_t index, double v)
  {
    auto* y = fx.get();
    ysfx_slider_set_value(y, index, v, true);
    const uint32_t ni = ysfx_get_num_inputs(y);
    const uint32_t no = ysfx_get_num_outputs(y);
    std::vector<float> zero(block), scratch(block * std::max<uint32_t>(no, 1));
    std::vector<const float*> ins(ni, zero.data());
    std::vector<float*> outs(no);
    for(uint32_t i = 0; i < no; i++)
      outs[i] = scratch.data() + i * block;
    // Playing, as score's node tells it
    ysfx_time_info_t info{};
    info.tempo = 120.;
    info.playback_state = ysfx_playback_playing;
    info.time_signature[0] = 4;
    info.time_signature[1] = 4;
    ysfx_set_time_info(y, &info);
    ysfx_process_float(y, ins.data(), outs.data(), ni, no, block);
    ysfx_midi_event_t ev;
    while(ysfx_receive_midi(y, &ev))
      ;
    return ysfx_slider_get_value(y, index);
  }

  //! With `split`, every block is two calls, cut where score's play cuts it
  void run(const stimulus& stim, double seconds, run_stats& res, bool split = false)
  {
    auto* y = fx.get();
    prepare();

    const uint32_t ni = ysfx_get_num_inputs(y);
    const uint32_t no = ysfx_get_num_outputs(y);
    res.out_channels = no;
    std::vector<std::vector<float>> in(ni, std::vector<float>(block));
    std::vector<std::vector<float>> out(no, std::vector<float>(block));
    std::vector<const float*> ins(ni);
    std::vector<float*> outs(no);

    const int cut = std::max(1, int(block * split_at));
    const int64_t total = int64_t(seconds * rate);
    const auto t0 = std::chrono::steady_clock::now();
    for(int64_t frame = 0; frame < total; frame += block)
    {
      for(uint32_t c = 0; c < ni; c++)
        for(int k = 0; k < block; k++)
          in[c][k] = float(stim.audio(int(c), frame + k));
      // In place, as hosts process: what @sample does not write is the input
      for(uint32_t c = 0; c < no; c++)
        for(int k = 0; k < block; k++)
          out[c][k] = c < ni ? in[c][k] : 0.f;

      const auto call = [&](int start, int len) {
        for(auto& n : stim.notes(frame + start, len))
        {
          uint8_t bytes[3]{
              uint8_t(n.on ? 0x90 : 0x80), uint8_t(n.pitch), uint8_t(n.on ? 100 : 0)};
          ysfx_midi_event_t ev{};
          ev.bus = 0;
          ev.offset = uint32_t(n.frame);
          ev.size = 3;
          ev.data = bytes;
          ysfx_send_midi(y, &ev);
        }
        ysfx_time_info_t info{};
        info.tempo = 120.;
        info.playback_state = ysfx_playback_playing;
        info.time_position = double(frame + start) / rate;
        info.beat_position = info.time_position * 2.;
        info.time_signature[0] = 4;
        info.time_signature[1] = 4;
        ysfx_set_time_info(y, &info);
        for(uint32_t i = 0; i < ni; i++)
          ins[i] = in[i].data() + start;
        for(uint32_t i = 0; i < no; i++)
          outs[i] = out[i].data() + start;
        ysfx_process_float(y, ins.data(), outs.data(), ni, no, len);

        ysfx_midi_event_t ev;
        while(ysfx_receive_midi(y, &ev))
        {
          res.midi_out++;
          res.notes_out += ev.size == 3 && (ev.data[0] & 0xF0) == 0x90 && ev.data[2] > 0;
        }
      };
      if(split)
      {
        call(0, cut);
        call(cut, block - cut);
      }
      else
      {
        call(0, block);
      }

      for(uint32_t c = 0; c < no; c++)
        for(int k = 0; k < block; k++)
        {
          const double v = out[c][k];
          res.audio.add(v);
          if(c == 0 && ni > 0)
          {
            res.audio.compared_input = true;
            if(ossia::safe_isfinite(v))
              res.audio.max_diff_input = std::max(
                  res.audio.max_diff_input, std::abs(v - double(in[0][k])));
          }
        }
    }
    res.seconds = double(total) / rate;
    res.wall_seconds
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  }
};

uint64_t midi2_note(bool on, int pitch)
{
  return on ? cmidi2_ump_midi2_note_on(0, 0, pitch, 0, 100 << 9, 0)
            : cmidi2_ump_midi2_note_off(0, 0, pitch, 0, 0, 0);
}

//! score's side of one effect: its document, its process, its execution
struct score_host
{
  const score::GUIApplicationContext& ctx;
  score::Document* doc{};
  YSFX::ProcessModel* proc{};

  Execution::DocumentPlugin& plug() const
  {
    return doc->context().plugin<Execution::DocumentPlugin>();
  }

  //! Starts the execution of the document, as the transport does
  ossia::graph_node* start()
  {
    auto& p = plug();
    p.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(p);
    if(!p.baseScenario())
      return nullptr;
    auto& procs = p.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    if(it == procs.end() || !it->second->node)
      return nullptr;
    return it->second->node.get();
  }

  //! Stops it, as the transport does
  void stop()
  {
    auto& p = plug();
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    if(p.baseScenario())
      for(auto& [_, c] : p.baseScenario()->baseInterval().processes())
        if(c->OSSIAProcessPtr())
          c->OSSIAProcessPtr()->stop();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    p.clear();
    QCoreApplication::processEvents();
  }

  int rate() { return int(plug().contextData()->execState->sampleRate); }
  int block() { return int(plug().contextData()->execState->bufferSize); }
};

//! The node's ports, by kind, in the order the node declares them
struct node_ports
{
  ossia::audio_port* audio_in{};
  ossia::midi_port* midi_in{};
  ossia::audio_port* audio_out{};
  ossia::midi_port* midi_out{};
  std::vector<ossia::value_port*> controls;

  explicit node_ports(ossia::graph_node& n)
  {
    for(auto* i : n.root_inputs())
    {
      if(auto* a = i->target<ossia::audio_port>())
        audio_in = a;
      else if(auto* m = i->target<ossia::midi_port>())
        midi_in = m;
      else if(auto* v = i->target<ossia::value_port>())
        controls.push_back(v);
    }
    for(auto* o : n.root_outputs())
    {
      if(auto* a = o->target<ossia::audio_port>())
        audio_out = a;
      else if(auto* m = o->target<ossia::midi_port>())
        midi_out = m;
    }
  }
};

//! One play of score's host: the node run block by block as the graph runs
//! it, its controls through the execution queue first. With `split`, every
//! block is two ticks, as a loop point inside the buffer makes it.
run_stats play(
    score_host& host, ossia::graph_node& node, const stimulus& stim, double seconds,
    bool split)
{
  run_stats res;
  auto& st = *host.plug().contextData()->execState;
  const int rate = int(st.sampleRate);
  const int block = int(st.bufferSize);
  node_ports ports{node};
  const int in_channels = ports.audio_in ? std::max<int>(ysfx_get_num_inputs(host.proc->fx.get()), 1) : 0;

  const int64_t total = int64_t(seconds * rate);
  const double flicks_per_frame = ossia::flicks_per_second<double> / rate;
  const int cut = std::max(1, int(block * split_at));
  const auto t0 = std::chrono::steady_clock::now();
  for(int64_t frame = 0; frame < total; frame += block)
  {
    score::test::run_exec(host.plug());

    if(ports.audio_in)
    {
      auto& a = *ports.audio_in;
      a.set_channels(in_channels);
      for(int c = 0; c < in_channels; c++)
      {
        a.channel(c).resize(block);
        for(int k = 0; k < block; k++)
          a.channel(c)[k] = stim.audio(c, frame + k);
      }
    }
    if(ports.midi_in)
    {
      ports.midi_in->messages.clear();
      for(auto& n : stim.notes(frame, block))
      {
        const uint64_t w = midi2_note(n.on, n.pitch);
        libremidi::ump u{uint32_t(w >> 32), uint32_t(w & 0xffffffff)};
        u.timestamp = n.frame;
        ports.midi_in->messages.push_back(u);
      }
    }
    // As the graph does before each buffer: outputs cleared, the clock moved
    if(ports.audio_out)
      ports.audio_out->set_channels(0);
    st.samples_since_start = frame;

    const auto tick = [&](int64_t start, int64_t len) {
      ossia::token_request tk{
          ossia::time_value{int64_t((frame + start) * flicks_per_frame)},
          ossia::time_value{int64_t((frame + start + len) * flicks_per_frame)},
          ossia::time_value{int64_t(seconds * ossia::flicks_per_second<double>)},
          ossia::time_value{0},
          1.,
          ossia::time_signature{4, 4},
          120.};
      tk.start_sample = start;
      tk.length_sample = len;
      node.run(tk, ossia::exec_state_facade{&st});
    };
    if(split)
    {
      tick(0, cut);
      tick(cut, block - cut);
    }
    else
    {
      tick(0, block);
    }

    if(ports.audio_out)
    {
      auto& out = *ports.audio_out;
      res.out_channels = out.channels();
      bool first_loud = false, second_loud = false;
      for(std::size_t c = 0; c < out.channels(); c++)
      {
        auto& ch = out.channel(c);
        for(std::size_t k = 0; k < ch.size() && k < std::size_t(block); k++)
        {
          const double v = ch[k];
          res.audio.add(v);
          if(ossia::safe_isfinite(v) && std::abs(v) > 1e-9)
            (int(k) < cut ? first_loud : second_loud) = true;
          if(c == 0 && ports.audio_in)
          {
            res.audio.compared_input = true;
            if(ossia::safe_isfinite(v))
              res.audio.max_diff_input = std::max(
                  res.audio.max_diff_input, std::abs(v - stim.audio(0, frame + k)));
          }
        }
      }
      if(split && (first_loud || second_loud))
      {
        res.split_blocks++;
        res.split_first_silent += !first_loud && second_loud;
      }
    }
    if(ports.midi_out)
    {
      res.midi_out += ports.midi_out->messages.size();
      for(auto& m : ports.midi_out->messages)
        res.notes_out += cmidi2_ump_get_status_code((cmidi2_ump*)m.data) == CMIDI2_STATUS_NOTE_ON
                         && cmidi2_ump_get_midi2_note_velocity((cmidi2_ump*)m.data) > 0;
      ports.midi_out->messages.clear();
    }
    // As teardown_node: what came in was for this buffer only
    for(auto* v : ports.controls)
      v->get_data().clear();
    if(ports.midi_in)
      ports.midi_in->messages.clear();
  }
  res.seconds = double(total) / rate;
  res.wall_seconds
      = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return res;
}

//! One block of silence, as the reference settles a slider on
void one_tick(score_host& host, ossia::graph_node& node, const stimulus& stim)
{
  const stimulus silence{stim.rate, true};
  (void)play(host, node, silence, double(host.block()) / stim.rate, false);
}

//! Slider i of the effect, as score shows it: the control with id 4 + i
Process::ControlInlet* control_of(YSFX::ProcessModel& p, uint32_t i)
{
  return qobject_cast<Process::ControlInlet*>(p.inlet(Id<Process::Port>(4 + int(i))));
}

YSFX::ProcessModel* find_ysfx(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto* y = qobject_cast<YSFX::ProcessModel*>(&p))
      return y;
  return nullptr;
}

std::string join(const std::vector<std::string>& v, std::size_t max = 4)
{
  std::string r;
  for(std::size_t i = 0; i < v.size() && i < max; i++)
    r += (i ? "; " : "") + v[i];
  if(v.size() > max)
    r += "; +" + std::to_string(v.size() - max);
  return r;
}

void probe(const score::GUIApplicationContext& ctx, results_file& out, const std::string& path)
{
  json j;
  j.str("type", "result").str("path", path);

  // The reference first: what the effect is, and whether it compiles at all
  out.phase(path, "reference");
  stimulus stim{48000};
  reference_host ref{path};
  const bool ref_ok = ref.open();
  j.boolean("ref_loaded", ref.loaded).boolean("ref_compiled", ref.compiled);
  j.strings("ref_errors", g_ref_log);
  std::vector<slider_info> ref_sliders;
  if(ref.loaded)
  {
    auto* y = ref.fx.get();
    j.str("name", ysfx_get_name(y));
    j.num("inputs", ysfx_get_num_inputs(y));
    j.num("outputs", ysfx_get_num_outputs(y));
    j.boolean("has_gfx", ysfx_has_section(y, ysfx_section_gfx));
    j.boolean("has_serialize", ysfx_has_section(y, ysfx_section_serialize));
    j.boolean("has_sample", ysfx_has_section(y, ysfx_section_sample));
    j.boolean("has_block", ysfx_has_section(y, ysfx_section_block));
    ref_sliders = sliders_of(y);
    j.num("sliders", double(ref_sliders.size()));
    int hidden = 0, enums = 0, paths = 0, inverted = 0, gaps = 0;
    for(std::size_t k = 0; k < ref_sliders.size(); k++)
    {
      auto& s = ref_sliders[k];
      hidden += !s.visible;
      enums += s.is_enum && !s.is_path;
      paths += s.is_path;
      inverted += !s.is_enum && s.range.min > s.range.max;
      gaps += s.index != k;
    }
    j.num("sliders_hidden", hidden).num("sliders_enum", enums).num("sliders_path", paths);
    j.num("sliders_inverted", inverted).num("sliders_after_gap", gaps);
    if(auto bank = ysfx_get_bank_path(y); bank && *bank)
      if(ysfx_bank_t* b = ysfx_load_bank(bank))
      {
        j.num("bank_presets", b->preset_count);
        ysfx_bank_free(b);
      }
  }
  if(!ref.loaded)
  {
    out.write(j.str("status", "load-failed").done());
    return;
  }

  // score's side
  out.phase(path, "model");
  score_host host{ctx};
  host.doc = score::test::new_document(ctx);
  if(!host.doc)
  {
    out.write(j.str("status", "no-document").done());
    return;
  }
  const auto t0 = std::chrono::steady_clock::now();
  host.proc = static_cast<YSFX::ProcessModel*>(
      score::test::add_process(*host.doc, ysfx_uuid, QString::fromStdString(path)));
  j.num(
      "load_ms", std::chrono::duration<double, std::milli>(
                     std::chrono::steady_clock::now() - t0)
                     .count());
  if(host.proc && host.proc->fx)
  {
    auto* cfg = ysfx_get_config(host.proc->fx.get());
    j.str("score_import_root", ysfx_get_import_root(cfg));
    j.str("score_data_root", ysfx_get_data_root(cfg));
  }
  j.str("ref_import_root", ysfx_get_import_root(ref.config.get()));
  j.str("ref_data_root", ysfx_get_data_root(ref.config.get()));

  const auto close = [&] {
    out.phase(path, "teardown");
    ctx.docManager.forceCloseDocument(ctx, *host.doc);
    QCoreApplication::processEvents();
    score::test::flush_deferred_deletes();
  };

  std::vector<slider_info> settable;
  if(ref_ok)
  {
    ref.rate = 48000;
    ref.block = 512;
    ref.prepare();

    reference_host probe_written{path, 48000, 512};
    if(probe_written.open())
    {
      const auto w = probe_written.written(stim);
      std::vector<std::string> names;
      for(auto& s : ref_sliders)
        if(w[s.index])
          names.push_back("slider" + std::to_string(s.index + 1));
        else
          settable.push_back(s);
      j.strings("sliders_written", names);
    }
  }
  const bool score_compiled = host.proc && host.proc->fx && ysfx_is_compiled(host.proc->fx.get());
  j.boolean("score_compiled", score_compiled);
  if(!ref_ok)
  {
    if(host.proc)
    {
      // Not compiled anywhere: what matters is that score survives it, and
      // plays it as it plays anything else
      out.phase(path, "play-uncompiled");
      if(auto* node = host.start())
        (void)play(host, *node, stim, 0.1, false);
      out.phase(path, "stop-uncompiled");
      host.stop();
    }
    close();
    out.write(j.str("status", "compile-failed").done());
    return;
  }
  if(!score_compiled)
  {
    close();
    out.write(j.str("status", "score-compile-failed").done());
    return;
  }

  // The controls score made against the sliders of the effect
  std::vector<std::string> mismatch, default_outside;
  for(auto& s : ref_sliders)
  {
    auto* c = control_of(*host.proc, s.index);
    if(!c)
    {
      mismatch.push_back("slider" + std::to_string(s.index + 1) + ": no control");
      continue;
    }
    const double init = ossia::convert<double>(c->init());
    // ysfx counts choices from 0: a default the script puts outside them
    // has no choice to show
    const bool outside_choices
        = (s.is_enum || s.is_path) && (s.range.def < 0 || s.range.def >= s.enum_size);
    if(outside_choices)
      default_outside.push_back("slider" + std::to_string(s.index + 1));
    else if(!close_enough(s, init, s.range.def))
      mismatch.push_back(
          "slider" + std::to_string(s.index + 1) + ": default " + std::to_string(init)
          + " in score, " + std::to_string(s.range.def) + " in the effect");
    if(s.is_enum || s.is_path)
    {
      auto* cb = qobject_cast<Process::ComboBox*>(c);
      if(!cb)
        mismatch.push_back("slider" + std::to_string(s.index + 1) + ": not a choice");
      else if(cb->alternatives.size() != s.enum_size)
        mismatch.push_back(
            "slider" + std::to_string(s.index + 1) + ": "
            + std::to_string(cb->alternatives.size()) + " choices in score, "
            + std::to_string(s.enum_size) + " in the effect");
    }
    // What the effect holds once score made its controls, against what it
    // holds once initialised alone
    const double held = ysfx_slider_get_value(host.proc->fx.get(), s.index);
    const double ref_held = ysfx_slider_get_value(ref.fx.get(), s.index);
    const bool is_settable = std::any_of(
        settable.begin(), settable.end(), [&](auto& o) { return o.index == s.index; });
    if(is_settable && !close_enough(s, held, ref_held))
      mismatch.push_back(
          "slider" + std::to_string(s.index + 1) + ": holds " + std::to_string(held)
          + " after loading, " + std::to_string(ref_held) + " in the reference");
  }
  j.strings("control_mismatch", mismatch);
  j.strings("enum_default_outside", default_outside);
  j.num("bank_presets_score", double(host.proc->builtinPresets().size()));

  out.phase(path, "start");
  auto* node = host.start();
  if(!node)
  {
    close();
    out.write(j.str("status", "no-node").done());
    return;
  }
  const int rate = host.rate(), block = host.block();
  j.num("rate", rate).num("block", block);
  stim.rate = rate;
  ref.rate = rate;
  ref.block = block;
  ref.prepare();

  out.phase(path, "play");
  const auto first = play(host, *node, stim, play_seconds, false);
  first.write(j, "play_");
  j.num("score_srate", ysfx_read_var(host.proc->fx.get(), "srate"));
  j.num("score_samplesblock", ysfx_read_var(host.proc->fx.get(), "samplesblock"));
  j.num("pdc_delay", ysfx_get_pdc_delay(host.proc->fx.get()));
  QCoreApplication::processEvents();

  out.phase(path, "stop");
  const auto ts = std::chrono::steady_clock::now();
  host.stop();
  j.num(
      "stop_ms",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - ts)
          .count());

  out.phase(path, "replay");
  node = host.start();
  if(!node)
  {
    close();
    out.write(j.str("status", "no-node-on-replay").done());
    return;
  }
  const auto second = play(host, *node, stim, replay_seconds, false);
  second.write(j, "replay_");

  // Sliders the effect sets itself when they are set (bounds, steps that
  // depend on other sliders), or as it plays: what they then hold depends on
  // the effect's state, which the two hosts do not share
  std::set<uint32_t> rewritten;

  // Every slider through its port, as a cable or an automation drives it,
  // from a stop: the replay ends with the chord still held
  out.phase(path, "ports");
  host.stop();
  node = host.start();
  if(!node)
  {
    close();
    out.write(j.str("status", "no-node-on-replay").done());
    return;
  }
  {
    std::vector<std::string> wrong;
    node_ports ports{*node};
    // The node's ports after audio and MIDI are the controls of the process,
    // in the order of the process
    std::vector<uint32_t> order;
    for(auto* inl : host.proc->inlets())
      if(qobject_cast<Process::ControlInlet*>(inl))
        order.push_back(inl->id().val() - 4);
    for(std::size_t k = 0; k < settable.size(); k++)
    {
      auto& s = settable[k];
      const double v = other_value(s);
      if(close_enough(s, v, ysfx_slider_get_value(host.proc->fx.get(), s.index)))
        continue;
      auto it = std::find(order.begin(), order.end(), s.index);
      if(it == order.end() || std::size_t(it - order.begin()) >= ports.controls.size())
        continue;
      auto* port = ports.controls[it - order.begin()];
      port->write_value(
          (s.is_enum || s.is_path) ? ossia::value{int(v)} : ossia::value{float(v)}, 0);
      one_tick(host, *node, stim);
      const double got = ysfx_slider_get_value(host.proc->fx.get(), s.index);
      one_tick(host, *node, stim);
      const bool moves = !close_enough(s, got, ysfx_slider_get_value(host.proc->fx.get(), s.index));
      const double expected = ref.settle(s.index, v);
      if(moves || !close_enough(s, expected, v))
      {
        rewritten.insert(s.index);
        continue;
      }
      if(!close_enough(s, got, expected))
        wrong.push_back(
            "slider" + std::to_string(s.index + 1) + " port sent " + std::to_string(v)
            + ", slider holds " + std::to_string(got) + ", "
            + std::to_string(expected) + " in the reference");
    }
    j.strings("port_mismatch", wrong);
  }

  // Every slider through its control, as the UI changes it
  out.phase(path, "controls");
  {
    std::vector<std::string> wrong;
    for(auto& s : settable)
    {
      auto* c = control_of(*host.proc, s.index);
      if(!c)
        continue;
      // A value neither the control nor the slider holds: setting the value
      // a control already has changes nothing
      const double shown = ossia::convert<double>(c->value());
      const double held = ysfx_slider_get_value(host.proc->fx.get(), s.index);
      std::optional<double> v;
      for(double cand : {other_value(s, 1), other_value(s, 0), s.range.def})
        if(!close_enough(s, cand, shown) && !close_enough(s, cand, held))
        {
          v = cand;
          break;
        }
      if(!v)
        continue;
      c->setValue(
          (s.is_enum || s.is_path) ? ossia::value{int(*v)} : ossia::value{float(*v)});
      one_tick(host, *node, stim);
      const double got = ysfx_slider_get_value(host.proc->fx.get(), s.index);
      one_tick(host, *node, stim);
      const bool moves = !close_enough(s, got, ysfx_slider_get_value(host.proc->fx.get(), s.index));
      const double expected = ref.settle(s.index, *v);
      if(moves || !close_enough(s, expected, *v))
      {
        rewritten.insert(s.index);
        continue;
      }
      if(!close_enough(s, got, expected))
        wrong.push_back(
            "slider" + std::to_string(s.index + 1) + " control set " + std::to_string(*v)
            + ", slider holds " + std::to_string(got) + ", "
            + std::to_string(expected) + " in the reference");
    }
    j.strings("ui_mismatch", wrong);
  }

  // Back to the defaults, as the reference plays
  for(auto& s : ref_sliders)
  {
    if(auto* c = control_of(*host.proc, s.index))
      c->setValue(
          (s.is_enum || s.is_path) ? ossia::value{int(s.range.def)}
                                   : ossia::value{float(s.range.def)});
    ysfx_slider_set_value(host.proc->fx.get(), s.index, s.range.def, true);
  }

  out.phase(path, "split");
  host.stop();
  node = host.start();
  if(node)
  {
    const auto split = play(host, *node, stim, replay_seconds, true);
    split.write(j, "split_");
  }
  out.phase(path, "stop2");
  host.stop();

  // Saved and loaded again, as a document is
  out.phase(path, "reload");
  {
    // A control changed before the save, so that the value saved is not the
    // default
    std::vector<std::pair<const slider_info*, double>> changed;
    for(auto& s : settable)
      if(auto* c = control_of(*host.proc, s.index))
      {
        const double v = other_value(s);
        c->setValue(
            (s.is_enum || s.is_path) ? ossia::value{int(v)} : ossia::value{float(v)});
        changed.push_back({&s, v});
      }

    auto* doc2 = score::test::reload_via_json(ctx, *host.doc);
    YSFX::ProcessModel* p2 = doc2 ? find_ysfx(*doc2) : nullptr;
    j.boolean("reload_found", p2 != nullptr);
    if(p2)
    {
      j.boolean("reload_compiled", p2->fx && ysfx_is_compiled(p2->fx.get()));
      j.num("bank_presets_reload", double(p2->builtinPresets().size()));
      std::vector<std::string> lost;
      if(p2->fx && ysfx_is_compiled(p2->fx.get()))
      {
        // The same values through a state of the reference, saved from one
        // instance and loaded in a new one: what the script itself makes of
        // its state when it is loaded
        std::map<uint32_t, double> ref_loaded;
        {
          reference_host from{path, rate, block}, to{path, rate, block};
          if(from.open() && to.open())
          {
            from.prepare();
            for(auto& [s, v] : changed)
              ysfx_slider_set_value(
                  from.fx.get(), s->index,
                  ysfx_slider_get_value(host.proc->fx.get(), s->index), true);
            if(ysfx_state_t* st = ysfx_save_state(from.fx.get()))
            {
              to.prepare();
              ysfx_load_state(to.fx.get(), st);
              ysfx_state_free(st);
              for(auto& [s, v] : changed)
                ref_loaded[s->index] = ysfx_slider_get_value(to.fx.get(), s->index);
            }
          }
        }
        for(auto& [s, v] : changed)
        {
          const double got = ysfx_slider_get_value(p2->fx.get(), s->index);
          const double saved = ysfx_slider_get_value(host.proc->fx.get(), s->index);
          const auto ref_it = ref_loaded.find(s->index);
          const bool ref_keeps
              = ref_it == ref_loaded.end() || close_enough(*s, ref_it->second, saved);
          if(ref_keeps && !close_enough(*s, got, saved))
            lost.push_back(
                "slider" + std::to_string(s->index + 1) + " saved " + std::to_string(saved)
                + ", loaded " + std::to_string(got));
          if(auto* c = control_of(*p2, s->index);
             c && !close_enough(*s, ossia::convert<double>(c->value()), v))
            lost.push_back(
                "slider" + std::to_string(s->index + 1) + " control saved "
                + std::to_string(v) + ", shows "
                + std::to_string(ossia::convert<double>(c->value())));
        }
        j.strings("reload_state_mismatch", lost);

        // Changed on the loaded document, then played
        std::vector<std::string> wrong;
        for(auto& [s, v] : changed)
          if(auto* c = control_of(*p2, s->index))
            c->setValue(
                (s->is_enum || s->is_path) ? ossia::value{int(s->range.def)}
                                           : ossia::value{float(s->range.def)});
        score_host h2{ctx, doc2, p2};
        out.phase(path, "reload-play");
        if(auto* n2 = h2.start())
        {
          one_tick(h2, *n2, stim);
          for(auto& [s, v] : changed)
          {
            const double got = ysfx_slider_get_value(p2->fx.get(), s->index);
            const double expected = ref.settle(s->index, s->range.def);
            if(!close_enough(*s, expected, s->range.def))
            {
              rewritten.insert(s->index);
              continue;
            }
            if(!close_enough(*s, got, expected))
              wrong.push_back(
                  "slider" + std::to_string(s->index + 1) + " control set "
                  + std::to_string(s->range.def) + " after loading, slider holds "
                  + std::to_string(got) + ", " + std::to_string(expected)
                  + " in the reference");
          }
          const auto third = play(h2, *n2, stim, reload_seconds, false);
          third.write(j, "reload_");
          h2.stop();
        }
        j.strings("reload_ui_mismatch", wrong);
      }
      ctx.docManager.forceCloseDocument(ctx, *doc2);
      QCoreApplication::processEvents();
    }
  }

  {
    std::vector<std::string> names;
    for(auto i : rewritten)
      names.push_back("slider" + std::to_string(i + 1));
    j.strings("sliders_rewritten", names);
  }

  out.phase(path, "reference-play");
  {
    // A fresh instance: the passes above moved the sliders of this one
    reference_host fresh{path, rate, block};
    run_stats r;
    if(fresh.open())
    {
      fresh.run(stim, play_seconds, r);
      r.write(j, "ref_");
    }
    reference_host split{path, rate, block};
    run_stats rs;
    if(split.open())
    {
      split.run(stim, replay_seconds, rs, true);
      j.num("ref_split_notes_out", double(rs.notes_out));
    }
  }

  close();
  out.write(j.str("status", "ran").done());
}
}

TEST_CASE("JSFX system sweep worker", "[ysfx][sweep]")
{
  const QString jobs = qEnvironmentVariable("SCORE_JSFX_SWEEP_JOBS");
  const QString results = qEnvironmentVariable("SCORE_JSFX_SWEEP_RESULTS");
  if(jobs.isEmpty() || results.isEmpty())
    return;

  QFile f{jobs};
  REQUIRE(f.open(QIODevice::ReadOnly));
  std::vector<std::string> todo;
  for(const auto& line : QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts))
    todo.push_back(line.toStdString());

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    results_file out{results};
    for(auto& path : todo)
      probe(ctx, out, path);
  });
}
