// The worker of the LV2 system sweep (lv2_sweep.py drives it): every LV2
// plug-in installed on the machine, run through score's host with a stimulus
// fitting what it is (an instrument gets notes, an effect gets a signal), then
// through a minimal host of its own written against the specification only.
// A plug-in silent in score but not in that host points at score; silent in
// both, at the plug-in or at what it needs and is not given.
//
// The worker measures and the driver judges: thresholds live in lv2_sweep.py.
//
//  SCORE_LV2_SWEEP_LIST=<file>     write every installed plug-in, one per line:
//                                  uri \t bundle uri \t name \t class
//  SCORE_LV2_SWEEP_JOBS=<file>     probe the plug-ins of these lines (uri \t
//                                  bundle uri), appending one JSON object per
//                                  line to SCORE_LV2_SWEEP_RESULTS: a "phase"
//                                  record before each step, so that a crash or
//                                  a hang can be told where it happened, then a
//                                  "result" record.
//
//  SCORE_LV2_SWEEP_TRACE=1        print the peak of every block of score's
//                                  host on stderr, to look closer at one.
//
//  SCORE_SWEEP_LIVE=1             instead, add each plug-in to a playing
//                                  document in every way a user can
//                                  (score_test/LivePlugin.hpp).
//
// Neither set, the test case has nothing to do and passes: ctest runs the
// driver, not this.

#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <LV2/Node.hpp>

#include <Media/AudioPluginCache.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QThread>

#include <libremidi/detail/conversion.hpp>
#include <lv2/atom/util.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct noop_hook
{
  void operator()() const noexcept { }
  void operator()(auto&) const noexcept { }
};
using sweep_node = LV2::lv2_node<noop_hook, noop_hook>;

constexpr int block = 512;
constexpr double play_seconds = 2.0;
constexpr double replay_seconds = 1.0;
constexpr unsigned settle_ms = 250;

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
  //! While the chord is held, and the last 0.1 s
  double sumsq_held{};
  int64_t samples_held{};
  double peak_tail{};
  //! Largest difference with the input on the first channel
  double max_diff_input{};
  bool compared_input{};

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
    if(t >= play_seconds - 0.1)
      peak_tail = std::max(peak_tail, a);
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
    if(compared_input)
      j.num(k("max_diff_input"), max_diff_input);
  }
};

//! What came out of the control outputs of a run, by port
struct control_stats
{
  std::map<int, std::pair<float, float>> range;
  std::map<int, float> first;
  int64_t nonfinite{};
  void add(int port, float v)
  {
    if(!ossia::safe_isfinite(v))
    {
      nonfinite++;
      return;
    }
    if(!first.contains(port))
      first[port] = v;
    auto [it, inserted] = range.try_emplace(port, v, v);
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
      n += (r.second - r.first) > 1e-6f;
    return n;
  }
};

struct run_stats
{
  audio_stats audio;
  control_stats controls;
  int64_t midi_out{};
  int64_t atom_out{};
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
    j.num(k("atom_out"), double(atom_out));
    j.num(k("rt_factor"), seconds > 0 ? wall_seconds / seconds : 0.);
  }
};

uint64_t midi2_note(bool on, int pitch)
{
  return on ? cmidi2_ump_midi2_note_on(0, 0, pitch, 0, 100 << 9, 0)
            : cmidi2_ump_midi2_note_off(0, 0, pitch, 0, 0, 0);
}

