// The worker of the VST3 system sweep (vst3_sweep.py drives it): every VST3
// plug-in installed on the machine, run through score's host with a stimulus
// fitting what it is (an instrument gets notes, an effect gets a signal), then
// through a minimal host of its own written against the SDK's hosting classes
// only. A plug-in silent in score but not in that host points at score; silent
// in both, at the plug-in or at what it needs and is not given.
//
// The worker measures and the driver judges: thresholds live in vst3_sweep.py.
//
//  SCORE_VST3_SWEEP_SCAN=<file>    the bundles of these lines (one path per
//                                  line) are loaded one by one, and their audio
//                                  classes appended to SCORE_VST3_SWEEP_RESULTS,
//                                  one JSON "result" record per bundle.
//  SCORE_VST3_SWEEP_JOBS=<file>    probe the plug-ins of these lines (bundle \t
//                                  class UID), appending one JSON object per
//                                  line to SCORE_VST3_SWEEP_RESULTS: a "phase"
//                                  record before each step, so that a crash or
//                                  a hang can be told where it happened, then a
//                                  "result" record.
//
//  SCORE_VST3_SWEEP_TRACE=1        print, on stderr, the peak of every block,
//                                  the process context and the events of both
//                                  hosts, to look closer at one.
//  SCORE_VST3_SWEEP_REF_INPLACE=1  the reference host writes its main output
//                                  over its main input, as score's float path
//                                  does.
//  SCORE_VST3_SWEEP_REF_MAX_BLOCK=n the reference host sets the plug-in up for
//                                  blocks of up to n samples, still playing
//                                  blocks of 512.
//  SCORE_VST3_SWEEP_REF_ON_SCORE=1  the reference host also plays score's own
//                                  instance once score is done with it: a
//                                  difference that stays is in how score set
//                                  the instance up, not in how it processes.
//
// Neither set, the test case has nothing to do and passes: ctest runs the
// driver, not this.

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Audio/Settings/Model.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/EffectModel.hpp>
#include <Vst3/Node.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QThread>

#include <libremidi/detail/conversion.hpp>
#include <libremidi/ump_events.hpp>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>
#include <public.sdk/source/vst/hosting/processdata.h>
#include <public.sdk/source/vst/utility/memoryibstream.h>

namespace
{
using namespace Steinberg;
using namespace Steinberg::Vst;

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
  double seconds{play_seconds};
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
    if(t >= seconds - 0.1)
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

//! The values the plug-in reported for its parameters during a run, by id
struct output_param_stats
{
  std::map<ParamID, std::pair<double, double>> range;
  int64_t nonfinite{};
  int64_t out_of_range{};
  void add(ParamID id, double v)
  {
    if(!ossia::safe_isfinite(v))
    {
      nonfinite++;
      return;
    }
    if(v < -1e-6 || v > 1. + 1e-6)
      out_of_range++;
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
  output_param_stats params;
  int64_t events_out{};
  int64_t bad_events_in{};
  double seconds{};
  double wall_seconds{};

  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    audio.write(j, prefix);
    j.num(k("params_changed"), params.changed());
    j.num(k("params_reported"), double(params.range.size()));
    j.num(k("params_nonfinite"), double(params.nonfinite));
    j.num(k("params_out_of_range"), double(params.out_of_range));
    j.num(k("events_out"), double(events_out));
    j.num(k("bad_events_in"), double(bad_events_in));
    j.num(k("rt_factor"), seconds > 0 ? wall_seconds / seconds : 0.);
  }
};

//! As a MIDI 1 device's note reaches score: upscaled to MIDI 2
libremidi::ump midi2_note(bool on, int pitch)
{
  return on ? libremidi::from_midi1::note_on(0, pitch, 100)
            : libremidi::from_midi1::note_off(0, pitch, 0);
}

struct bus_layout
{
  std::vector<int> audio_in, audio_out;
  int event_in{}, event_out{};
  std::vector<bool> audio_in_active, audio_out_active;