//! One play of score's host: a node made as LV2EffectComponent makes it, the
//! controls at the values of the process
run_stats play(
    LV2::Model& model, LV2::LV2Data& data, const stimulus& stim, double seconds,
    bool pace)
{
  run_stats res;
  const int rate = stim.rate;
  ossia::execution_state st;
  st.sampleRate = rate;
  st.bufferSize = block;

  sweep_node node{data, rate, LV2::choose_voice_strategy(data), {}, {}};
  for(std::size_t i = model.m_controlInStart; i < model.inlets().size(); i++)
  {
    auto* inlet = dynamic_cast<Process::ControlInlet*>(model.inlets()[i]);
    if(!inlet)
      continue;
    const auto ctrl_idx = i - model.m_controlInStart;
    const float v = ossia::convert<float>(inlet->value());
    for(auto& voice : node.voices)
      if(ctrl_idx < voice->fInControls.size())
        voice->fInControls[ctrl_idx] = v * node.fInScale[ctrl_idx];
  }

  if(qEnvironmentVariableIsSet("SCORE_LV2_SWEEP_TRACE"))
    for(std::size_t i = 0; i < node.voices[0]->fInControls.size(); i++)
      std::fprintf(
          stderr, "TRACE control %d = %g\n", data.control_in_ports[i],
          node.voices[0]->fInControls[i]);

  const auto audio_in = data.audio_in_ports.size();
  const std::size_t in_channels
      = audio_in == 0 ? 0 : (node.routing == LV2::voice_routing::per_channel ? 2 : audio_in);
  const std::size_t cv_in = data.cv_ports.size();
  const std::size_t midi_in = data.midi_in_ports.size();
  const std::size_t midi_out = data.midi_out_ports.size();
  const std::size_t atom_out = data.atom_out_ports.size();
  const std::size_t control_out = data.control_out_ports.size();
  const bool has_audio_out = !data.audio_out_ports.empty();
  const std::size_t out_base = has_audio_out ? 1 : 0;

  const bool trace = qEnvironmentVariableIsSet("SCORE_LV2_SWEEP_TRACE");
  const int64_t total = int64_t(seconds * rate);
  const double flicks_per_frame = ossia::flicks_per_second<double> / rate;
  const auto t0 = std::chrono::steady_clock::now();
  std::chrono::steady_clock::duration paced{};
  for(int64_t frame = 0; frame < total; frame += block)
  {
    std::size_t inl = 0;
    if(audio_in > 0)
    {
      auto& a = node.root_inputs()[inl++]->cast<ossia::audio_port>();
      a.set_channels(in_channels);
      for(std::size_t c = 0; c < in_channels; c++)
      {
        a.channel(c).resize(block);
        for(int k = 0; k < block; k++)
          a.channel(c)[k] = stim.audio(int(c), frame + k);
      }
    }
    for(std::size_t i = 0; i < cv_in; i++)
    {
      auto& a = node.root_inputs()[inl++]->cast<ossia::audio_port>();
      a.set_channels(1);
      a.channel(0).assign(block, 0.);
    }
    for(std::size_t i = 0; i < midi_in; i++)
    {
      auto& m = node.root_inputs()[inl++]->cast<ossia::midi_port>();
      m.messages.clear();
      for(auto& n : stim.notes(frame, block))
      {
        const uint64_t w = midi2_note(n.on, n.pitch);
        libremidi::ump u{uint32_t(w >> 32), uint32_t(w & 0xffffffff)};
        u.timestamp = n.frame;
        m.messages.push_back(u);
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
    ossia::exec_state_facade fac{&st};
    static_cast<ossia::graph_node&>(node).run(tk, fac);

    if(trace && midi_in > 0 && !stim.notes(frame, block).empty())
    {
      auto* seq = &node.voices[0]->midi_atom_ins[0].buf->atoms;
      std::fprintf(
          stderr, "TRACE seq type %u size %u unit %u (sequence urid %u, midi urid %u)\n",
          seq->atom.type, seq->atom.size, seq->body.unit,
          data.host.atom_sequence_id, data.host.midi_event_id);
      LV2_ATOM_SEQUENCE_FOREACH(seq, ev)
      {
        const auto* b = (const uint8_t*)LV2_ATOM_BODY_CONST(&ev->body);
        std::fprintf(
            stderr, "TRACE   ev frame %lld type %u size %u bytes %02x %02x %02x\n",
            (long long)ev->time.frames, ev->body.type, ev->body.size, b[0],
            ev->body.size > 1 ? b[1] : 0, ev->body.size > 2 ? b[2] : 0);
      }
    }
    if(has_audio_out)
    {
      auto& out = node.root_outputs()[0]->cast<ossia::audio_port>();
      if(trace)
      {
        double pk = 0;
        int nf = 0;
        for(std::size_t c = 0; c < out.channels(); c++)
          for(double v : out.channel(c))
            ossia::safe_isfinite(v) ? void(pk = std::max(pk, std::abs(v))) : void(nf++);
        std::fprintf(
            stderr, "TRACE frame %lld channels %zu peak %g nonfinite %d\n",
            (long long)frame, out.channels(), pk, nf);
      }
      for(std::size_t c = 0; c < out.channels(); c++)
      {
        auto& ch = out.channel(c);
        for(std::size_t k = 0; k < ch.size() && k < std::size_t(block); k++)
        {
          const double v = ch[k];
          res.audio.add(v, double(frame + k) / rate);
          if(c == 0 && in_channels > 0)
          {
            res.audio.compared_input = true;
            if(ossia::safe_isfinite(v))
              res.audio.max_diff_input = std::max(
                  res.audio.max_diff_input, std::abs(v - stim.audio(0, frame + k)));
          }
        }
      }
    }
    for(std::size_t i = 0; i < midi_out; i++)
    {
      auto& m = node.root_outputs()[out_base + i]->cast<ossia::midi_port>();
      res.midi_out += m.messages.size();
      m.messages.clear();
    }
    for(std::size_t i = 0; i < atom_out; i++)
    {
      auto& v = node.root_outputs()[out_base + midi_out + i]->cast<ossia::value_port>();
      res.atom_out += v.get_data().size();
      v.get_data().clear();
    }
    for(std::size_t i = 0; i < control_out; i++)
    {
      auto& v = node.root_outputs()[out_base + midi_out + atom_out + i]
                    ->cast<ossia::value_port>();
      for(auto& tv : v.get_data())
        res.controls.add(data.control_out_ports[i], ossia::convert<float>(tv.value));
      v.get_data().clear();
    }

    // What a worker thread prepares (a sampler loading its samples) arrives
    // while the audio plays: give it the time it would have
    if(pace)
    {
      const auto p0 = std::chrono::steady_clock::now();
      QThread::msleep(block * 1000 / rate);
      paced += std::chrono::steady_clock::now() - p0;
    }
  }
  res.seconds = double(total) / rate;
  res.wall_seconds
      = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0 - paced)
            .count();

  // As lv2_node_process::stop(): one more, empty, tick
  node.mustStop = true;
  ossia::exec_state_facade fac{&st};
  static_cast<ossia::graph_node&>(node).run(ossia::token_request{}, fac);
  return res;
}

//! A host written from the specification alone, sharing nothing with score's
//! but the features it gives: no transport, no voices, the port defaults
//! (lv2:sampleRate ones times the rate) and the default state's port values.
struct reference_host
{
  LV2::ApplicationPlugin& plug;
  const LilvPlugin* plugin;
  int rate;

  struct atom_buffer
  {
    std::vector<uint64_t> storage;
    LV2_Atom_Sequence* seq() { return reinterpret_cast<LV2_Atom_Sequence*>(storage.data()); }
    uint32_t capacity() const { return uint32_t(storage.size() * sizeof(uint64_t)); }
  };

  std::string run(const stimulus& stim, run_stats& res)
  {
    auto& h = plug.lv2_host_context;
    auto& g = *plug.lv2_context;

    std::vector<const LV2_Feature*> features;
    for(auto f = g.features(); f && *f; ++f)
      if(std::string_view{(*f)->URI} != LV2_WORKER__schedule)
        features.push_back(*f);
    features.push_back(nullptr);

    if(LilvNodes* req = lilv_plugin_get_required_features(plugin))
    {
      const bool worker = lilv_nodes_contains(req, h.work_schedule.me);
      lilv_nodes_free(req);
      if(worker)
        return "needs a worker";
    }

    LilvInstance* inst = lilv_plugin_instantiate(plugin, rate, features.data());
    if(!inst)
      return "instantiation failed";

    const uint32_t n = lilv_plugin_get_num_ports(plugin);
    std::vector<float> lo(n), hi(n), def(n);
    lilv_plugin_get_port_ranges_float(plugin, lo.data(), hi.data(), def.data());

    std::vector<float> controls(n, 0.f);
    std::vector<std::vector<float>> audio(n);
    std::vector<atom_buffer> atoms(n);
    std::vector<int> kind(n, 0); // 1 audio in, 2 audio out, 3 cv in, 4 cv out,
                                 // 5 control in, 6 control out, 7 atom in, 8 atom out
    std::vector<bool> midi(n, false);
    std::vector<float> scratch(block * 4, 0.f);
    LilvNode* sr_prop = lilv_new_uri(h.world.me, LV2_CORE__sampleRate);
    LilvNode* min_size = lilv_new_uri(h.world.me, LV2_RESIZE_PORT__minimumSize);
    int audio_in_index = 0;
    std::vector<int> in_channel(n, 0);
    for(uint32_t i = 0; i < n; i++)
    {
      const LilvPort* p = lilv_plugin_get_port_by_index(plugin, i);
      const bool in = lilv_port_is_a(plugin, p, h.input_class.me);
      if(lilv_port_is_a(plugin, p, h.audio_class.me))
      {
        kind[i] = in ? 1 : 2;
        if(in)
          in_channel[i] = audio_in_index++;
        audio[i].assign(block, 0.f);
        lilv_instance_connect_port(inst, i, audio[i].data());
      }
      else if(lilv_port_is_a(plugin, p, h.cv_class.me))
      {
        kind[i] = in ? 3 : 4;
        audio[i].assign(block, 0.f);
        lilv_instance_connect_port(inst, i, audio[i].data());
      }
      else if(lilv_port_is_a(plugin, p, h.control_class.me))
      {
        kind[i] = in ? 5 : 6;
        float d = ossia::safe_isnan(def[i]) ? (ossia::safe_isnan(lo[i]) ? 0.f : lo[i]) : def[i];
        if(in && lilv_port_has_property(plugin, p, sr_prop))
          d *= rate;
        controls[i] = d;
        lilv_instance_connect_port(inst, i, &controls[i]);
      }
      else if(lilv_port_is_a(plugin, p, h.atom_class.me))
      {
        kind[i] = in ? 7 : 8;
        midi[i] = lilv_port_supports_event(plugin, p, h.midi_event_class.me);
        uint32_t size = 65536;
        if(LilvNodes* sz = lilv_port_get_value(plugin, p, min_size))
        {
          if(lilv_nodes_size(sz) > 0)
            size = std::max<uint32_t>(
                size, lilv_node_as_int(lilv_nodes_get(sz, lilv_nodes_begin(sz))));
          lilv_nodes_free(sz);
        }
        atoms[i].storage.assign(size / sizeof(uint64_t) + 1, 0);
        lilv_instance_connect_port(inst, i, atoms[i].seq());
      }
      else
      {
        lilv_instance_connect_port(inst, i, scratch.data());
      }
    }
    lilv_node_free(sr_prop);
    lilv_node_free(min_size);

    // A plug-in without state:loadDefaultState has no default state to apply:
    // what lilv would build from its description holds the raw lv2:default of
    // each port
    LilvNodes* supported = lilv_plugin_get_supported_features(plugin);
    const bool default_state
        = supported && lilv_nodes_contains(supported, h.state_loadDefaultState_node.me);
    if(supported)
      lilv_nodes_free(supported);
    if(LilvState* state = default_state ? lilv_state_new_from_world(
                              h.world.me, &g.map, lilv_plugin_get_uri(plugin))
                                        : nullptr)
    {
      struct ctx_t
      {
        const LilvPlugin* plugin;
        LilvWorld* world;
        std::vector<float>* controls;
      } ctx{plugin, h.world.me, &controls};
      lilv_state_emit_port_values(
          state,
          [](const char* sym, void* ud, const void* v, uint32_t size, uint32_t) {
        auto& c = *static_cast<ctx_t*>(ud);
        if(size != sizeof(float))
          return;
        LilvNode* s = lilv_new_string(c.world, sym);
        if(const LilvPort* p = lilv_plugin_get_port_by_symbol(c.plugin, s))
          (*c.controls)[lilv_port_get_index(c.plugin, p)] = *static_cast<const float*>(v);
        lilv_node_free(s);
          },
          &ctx);
      lilv_state_free(state);
    }

    lilv_instance_activate(inst);
    const int64_t total = int64_t(play_seconds * rate);
    const auto t0 = std::chrono::steady_clock::now();
    for(int64_t frame = 0; frame < total; frame += block)
    {
      for(uint32_t i = 0; i < n; i++)
      {
        switch(kind[i])
        {
          case 1:
            for(int k = 0; k < block; k++)
              audio[i][k] = float(stim.audio(in_channel[i], frame + k));
            break;
          case 7: {
            auto* seq = atoms[i].seq();
            seq->atom.type = g.host.atom_sequence_id;
            seq->atom.size = sizeof(LV2_Atom_Sequence_Body);
            seq->body.unit = 0;
            seq->body.pad = 0;
            if(midi[i])
            {
              for(auto& e : stim.notes(frame, block))
              {
                struct
                {
                  LV2_Atom_Event ev;
                  uint8_t msg[3];
                } m{};
                m.ev.time.frames = e.frame;
                m.ev.body.type = g.host.midi_event_id;
                m.ev.body.size = 3;
                m.msg[0] = e.on ? 0x90 : 0x80;
                m.msg[1] = uint8_t(e.pitch);
                m.msg[2] = e.on ? 100 : 0;
                lv2_atom_sequence_append_event(seq, atoms[i].capacity(), &m.ev);
              }
            }
            break;
          }
          case 8: {
            auto* seq = atoms[i].seq();
            seq->atom.type = g.host.atom_chunk_id;
            seq->atom.size = atoms[i].capacity() - sizeof(LV2_Atom);
            break;
          }
          default:
            break;
        }
      }

      lilv_instance_run(inst, block);

      for(uint32_t i = 0; i < n; i++)
      {
        switch(kind[i])
        {
          case 2:
            for(int k = 0; k < block; k++)
              res.audio.add(audio[i][k], double(frame + k) / rate);
            break;
          case 6:
            res.controls.add(int(i), controls[i]);
            break;
          case 8: {
            auto* seq = atoms[i].seq();
            if(seq->atom.type == g.host.atom_sequence_id
               && seq->atom.size <= atoms[i].capacity() - sizeof(LV2_Atom))
            {
              LV2_ATOM_SEQUENCE_FOREACH(seq, ev)
              {
                if(midi[i] && ev->body.type == g.host.midi_event_id)
                  res.midi_out++;
                else
                  res.atom_out++;
              }
            }
            break;
          }
          default:
            break;
        }
      }
    }
    res.seconds = double(total) / rate;
    res.wall_seconds
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    lilv_instance_deactivate(inst);
    lilv_instance_free(inst);
    return {};
  }
};

std::vector<std::string> node_strings(const LilvNodes* nodes)
{
  std::vector<std::string> res;
  if(!nodes)
    return res;
  LILV_FOREACH(nodes, it, nodes)
  res.push_back(lilv_node_as_uri(lilv_nodes_get(nodes, it)));
  return res;
}

std::string role_of(const LV2::LV2Data& d, std::string_view cls)
{
  const bool ain = !d.audio_in_ports.empty();
  const bool aout = !d.audio_out_ports.empty();
  const bool min = !d.midi_in_ports.empty();
  const bool mout = !d.midi_out_ports.empty();
  const bool is_instrument = cls.find("Instrument") != std::string_view::npos;
  if(aout && min && (is_instrument || !ain))
    return "instrument";
  if(ain && aout)
    return "effect";
  if(aout && !d.cv_ports.empty())
    return "cv-generator";
  if(aout)
    return "generator";
  if(ain && !aout)
    return "analyzer";
  if(min && mout)
    return "midi-effect";
  if(mout)
    return "midi-generator";
  if(!d.cv_out_ports.empty())
    return "cv";
  if(!d.control_out_ports.empty())
    return "control";
  return "other";
}

void probe(
    LV2::ApplicationPlugin& plug, results_file& out, const std::string& uri,
    const QString& bundle)
{
  json j;
  j.str("type", "result").str("uri", uri).str("bundle", bundle.toStdString());

  out.phase(uri, "load");
  LV2::PluginInfo info;
  info.bundle = bundle;
  info.uri = QString::fromStdString(uri);
  info.valid = true;
  plug.setCachedDescriptors({info});
  auto found = LV2::find_lv2_plugin(plug.lilv, QString::fromStdString(uri));
  if(!found)
  {
    out.write(j.str("status", "not-found").done());
    return;
  }
  const LilvPlugin* lp = found->me;
  j.str("name", LV2::get_lv2_plugin_name(*found).toStdString());
  const LilvPluginClass* cls = lilv_plugin_get_class(lp);
  const std::string cls_label
      = cls ? lilv_node_as_string(lilv_plugin_class_get_label(cls)) : "";
  j.str("class", cls_label);

  // What the plug-in requires that score does not give
  std::set<std::string> given;
  for(auto f = plug.lv2_context->features(); f && *f; ++f)
    given.insert((*f)->URI);
  LilvNodes* req = lilv_plugin_get_required_features(lp);
  const auto required = node_strings(req);
  if(req)
    lilv_nodes_free(req);
  std::vector<std::string> missing;
  for(auto& r : required)
    if(!given.contains(r))
      missing.push_back(r);
  j.strings("required_features", required);
  j.strings("missing_features", missing);

  out.phase(uri, "model");
  QObject parent;
  const auto t0 = std::chrono::steady_clock::now();
  auto* model = new LV2::Model{
      TimeVal::fromMsecs(10000), QString::fromStdString(uri),
      Id<Process::ProcessModel>{1}, &parent};
  j.num(
      "instantiate_ms",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
          .count());
  if(!model->plugin || !model->effectContext.instance)
  {
    out.write(j.str("status", "instantiate-failed").done());
    return;
  }

  LV2::LV2Data data{plug.lv2_host_context, model->effectContext};
  const auto role = role_of(data, cls_label);
  j.str("role", role);
  {
    std::ostringstream ports;
    ports << "{\"audio_in\":" << data.audio_in_ports.size()
          << ",\"audio_out\":" << data.audio_out_ports.size()
          << ",\"cv_in\":" << data.cv_ports.size()
          << ",\"cv_out\":" << data.cv_out_ports.size()
          << ",\"midi_in\":" << data.midi_in_ports.size()
          << ",\"midi_out\":" << data.midi_out_ports.size()
          << ",\"atom_in\":" << data.atom_in_ports.size()
          << ",\"atom_out\":" << data.atom_out_ports.size()
          << ",\"control_in\":" << data.control_in_ports.size()
          << ",\"control_out\":" << data.control_out_ports.size()
          << ",\"time_position\":" << data.time_Position_ports.size() << "}";
    j.raw("ports", ports.str());
  }
  const bool worker
      = lilv_plugin_has_extension_data(lp, plug.lv2_host_context.work_interface.me);
  j.boolean("worker", worker);

  // A process is created some time before it plays: plug-ins loading on a
  // thread of their own (Geonkick's kit) are ready by then
  QThread::msleep(settle_ms);
  QCoreApplication::processEvents();

  const int rate = plug.lv2_context->sampleRate;
  j.num("rate", rate);
  stimulus stim{rate};

  out.phase(uri, "play");
  const auto first = play(*model, data, stim, play_seconds, worker);
  first.write(j, "play_");
  QCoreApplication::processEvents();

  out.phase(uri, "replay");
  const auto second = play(*model, data, stim, replay_seconds, false);
  second.write(j, "replay_");
  QCoreApplication::processEvents();

  out.phase(uri, "reference");
  run_stats ref;
  reference_host ref_host{plug, lp, rate};
  const auto ref_skip = ref_host.run(stim, ref);
  if(ref_skip.empty())
    ref.write(j, "ref_");
  else
    j.str("ref_skipped", ref_skip);

  out.phase(uri, "teardown");
  delete model;
  QCoreApplication::processEvents();
  out.write(j.str("status", "ran").done());
}

void probe_live(
    const score::GUIApplicationContext& ctx, LV2::ApplicationPlugin& plug,
    results_file& out, const std::string& uri, const QString& bundle)
{
  json j;
  j.str("type", "result").str("uri", uri).str("bundle", bundle.toStdString());
  j.str("mode", "live");

  out.phase(uri, "load");
  LV2::PluginInfo info;
  info.bundle = bundle;
  info.uri = QString::fromStdString(uri);
  info.valid = true;
  plug.setCachedDescriptors({info});
  auto found = LV2::find_lv2_plugin(plug.lilv, QString::fromStdString(uri));
  if(!found)
  {
    out.write(j.str("status", "not-found").done());
    return;
  }
  j.str("name", LV2::get_lv2_plugin_name(*found).toStdString());
  {
    // What fails to instantiate fails every phase alike: the usual probe
    // tells it
    QObject parent;
    auto* model = new LV2::Model{
        TimeVal::fromMsecs(10000), QString::fromStdString(uri),
        Id<Process::ProcessModel>{1}, &parent};
    if(!model->plugin || !model->effectContext.instance)
    {
      out.write(j.str("status", "instantiate-failed").done());
      return;
    }
  }

  score::test::live::options opt;
  opt.what = {Metadata<ConcreteKey_k, LV2::Model>::get(), {}, QString::fromStdString(uri)};
  opt.only = score::test::live::requested_phases();
  score::test::live::verdicts res;
  score::test::live::recorder rec;
  rec.phase = [&](std::string_view phase) { out.phase(uri, std::string{phase}.c_str()); };
  rec.result = [&](std::string_view phase, std::string_view verdict, std::string_view detail) {
    res.add(phase, verdict, detail);
  };
  score::test::live::run(ctx, opt, rec);
  j.raw("live", res.json());
  out.write(j.str("status", "ran").done());
}

void list_plugins(const QString& path)
{
  LilvWorld* world = lilv_world_new();
  lilv_world_load_all(world);
  std::ofstream f{path.toStdString()};
  const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
  LILV_FOREACH(plugins, it, plugins)
  {
    const LilvPlugin* p = lilv_plugins_get(plugins, it);
    LilvNode* name = lilv_plugin_get_name(p);
    const LilvPluginClass* cls = lilv_plugin_get_class(p);
    f << lilv_node_as_uri(lilv_plugin_get_uri(p)) << '\t'
      << lilv_node_as_uri(lilv_plugin_get_bundle_uri(p)) << '\t'
      << (name ? lilv_node_as_string(name) : "") << '\t'
      << (cls ? lilv_node_as_string(lilv_plugin_class_get_label(cls)) : "") << '\n';
    if(name)
      lilv_node_free(name);
  }
  lilv_world_free(world);
}
}

TEST_CASE("LV2 system sweep worker", "[lv2][sweep]")
{
  const QString list = qEnvironmentVariable("SCORE_LV2_SWEEP_LIST");
  if(!list.isEmpty())
  {
    list_plugins(list);
    return;
  }

  const QString jobs = qEnvironmentVariable("SCORE_LV2_SWEEP_JOBS");
  const QString results = qEnvironmentVariable("SCORE_LV2_SWEEP_RESULTS");
  if(jobs.isEmpty() || results.isEmpty())
    return;

  QFile f{jobs};
  REQUIRE(f.open(QIODevice::ReadOnly));
  std::vector<std::pair<std::string, QString>> todo;
  for(const auto& line : QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts))
  {
    const auto parts = line.split('\t');
    if(parts.size() >= 2)
      todo.emplace_back(parts[0].toStdString(), parts[1]);
  }

  // No scanner processes: the bundles are loaded one by one
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
    plug.lv2_context->loadPlugins();
    results_file out{results};
    const bool live = score::test::live::requested();
    for(auto& [uri, bundle] : todo)
    {
      if(live)
        probe_live(ctx, plug, out, uri, bundle);
      else
        probe(plug, out, uri, bundle);
    }
  });
}