  explicit bus_layout(IComponent& c)
  {
    BusInfo info{};
    for(int i = 0; i < c.getBusCount(kAudio, kInput); i++)
    {
      info = {};
      c.getBusInfo(kAudio, kInput, i, info);
      audio_in.push_back(info.channelCount);
      audio_in_active.push_back(info.flags & BusInfo::kDefaultActive);
    }
    for(int i = 0; i < c.getBusCount(kAudio, kOutput); i++)
    {
      info = {};
      c.getBusInfo(kAudio, kOutput, i, info);
      audio_out.push_back(info.channelCount);
      audio_out_active.push_back(info.flags & BusInfo::kDefaultActive);
    }
    event_in = c.getBusCount(kEvent, kInput);
    event_out = c.getBusCount(kEvent, kOutput);
  }
  int first_audio_in_channels() const { return audio_in.empty() ? 0 : audio_in[0]; }
  int first_audio_out_channels() const { return audio_out.empty() ? 0 : audio_out[0]; }
};

void trace_context(const char* who, int64_t frame, const ProcessContext& c, const ProcessData& d)
{
  if(!qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_TRACE") || frame > 1024)
    return;
  std::fprintf(
      stderr,
      "TRACE %s ctx frame %lld state %x sr %g pts %lld sys %lld cts %lld ptm %g bar %g "
      "tempo %g sig %d/%d | mode %d size %d n %d ins %d outs %d\n",
      who, (long long)frame, c.state, c.sampleRate, (long long)c.projectTimeSamples,
      (long long)c.systemTime, (long long)c.continousTimeSamples, c.projectTimeMusic,
      c.barPositionMusic, c.tempo, c.timeSigNumerator, c.timeSigDenominator,
      d.processMode, d.symbolicSampleSize, d.numSamples, d.numInputs, d.numOutputs);
  for(int b = 0; b < d.numInputs; b++)
    std::fprintf(stderr, "TRACE %s   in %d: %d ch, silence %llx\n", who, b, d.inputs[b].numChannels, (unsigned long long)d.inputs[b].silenceFlags);
  for(int b = 0; b < d.numOutputs; b++)
    std::fprintf(stderr, "TRACE %s   out %d: %d ch, silence %llx\n", who, b, d.outputs[b].numChannels, (unsigned long long)d.outputs[b].silenceFlags);
}

//! The events with fields the host has no reason to set: flags beyond
//! kIsLive, a note length, a tuning; or a channel out of 0-15
int64_t bad_events(IEventList& events)
{
  int64_t n = 0;
  for(int32 i = 0; i < events.getEventCount(); i++)
  {
    Event e{};
    if(events.getEvent(i, e) != kResultOk)
      continue;
    bool bad = (e.flags & ~Event::kIsLive) != 0;
    if(e.type == Event::kNoteOnEvent)
      bad |= e.noteOn.length != 0 || e.noteOn.tuning != 0.f || e.noteOn.channel < 0
             || e.noteOn.channel > 15;
    else if(e.type == Event::kNoteOffEvent)
      bad |= e.noteOff.tuning != 0.f || e.noteOff.channel < 0 || e.noteOff.channel > 15;
    n += bad;
  }
  return n;
}

void trace_events(const char* who, int64_t frame, IEventList& events)
{
  for(int32 i = 0; i < events.getEventCount(); i++)
  {
    Event e{};
    events.getEvent(i, e);
    std::fprintf(
        stderr,
        "TRACE %s ev frame %lld bus %d off %d ppq %g flags %x type %d ch %d pitch %d vel %g "
        "len %d tun %g id %d\n",
        who, (long long)frame, e.busIndex, e.sampleOffset, e.ppqPosition, e.flags, e.type,
        e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity, e.noteOn.length,
        e.noteOn.tuning, e.noteOn.noteId);
  }
}

//! One play of score's host: a node made as vst3::Executor makes it, with the
//! controls of the process, then stopped as ossia::node_process::stop() does.
template <bool UseDouble>
run_stats play_t(
    vst3::Model& model, const stimulus& stim, double seconds, results_file& out,
    const std::string& uri, const char* stop_phase)
{
  run_stats res;
  res.audio.seconds = seconds;
  const int rate = stim.rate;
  ossia::execution_state st;
  st.sampleRate = rate;
  st.bufferSize = block;
  // The dates of the ticks are in flicks, as the engine's
  st.modelToSamplesRatio = rate / ossia::flicks_per_second<double>;
  st.samplesToModelRatio = ossia::flicks_per_second<double> / rate;

  auto node = std::make_shared<vst3::vst_node<UseDouble>>(model.fx, rate);
  for(auto* inlet : model.inlets())
    // vst3::ControlInlet is not exported, and is the only control the
    // process has
    if(qobject_cast<Process::ControlInlet*>(inlet))
    {
      auto* ctrl = static_cast<vst3::ControlInlet*>(inlet);
      node->add_control(new ossia::value_inlet, ctrl->fxNum, ctrl->value());
    }

  const auto& ins = node->m_audioInputChannels;
  const auto& outs = node->m_audioOutputChannels;
  const int event_ins = node->m_totalEventIns;
  const int event_outs = node->m_totalEventOuts;
  const bool trace = qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_TRACE");

  const int64_t total = int64_t(seconds * rate);
  const double flicks_per_frame = ossia::flicks_per_second<double> / rate;
  const auto t0 = std::chrono::steady_clock::now();
  for(int64_t frame = 0; frame < total; frame += block)
  {
    std::size_t inl = 0;
    for(std::size_t b = 0; b < ins.size(); b++)
    {
      auto& a = node->root_inputs()[inl++]->template cast<ossia::audio_port>();
      a.set_channels(ins[b]);
      for(int c = 0; c < ins[b]; c++)
      {
        a.channel(c).resize(block);
        for(int k = 0; k < block; k++)
          a.channel(c)[k] = b == 0 ? stim.audio(c, frame + k) : 0.;
      }
    }
    for(int i = 0; i < event_ins; i++)
    {
      auto& m = node->root_inputs()[inl++]->template cast<ossia::midi_port>();
      m.messages.clear();
      for(auto& n : stim.notes(frame, block))
      {
        libremidi::ump u = midi2_note(n.on, n.pitch);
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
    // 120 BPM in 4/4, as the reference host says
    tk.musical_start_position = double(frame) / rate * 2.;
    tk.musical_end_position = double(frame + block) / rate * 2.;
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    static_cast<ossia::graph_node&>(*node).run(tk, ossia::exec_state_facade{&st});
    node->process_time(tk, st);
    trace_context("score", frame, node->m_context, node->m_vstData);
    if(qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_TRACE"))
      trace_events("score", frame, node->m_inputEvents);
    res.bad_events_in += bad_events(node->m_inputEvents);
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    for(auto* in : node->root_inputs())
      if(auto* v = in->template target<ossia::value_port>())
        v->clear();

    for(std::size_t b = 0; b < outs.size(); b++)
    {
      auto& port = node->root_outputs()[b]->template cast<ossia::audio_port>();
      if(trace && b == 0)
      {
        double pk = 0;
        int nf = 0;
        for(std::size_t c = 0; c < port.channels(); c++)
          for(double v : port.channel(c))
            ossia::safe_isfinite(v) ? void(pk = std::max(pk, std::abs(v))) : void(nf++);
        std::fprintf(
            stderr, "TRACE frame %lld channels %zu peak %g nonfinite %d\n",
            (long long)frame, port.channels(), pk, nf);
        if(frame == 0)
          for(std::size_t c = 0; c < port.channels(); c++)
            std::fprintf(stderr, "TRACE score out %d: %g %g %g %g\n", int(c), port.channel(c)[0], port.channel(c)[1], port.channel(c)[100], port.channel(c)[511]);
      }
      // The main bus is what the process outputs; the others are counted for
      // NaN only
      for(std::size_t c = 0; c < port.channels(); c++)
      {
        auto& ch = port.channel(c);
        for(std::size_t k = 0; k < ch.size() && k < std::size_t(block); k++)
        {
          const double v = ch[k];
          if(b != 0)
          {
            if(!ossia::safe_isfinite(v))
              res.audio.nonfinite++;
            continue;
          }
          res.audio.add(v, double(frame + k) / rate);
          if(c == 0 && !ins.empty() && ins[0] > 0 && ossia::safe_isfinite(v))
          {
            res.audio.compared_input = true;
            res.audio.max_diff_input = std::max(
                res.audio.max_diff_input, std::abs(v - stim.audio(0, frame + k)));
          }
        }
      }
    }
    for(int i = 0; i < event_outs; i++)
    {
      auto& m
          = node->root_outputs()[outs.size() + i]->template cast<ossia::midi_port>();
      res.events_out += m.messages.size();
      m.messages.clear();
    }
    node->consumeOutputValues([&](ParamID id, ParamValue v) { res.params.add(id, v); });
  }
  res.seconds = double(total) / rate;
  res.wall_seconds
      = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

  // As the transport does on stop: the process stops, which sends the notes
  // off, then the execution is torn down
  out.phase(uri, stop_phase);
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  node->all_notes_off();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  node.reset();
  return res;
}

run_stats play(
    vst3::Model& model, const stimulus& stim, double seconds, results_file& out,
    const std::string& uri, const char* stop_phase)
{
  return model.fx.supports_double
             ? play_t<true>(model, stim, seconds, out, uri, stop_phase)
             : play_t<false>(model, stim, seconds, out, uri, stop_phase);
}

//! A digest of the processor's state, to tell whether two instances start
//! from the same one
std::string state_digest(IComponent& comp)
{
  ResizableMemoryIBStream st;
  if(comp.getState(&st) != kResultOk)
    return "none";
  int64 size{};
  st.tell(&size);
  st.rewind();
  std::vector<char> bytes(size);
  int32 read{};
  if(size > 0)
    st.read(bytes.data(), int32(size), &read);
  uint64_t h = 1469598103934665603ull;
  for(char c : bytes)
    h = (h ^ uint8_t(c)) * 1099511628211ull;
  return std::to_string(size) + ":" + std::to_string(h);
}

//! A host written against the SDK's hosting classes, sharing nothing with
//! score's but the plug-in factory: the call sequence of the SDK's
//! documentation, the buses active by default, the plug-in's own default
//! parameter values, and the sample size score picks.
struct reference_host
{
  const VST3::Hosting::PluginFactory& factory;
  VST3::UID uid;
  int rate;
  bool use_double;
  std::string state{};

  template <typename T>
  static T* query(FUnknown* o)
  {
    T* res{};
    if(o && o->queryInterface(T::iid, (void**)&res) == kResultOk && res)
      return res;
    return nullptr;
  }

  //! Plays the stimulus through an active instance, as this host does
  void play_on(
      IComponent& comp_ref, IAudioProcessor& proc_ref, IEditController* ctl,
      int event_in, int32 sample_size, const stimulus& stim, run_stats& res)
  {
    auto* comp = &comp_ref;
    auto* proc = &proc_ref;
    struct
    {
      int event_in;
    } buses{event_in};
    proc->setProcessing(true);

    HostProcessData data;
    data.prepare(*comp, block, sample_size);
    data.processMode = kRealtime;
    data.numSamples = block;
    EventList in_events{512};
    EventList out_events{512};
    ParameterChanges in_params{0};
    ParameterChanges out_params{ctl ? std::max(1, int(ctl->getParameterCount())) : 64};
    ProcessContext ctx{};
    data.inputEvents = &in_events;
    data.outputEvents = &out_events;
    data.inputParameterChanges = &in_params;
    data.outputParameterChanges = &out_params;
    data.processContext = &ctx;

    auto each_channel = [&](AudioBusBuffers& bus, int c, auto&& f) {
      if(use_double)
      {
        if(bus.channelBuffers64 && bus.channelBuffers64[c])
          f(bus.channelBuffers64[c]);
      }
      else if(bus.channelBuffers32 && bus.channelBuffers32[c])
        f(bus.channelBuffers32[c]);
    };

    // To look closer at one: the main output written over the main input,
    // as a host processing in place does
    std::vector<void*> own_outputs;
    if(qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_REF_INPLACE") && data.numInputs > 0
       && data.numOutputs > 0)
    {
      auto& in = data.inputs[0];
      auto& o = data.outputs[0];
      for(int c = 0; c < std::min(in.numChannels, o.numChannels); c++)
      {
        own_outputs.push_back(o.channelBuffers32[c]);
        o.channelBuffers32[c] = in.channelBuffers32[c];
      }
    }

    const int64_t total = int64_t(play_seconds * rate);
    const auto t0 = std::chrono::steady_clock::now();
    for(int64_t frame = 0; frame < total; frame += block)
    {
      for(int b = 0; b < data.numOutputs; b++)
        for(int c = 0; c < data.outputs[b].numChannels; c++)
          each_channel(data.outputs[b], c, [&](auto* buf) {
            std::fill_n(buf, block, 0);
          });
      for(int b = 0; b < data.numInputs; b++)
        for(int c = 0; c < data.inputs[b].numChannels; c++)
          each_channel(data.inputs[b], c, [&](auto* buf) {
            for(int k = 0; k < block; k++)
              buf[k] = b == 0 ? stim.audio(c, frame + k) : 0.;
          });
      for(int b = 0; b < data.numInputs; b++)
        data.inputs[b].silenceFlags = 0;
      for(int b = 0; b < data.numOutputs; b++)
        data.outputs[b].silenceFlags = 0;

      in_events.clear();
      out_events.clear();
      out_params.clearQueue();
      if(buses.event_in > 0)
      {
        for(auto& n : stim.notes(frame, block))
        {
          Event e{};
          e.busIndex = 0;
          e.sampleOffset = int32(n.frame);
          if(n.on)
          {
            e.type = Event::kNoteOnEvent;
            e.noteOn.channel = 0;
            e.noteOn.pitch = int16(n.pitch);
            e.noteOn.velocity = 100.f / 127.f;
            e.noteOn.noteId = -1;
          }
          else
          {
            e.type = Event::kNoteOffEvent;
            e.noteOff.channel = 0;
            e.noteOff.pitch = int16(n.pitch);
            e.noteOff.velocity = 0.f;
            e.noteOff.noteId = -1;
          }
          in_events.addEvent(e);
        }
      }

      ctx.state = ProcessContext::kPlaying | ProcessContext::kTempoValid
                  | ProcessContext::kTimeSigValid | ProcessContext::kProjectTimeMusicValid
                  | ProcessContext::kContTimeValid | ProcessContext::kBarPositionValid;
      ctx.sampleRate = rate;
      ctx.projectTimeSamples = frame;
      ctx.continousTimeSamples = frame;
      ctx.tempo = 120.;
      ctx.timeSigNumerator = 4;
      ctx.timeSigDenominator = 4;
      ctx.projectTimeMusic = double(frame) / rate * 2.;
      ctx.barPositionMusic = std::floor(ctx.projectTimeMusic / 4.) * 4.;

      proc->process(data);
      trace_context("ref", frame, ctx, data);
      if(qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_TRACE"))
        trace_events("ref", frame, in_events);

      if(data.numOutputs > 0 && qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_TRACE"))
      {
        double pk = 0;
        auto& bus = data.outputs[0];
        for(int c = 0; c < bus.numChannels; c++)
          each_channel(bus, c, [&](auto* buf) {
            for(int k = 0; k < block; k++)
              pk = std::max(pk, std::abs(double(buf[k])));
          });
        std::fprintf(stderr, "TRACE ref frame %lld peak %g\n", (long long)frame, pk);
        if(frame == 0)
          for(int c = 0; c < bus.numChannels; c++)
            each_channel(bus, c, [&](auto* buf) {
              std::fprintf(stderr, "TRACE ref out %d: %g %g %g %g\n", c, double(buf[0]), double(buf[1]), double(buf[100]), double(buf[511]));
            });
      }
      if(data.numOutputs > 0)
      {
        auto& bus = data.outputs[0];
        for(int c = 0; c < bus.numChannels; c++)
          each_channel(bus, c, [&](auto* buf) {
            for(int k = 0; k < block; k++)
              res.audio.add(double(buf[k]), double(frame + k) / rate);
          });
      }
      res.events_out += out_events.getEventCount();
      for(int32 i = 0; i < out_params.getParameterCount(); i++)
      {
        if(auto* q = out_params.getParameterData(i))
        {
          const int32 n = q->getPointCount();
          if(n <= 0)
            continue;
          int32 off{};
          ParamValue v{};
          q->getPoint(n - 1, off, v);
          res.params.add(q->getParameterId(), v);
        }
      }
    }
    res.seconds = double(total) / rate;
    res.wall_seconds
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    proc->setProcessing(false);
    for(std::size_t c = 0; c < own_outputs.size(); c++)
      data.outputs[0].channelBuffers32[c] = (Sample32*)own_outputs[c];
    data.unprepare();
  }

  std::string run(const stimulus& stim, run_stats& res)
  {
    // Never released: a plug-in keeping a reference past terminate() is not
    // what this host is here to find
    static auto* host = new HostApplication;
    IComponent* comp{};
    if(factory.get()->createInstance(
           uid.data(), IComponent::iid, reinterpret_cast<void**>(&comp))
           != kResultOk
       || !comp)
      return "createInstance failed";
    if(comp->initialize(host) != kResultOk)
    {
      comp->release();
      return "initialize failed";
    }
    IAudioProcessor* proc = query<IAudioProcessor>(comp);
    if(!proc)
    {
      comp->terminate();
      comp->release();
      return "no IAudioProcessor";
    }

    IEditController* ctl = query<IEditController>(comp);
    bool separate_ctl = false;
    if(!ctl)
    {
      TUID cid;
      if(comp->getControllerClassId(cid) == kResultTrue)
      {
        if(factory.get()->createInstance(cid, IEditController::iid, (void**)&ctl)
               == kResultOk
           && ctl)
        {
          separate_ctl = true;
          if(ctl->initialize(host) != kResultOk)
          {
            ctl->release();
            ctl = nullptr;
            separate_ctl = false;
          }
        }
        else
          ctl = nullptr;
      }
    }
    IConnectionPoint* comp_cp{};
    IConnectionPoint* ctl_cp{};
    if(separate_ctl)
    {
      comp_cp = query<IConnectionPoint>(comp);
      ctl_cp = query<IConnectionPoint>(ctl);
      if(comp_cp && ctl_cp)
      {
        comp_cp->connect(ctl_cp);
        ctl_cp->connect(comp_cp);
      }
      ResizableMemoryIBStream state;
      if(comp->getState(&state) == kResultOk)
      {
        state.rewind();
        ctl->setComponentState(&state);
      }
    }

    state = state_digest(*comp);
    const int32 sample_size = use_double ? kSample64 : kSample32;
    // To look closer at one: another largest block than the one played
    const int32 max_block = qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_REF_MAX_BLOCK")
                                ? qEnvironmentVariableIntValue("SCORE_VST3_SWEEP_REF_MAX_BLOCK")
                                : block;
    ProcessSetup setup{kRealtime, sample_size, max_block, double(rate)};
    std::string err;
    if(proc->setupProcessing(setup) != kResultOk)
      err = "setupProcessing failed";

    bus_layout buses{*comp};
    if(err.empty())
    {
      for(std::size_t i = 0; i < buses.audio_in.size(); i++)
        if(buses.audio_in_active[i])
          comp->activateBus(kAudio, kInput, i, true);
      for(std::size_t i = 0; i < buses.audio_out.size(); i++)
        if(buses.audio_out_active[i])
          comp->activateBus(kAudio, kOutput, i, true);
      BusInfo info{};
      for(int i = 0; i < buses.event_in; i++)
        if(comp->getBusInfo(kEvent, kInput, i, info) == kResultOk
           && (info.flags & BusInfo::kDefaultActive))
          comp->activateBus(kEvent, kInput, i, true);
      for(int i = 0; i < buses.event_out; i++)
        if(comp->getBusInfo(kEvent, kOutput, i, info) == kResultOk
           && (info.flags & BusInfo::kDefaultActive))
          comp->activateBus(kEvent, kOutput, i, true);

      if(comp->setActive(true) != kResultOk)
        err = "setActive failed";
    }

    if(err.empty())
    {
      play_on(*comp, *proc, ctl, buses.event_in, sample_size, stim, res);
      comp->setActive(false);
    }

    if(comp_cp && ctl_cp)
    {
      comp_cp->disconnect(ctl_cp);
      ctl_cp->disconnect(comp_cp);
    }
    if(comp_cp)
      comp_cp->release();
    if(ctl_cp)
      ctl_cp->release();
    if(ctl)
    {
      if(separate_ctl)
        ctl->terminate();
      ctl->release();
    }
    proc->release();
    comp->terminate();
    comp->release();
    return err;
  }
};

std::string role_of(const bus_layout& b, std::string_view subcategories)
{
  const bool ain = b.first_audio_in_channels() > 0;
  const bool aout = b.first_audio_out_channels() > 0;
  const bool min = b.event_in > 0;
  const bool mout = b.event_out > 0;
  const bool is_instrument = subcategories.find("Instrument") != std::string_view::npos;
  if(aout && min && (is_instrument || !ain))
    return "instrument";
  if(ain && aout)
    return "effect";
  if(aout)
    return "generator";
  if(ain && !aout)
    return "analyzer";
  if(min && mout)
    return "midi-effect";
  if(mout)
    return "midi-generator";
  return "other";
}

std::string int_list(const std::vector<int>& v)
{
  std::string s = "[";
  for(std::size_t i = 0; i < v.size(); i++)
    s += (i ? "," : "") + std::to_string(v[i]);
  return s + "]";
}

vst3::AvailablePlugin
available_plugin(const QString& bundle, const VST3::Hosting::Module& mdl)
{
  vst3::AvailablePlugin p;
  p.path = bundle;
  p.name = QString::fromStdString(mdl.getName());
  for(const auto& cls : mdl.getFactory().classInfos())
    if(cls.category() == kVstAudioEffectClass)
      p.classInfo.push_back(cls);
  p.isValid = !p.classInfo.empty();
  return p;
}

void probe(
    const score::GUIApplicationContext& ctx, score::Document& doc,
    vst3::ApplicationPlugin& plug, results_file& out, const QString& bundle,
    const std::string& uid_str)
{
  const std::string uri = bundle.toStdString() + "::" + uid_str;
  json j;
  j.str("type", "result").str("uri", uri).str("bundle", bundle.toStdString());
  j.str("uid", uid_str);

  out.phase(uri, "load");
  auto uid = VST3::UID::fromString(uid_str);
  auto mdl = plug.getModule(bundle.toStdString());
  if(!mdl || !uid)
  {
    out.write(j.str("status", "not-found").done());
    return;
  }
  plug.vst_infos = {available_plugin(bundle, *mdl)};
  std::string subcategories;
  for(const auto& cls : mdl->getFactory().classInfos())
    if(cls.ID() == *uid)
    {
      j.str("name", cls.name());
      j.str("vendor", cls.vendor());
      j.str("sdk", cls.sdkVersion());
      subcategories = cls.subCategoriesString();
    }
  j.str("subcategories", subcategories);

  out.phase(uri, "model");
  const auto t0 = std::chrono::steady_clock::now();
  // In the document, as the library puts it there: a plug-in may change its
  // parameters while it loads, which score records as commands on the process
  auto& interval = score::test::base_interval(doc);
  const auto key = Metadata<ConcreteKey_k, vst3::Model>::get();
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::Command::AddOnlyProcessToInterval>(
          interval, key, QString::fromStdString(uid_str), QPointF{});
  // The probes before this one removed theirs
  auto found = ossia::find_if(
      interval.processes, [&](const auto& p) { return p.concreteKey() == key; });
  if(found == interval.processes.end())
  {
    out.write(j.str("status", "instantiate-failed").done());
    return;
  }
  const auto id = found->id();
  // vst3::Model is not exported: no qobject_cast
  auto* model = static_cast<vst3::Model*>(const_cast<Process::ProcessModel*>(&*found));
  const auto remove = [&] {
    CommandDispatcher<>{doc.context().commandStack}
        .submit<Scenario::Command::RemoveProcessFromInterval>(interval, id);
  };
  j.num(
      "instantiate_ms",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
          .count());
  if(!model->fx)
  {
    remove();
    out.write(j.str("status", "instantiate-failed").done());
    return;
  }

  bus_layout buses{*model->fx.component};
  const auto role = role_of(buses, subcategories);
  j.str("role", role);
  int controls = 0;
  for(auto* inlet : model->inlets())
    controls += bool(qobject_cast<Process::ControlInlet*>(inlet));
  {
    std::ostringstream ports;
    ports << "{\"audio_in\":" << int_list(buses.audio_in)
          << ",\"audio_out\":" << int_list(buses.audio_out)
          << ",\"event_in\":" << buses.event_in << ",\"event_out\":" << buses.event_out
          << ",\"params\":"
          << (model->fx.controller ? model->fx.controller->getParameterCount() : -1)
          << ",\"controls\":" << controls
          << ",\"midi_mapped\":" << model->fx.midiControls.size() << "}";
    j.raw("ports", ports.str());
  }
  j.boolean("double", model->fx.supports_double);
  j.num("max_block", model->fx.max_block);
  j.num("setup_rate", model->fx.sample_rate);
  j.str("state", state_digest(*model->fx.component));
  j.boolean("controller", model->fx.controller != nullptr);
  j.boolean("view", model->fx.view != nullptr);

  // A process is created some time before it plays: plug-ins loading on a
  // thread of their own are ready by then
  QThread::msleep(settle_ms);
  QCoreApplication::processEvents();

  const int rate = ctx.settings<Audio::Settings::Model>().getRate();
  j.num("rate", rate);
  stimulus stim{rate};

  out.phase(uri, "play");
  const auto first = play(*model, stim, play_seconds, out, uri, "stop");
  first.write(j, "play_");
  QCoreApplication::processEvents();

  out.phase(uri, "replay");
  const auto second = play(*model, stim, replay_seconds, out, uri, "restop");
  second.write(j, "replay_");
  QCoreApplication::processEvents();

  if(qEnvironmentVariableIsSet("SCORE_VST3_SWEEP_REF_ON_SCORE"))
  {
    run_stats on_score;
    reference_host{mdl->getFactory(), *uid, rate, model->fx.supports_double}.play_on(
        *model->fx.component, *model->fx.processor, model->fx.controller, buses.event_in,
        model->fx.supports_double ? kSample64 : kSample32, stim, on_score);
    j.num("ref_on_score_rms", on_score.audio.samples ? std::sqrt(on_score.audio.sumsq / on_score.audio.samples) : 0.);
  }

  out.phase(uri, "teardown");
  const bool use_double = model->fx.supports_double;
  remove();
  // What the plug-in left on the event loop runs now, while it is still the
  // one to blame
  for(int i = 0; i < 10; i++)
  {
    QCoreApplication::processEvents();
    score::test::flush_deferred_deletes();
    QThread::msleep(20);
  }

  // Alone, as score's instance is gone
  out.phase(uri, "reference");
  run_stats ref;
  reference_host ref_host{mdl->getFactory(), *uid, rate, use_double};
  const auto ref_skip = ref_host.run(stim, ref);
  j.str("ref_state", ref_host.state);
  if(ref_skip.empty())
    ref.write(j, "ref_");
  else
    j.str("ref_skipped", ref_skip);

  out.write(j.str("status", "ran").done());
}

//! The audio classes of each bundle, loaded as score's scanner loads them
void scan(vst3::ApplicationPlugin& plug, const QStringList& bundles, results_file& out)
{
  for(const auto& bundle : bundles)
  {
    const std::string path = bundle.toStdString();
    out.phase(path, "scan");
    json j;
    j.str("type", "result").str("uri", path);
    auto mdl = plug.getModule(path);
    if(!mdl)
    {
      out.write(j.str("status", "load-failed").done());
      continue;
    }
    std::ostringstream classes;
    classes << '[';
    bool first = true;
    for(const auto& cls : mdl->getFactory().classInfos())
    {
      if(cls.category() != kVstAudioEffectClass)
        continue;
      classes << (first ? "" : ",") << json{}
                                           .str("uid", cls.ID().toString())
                                           .str("name", cls.name())
                                           .str("vendor", cls.vendor())
                                           .str("subcategories", cls.subCategoriesString())
                                           .done();
      first = false;
    }
    classes << ']';
    j.str("name", mdl->getName());
    j.raw("classes", classes.str());
    out.phase(path, "unload");
    mdl.reset();
    plug.modules.erase(path);
    out.write(j.str("status", "scanned").done());
  }
}

QStringList read_lines(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
}
}

TEST_CASE("VST3 system sweep worker", "[vst3][sweep]")
{
  const QString results = qEnvironmentVariable("SCORE_VST3_SWEEP_RESULTS");
  const QString scan_jobs = qEnvironmentVariable("SCORE_VST3_SWEEP_SCAN");
  const QString jobs = qEnvironmentVariable("SCORE_VST3_SWEEP_JOBS");
  if((jobs.isEmpty() && scan_jobs.isEmpty()) || results.isEmpty())
    return;

  std::vector<std::pair<QString, std::string>> todo;
  for(const auto& line : read_lines(jobs))
  {
    const auto parts = line.split('\t');
    if(parts.size() >= 2)
      todo.emplace_back(parts[0], parts[1].toStdString());
  }

  // No scanner processes: the bundles are loaded one by one
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
    if(!scan_jobs.isEmpty())
    {
      results_file out{results};
      scan(plug, read_lines(scan_jobs), out);
      return;
    }
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    results_file out{results};
    for(auto& [bundle, uid] : todo)
      probe(ctx, *doc, plug, out, bundle, uid);
  });
}
