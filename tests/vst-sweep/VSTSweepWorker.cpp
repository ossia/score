// The worker of the VST 2.4 system sweep (vst_sweep.py drives it): every VST
// plug-in installed on the machine, run through score's host with a stimulus
// fitting what it is (an instrument gets notes, an effect gets a signal), then
// through a minimal host of its own written against the VST 2.4 interface
// only. A plug-in silent in score but not in that host points at score; silent
// in both, at the plug-in or at what it needs and is not given.
//
// The worker measures and the driver judges: thresholds live in vst_sweep.py.
//
//  SCORE_VST_SWEEP_JOBS=<file>     probe the libraries of these lines (one path
//                                  each), appending one JSON object per line to
//                                  SCORE_VST_SWEEP_RESULTS: a "phase" record
//                                  before each step, so that a crash or a hang
//                                  can be told where it happened, then a
//                                  "result" record.
//
//  SCORE_VST_SWEEP_TRACE=1        print the peak of every block of score's
//                                  host on stderr, to look closer at one.
//
//  To tell what a difference between the hosts comes from, the reference
//  host can do as other hosts or score's float path do:
//  SCORE_VST_SWEEP_REF_PROGRAM0=1  select program 0 once the plug-in is open,
//                                  as JUCE and Carla do
//  SCORE_VST_SWEEP_REF_INPLACE=1   process in place, the outputs over the
//                                  inputs, as score's float path does
//
// Neither set, the test case has nothing to do and passes: ctest runs the
// driver, not this.

#include <Process/Dataflow/Port.hpp>

#include <Audio/Settings/Model.hpp>
#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Loader.hpp>
#include <Vst/Node.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <score_test/App.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
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
        if(c < 0x20 || c >= 0x80)
        {
          // Plug-ins write their strings in whatever encoding they like:
          // escaped byte by byte, the record stays valid JSON
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
    // Unique IDs and counts exactly, measures to the precision they have
    if(ossia::safe_isfinite(v) && v == std::trunc(v) && std::abs(v) < 1e15)
      s << int64_t(v);
    else if(ossia::safe_isfinite(v))
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
  double seconds{};
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

struct run_stats
{
  audio_stats audio;
  int64_t midi_out{};
  int64_t automate{};
  double seconds{};
  double wall_seconds{};

  void write(json& j, std::string_view prefix) const
  {
    const auto k = [&](const char* s) { return std::string(prefix) + s; };
    audio.write(j, prefix);
    j.num(k("midi_out"), double(midi_out));
    j.num(k("automate"), double(automate));
    j.num(k("rt_factor"), seconds > 0 ? wall_seconds / seconds : 0.);
  }
};

bool can_do(AEffect* fx, const char* what)
{
  return fx->dispatcher(fx, effCanDo, 0, 0, const_cast<char*>(what), 0.f) > 0;
}

std::string get_string(AEffect* fx, int op, int index = 0)
{
  // Plug-ins write past kVstMaxProductStrLen and friends: room to spare
  char buf[1024]{};
  fx->dispatcher(fx, op, index, 0, buf, 0.f);
  buf[sizeof(buf) - 1] = 0;
  return buf;
}

/**
 * A host written from the VST 2.4 interface alone, sharing nothing with
 * score's but the declarations of vst-compat.hpp: its own dlopen, its own
 * audioMaster, separate input and output buffers of the plug-in's own channel
 * counts, events sorted and within the block.
 */
struct reference_host
{
  // The audioMaster callback knows no instance until the entry point has
  // returned: the one being made is this one
  static inline thread_local reference_host* current{};

  int rate{48000};
  int block{512};
  bool use_double{};
  VstTimeInfo time{};
  std::thread::id main_thread{std::this_thread::get_id()};
  int64_t midi_out{};
  int64_t automate{};
  std::set<int> opcodes;
  //! Those asked from the thread that processes
  std::set<int> audio_thread_opcodes;
  std::set<std::string> can_dos;
  std::mutex mutex;
  AEffect* fx{};
  int32_t shell_id{};

  static intptr_t callback(
      AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr,
      float opt)
  {
    reference_host* self = current;
    if(effect && effect->resvd1)
      self = reinterpret_cast<reference_host*>(effect->resvd1);
    if(!self)
      return opcode == audioMasterVersion ? 2400 : 0;
    return self->answer(effect, opcode, index, value, ptr, opt);
  }

  intptr_t answer(
      AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr,
      float opt)
  {
    {
      std::lock_guard l{mutex};
      opcodes.insert(opcode);
      if(std::this_thread::get_id() != main_thread)
        audio_thread_opcodes.insert(opcode);
      if(opcode == audioMasterCanDo && ptr)
        can_dos.insert(static_cast<const char*>(ptr));
    }
    switch(opcode)
    {
      case audioMasterAutomate:
        automate++;
        return 0;
      case audioMasterVersion:
        return 2400;
      case audioMasterCurrentId:
        return shell_id;
      case audioMasterIdle:
        return 0;
      case audioMasterGetTime:
        return reinterpret_cast<intptr_t>(&time);
      case audioMasterProcessEvents:
        if(auto ev = static_cast<VstEvents*>(ptr))
          midi_out += std::max(0, ev->numEvents);
        return 1;
      case audioMasterIOChanged:
        return 0;
      case audioMasterSizeWindow:
        return 0;
      case audioMasterGetSampleRate:
        return rate;
      case audioMasterGetBlockSize:
        return block;
      case audioMasterGetInputLatency:
      case audioMasterGetOutputLatency:
        return 0;
      case audioMasterGetCurrentProcessLevel:
        return std::this_thread::get_id() == main_thread ? kVstProcessLevelUser
                                                         : kVstProcessLevelRealtime;
      case audioMasterGetAutomationState:
        return kVstAutomationOff;
      case audioMasterGetVendorString:
        if(ptr)
          std::strcpy(static_cast<char*>(ptr), "sweep");
        return ptr ? 1 : 0;
      case audioMasterGetProductString:
        if(ptr)
          std::strcpy(static_cast<char*>(ptr), "reference");
        return ptr ? 1 : 0;
      case audioMasterGetVendorVersion:
        return 1;
      case audioMasterCanDo: {
        if(!ptr)
          return 0;
        const std::string_view s{static_cast<const char*>(ptr)};
        for(auto c :
            {"sendVstEvents", "sendVstMidiEvent", "sendVstTimeInfo", "receiveVstEvents",
             "receiveVstMidiEvent", "sendVstMidiEventFlagIsRealtime"})
          if(s == c)
            return 1;
        return 0;
      }
      case audioMasterGetLanguage:
        return kVstLangEnglish;
      case audioMasterUpdateDisplay:
        return 0;
      case audioMasterBeginEdit:
      case audioMasterEndEdit:
        return 0;
      default:
        return 0;
    }
  }

  void set_time(int64_t frame, bool playing)
  {
    time.samplePos = double(frame);
    time.sampleRate = rate;
    time.nanoSeconds = double(frame) * 1e9 / rate;
    time.tempo = 120.;
    time.ppqPos = double(frame) / rate * 2.;
    time.barStartPos = std::floor(time.ppqPos / 4.) * 4.;
    time.timeSigNumerator = 4;
    time.timeSigDenominator = 4;
    time.flags = kVstNanosValid | kVstPpqPosValid | kVstTempoValid | kVstBarsValid
                 | kVstTimeSigValid | (playing ? kVstTransportPlaying : 0);
  }

  //! Makes an instance from the entry point of the library
  static AEffect* instantiate(vst::PluginEntryProc main, reference_host& self)
  {
    current = &self;
    AEffect* fx = main(&reference_host::callback);
    current = nullptr;
    if(fx && fx->magic == kEffectMagic)
      fx->resvd1 = reinterpret_cast<intptr_t>(&self);
    return fx;
  }

  std::string run(vst::PluginEntryProc main, const stimulus& stim, bool midi_in, run_stats& res)
  {
    fx = instantiate(main, *this);
    if(!fx || fx->magic != kEffectMagic)
      return "no instance";
    set_time(0, false);
    fx->dispatcher(fx, effOpen, 0, 0, nullptr, 0.f);
    if(qEnvironmentVariableIsSet("SCORE_VST_SWEEP_REF_PROGRAM0") && fx->numPrograms > 0)
      fx->dispatcher(fx, effSetProgram, 0, 0, nullptr, 0.f);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, float(rate));
    fx->dispatcher(fx, effSetBlockSize, 0, block, nullptr, 0.f);
    if(use_double)
      fx->dispatcher(fx, effSetProcessPrecision, 0, kVstProcessPrecision64, nullptr, 0.f);
    fx->dispatcher(fx, effMainsChanged, 0, 1, nullptr, 0.f);
    fx->dispatcher(fx, effStartProcess, 0, 0, nullptr, 0.f);

    const int ni = std::max(0, fx->numInputs), no = std::max(0, fx->numOutputs);
    std::vector<std::vector<float>> fin(ni, std::vector<float>(block)),
        fout(no, std::vector<float>(block));
    std::vector<std::vector<double>> din(ni, std::vector<double>(block)),
        dout(no, std::vector<double>(block));
    std::vector<float*> fin_p, fout_p;
    std::vector<double*> din_p, dout_p;
    for(auto& v : fin)
      fin_p.push_back(v.data());
    for(auto& v : fout)
      fout_p.push_back(v.data());
    for(auto& v : din)
      din_p.push_back(v.data());
    for(auto& v : dout)
      dout_p.push_back(v.data());
    // Never a null array, even with no channel
    fin_p.push_back(nullptr);
    fout_p.push_back(nullptr);
    din_p.push_back(nullptr);
    dout_p.push_back(nullptr);

    std::string err;
    std::thread audio{[&] {
      const int64_t total = int64_t(play_seconds * rate);
      res.audio.seconds = play_seconds;
      const auto t0 = std::chrono::steady_clock::now();
      for(int64_t frame = 0; frame < total; frame += block)
      {
        set_time(frame, true);
        // A signal on the two channels score's process has, silence on the
        // others (side-chains, extra buses)
        for(int c = 0; c < ni; c++)
          for(int k = 0; k < block; k++)
          {
            const double v = c < 2 ? stim.audio(c, frame + k) : 0.;
            fin[c][k] = float(v);
            din[c][k] = v;
          }
        for(int c = 0; c < no; c++)
        {
          std::fill(fout[c].begin(), fout[c].end(), 0.f);
          std::fill(dout[c].begin(), dout[c].end(), 0.);
        }

        if(midi_in)
        {
          const auto notes = stim.notes(frame, block);
          std::vector<VstMidiEvent> ev(notes.size());
          std::vector<char> buf(sizeof(VstEvents) + sizeof(VstEvent*) * (notes.size() + 2));
          auto* events = reinterpret_cast<VstEvents*>(buf.data());
          events->numEvents = int32_t(notes.size());
          for(std::size_t i = 0; i < notes.size(); i++)
          {
            ev[i].type = kVstMidiType;
            ev[i].byteSize = sizeof(VstMidiEvent);
            ev[i].deltaFrames = int32_t(notes[i].frame);
            ev[i].flags = kVstMidiEventIsRealtime;
            ev[i].midiData[0] = char(notes[i].on ? 0x90 : 0x80);
            ev[i].midiData[1] = char(notes[i].pitch);
            ev[i].midiData[2] = char(notes[i].on ? 100 : 0);
            events->events[i] = reinterpret_cast<VstEvent*>(&ev[i]);
          }
          if(!notes.empty())
            fx->dispatcher(fx, effProcessEvents, 0, 0, events, 0.f);
          process(ni, no, fin_p, fout_p, din_p, dout_p);
        }
        else
        {
          process(ni, no, fin_p, fout_p, din_p, dout_p);
        }

        for(int c = 0; c < std::min(no, 2); c++)
          for(int k = 0; k < block; k++)
          {
            const double v = use_double ? dout[c][k] : double(fout[c][k]);
            res.audio.add(v, double(frame + k) / rate);
            if(c == 0 && ni > 0)
            {
              res.audio.compared_input = true;
              if(ossia::safe_isfinite(v))
                res.audio.max_diff_input = std::max(
                    res.audio.max_diff_input, std::abs(v - stim.audio(0, frame + k)));
            }
          }
      }
      res.seconds = play_seconds;
      res.wall_seconds
          = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }};
    audio.join();

    fx->dispatcher(fx, effStopProcess, 0, 0, nullptr, 0.f);
    fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0.f);

    // Its state, as a host saves it: the chunk, or else the parameters
    const bool chunks = fx->flags & effFlagsProgramChunks;
    std::vector<char> chunk;
    std::vector<float> params;
    if(chunks)
    {
      void* ptr{};
      const auto n = fx->dispatcher(fx, effGetChunk, 0, 0, &ptr, 0.f);
      if(ptr && n > 0)
        chunk.assign((const char*)ptr, (const char*)ptr + n);
    }
    for(int i = 0; i < fx->numParams; i++)
      params.push_back(fx->getParameter(fx, i));
    fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
    fx = nullptr;

    // ... and loaded back into a new instance, the copy the host keeps freed
    // once the call returns, as the specification allows
    if(AEffect* fx2 = instantiate(main, *this); fx2 && fx2->magic == kEffectMagic)
    {
      fx2->dispatcher(fx2, effOpen, 0, 0, nullptr, 0.f);
      fx2->dispatcher(fx2, effSetSampleRate, 0, 0, nullptr, float(rate));
      fx2->dispatcher(fx2, effSetBlockSize, 0, block, nullptr, 0.f);
      if(chunks && !chunk.empty())
      {
        std::vector<char> copy = chunk;
        fx2->dispatcher(fx2, effSetChunk, 0, intptr_t(copy.size()), copy.data(), 0.f);
      }
      else if(!chunks)
      {
        for(int i = 0; i < int(params.size()) && i < fx2->numParams; i++)
          fx2->setParameter(fx2, i, params[i]);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      for(int i = 0; i < int(params.size()) && i < fx2->numParams; i++)
        if(std::abs(fx2->getParameter(fx2, i) - params[i]) > 1e-4f)
          state_mismatch++;
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      fx2->dispatcher(fx2, effClose, 0, 0, nullptr, 0.f);
      state_ran = true;
    }
    res.midi_out = midi_out;
    res.automate = automate;
    return err;
  }

  int state_mismatch{};
  bool state_ran{};

  void process(
      int ni, int no, std::vector<float*>& fin, std::vector<float*>& fout,
      std::vector<double*>& din, std::vector<double*>& dout)
  {
    if(qEnvironmentVariableIsSet("SCORE_VST_SWEEP_REF_INPLACE"))
    {
      // The outputs written over the inputs, as score's float path does
      for(int c = 0; c < std::min(ni, no); c++)
      {
        std::copy_n(fin[c], block, fout[c]);
        std::copy_n(din[c], block, dout[c]);
      }
      if(use_double)
        fx->processDoubleReplacing(fx, dout.data(), dout.data(), block);
      else
        fx->processReplacing(fx, fout.data(), fout.data(), block);
      return;
    }
    if(use_double)
      fx->processDoubleReplacing(fx, din.data(), dout.data(), block);
    else
      fx->processReplacing(fx, fin.data(), fout.data(), block);
  }
};

//! What the reference host learns from a first instance, before score makes one
struct identity
{
  std::string status;
  std::string reason;
  int32_t unique_id{};
  int32_t version{};
  int32_t flags{};
  int32_t inputs{}, outputs{}, params{}, programs{};
  int32_t category{};
  int32_t initial_delay{};
  std::string effect_name, product, vendor;
  bool receives_midi{}, sends_midi{};
  bool processes_float{}, processes_double{};
  std::vector<std::string> shell_children;
};

identity identify(vst::PluginEntryProc main)
{
  identity id;
  reference_host host;
  AEffect* fx = reference_host::instantiate(main, host);
  if(!fx)
  {
    id.status = "not-a-plugin";
    id.reason = "the entry point returned no instance";
    return id;
  }
  if(fx->magic != kEffectMagic)
  {
    id.status = "not-a-plugin";
    id.reason = "the entry point returned something that is not an AEffect";
    return id;
  }
  host.fx = fx;
  fx->dispatcher(fx, effOpen, 0, 0, nullptr, 0.f);
  id.unique_id = fx->uniqueID;
  id.version = fx->version;
  id.flags = fx->flags;
  id.inputs = fx->numInputs;
  id.outputs = fx->numOutputs;
  id.params = fx->numParams;
  id.programs = fx->numPrograms;
  id.initial_delay = fx->initialDelay;
  id.category = int32_t(fx->dispatcher(fx, effGetPlugCategory, 0, 0, nullptr, 0.f));
  id.effect_name = get_string(fx, effGetEffectName);
  id.product = get_string(fx, effGetProductString);
  id.vendor = get_string(fx, effGetVendorString);
  id.receives_midi = can_do(fx, "receiveVstMidiEvent") || can_do(fx, "receiveVstEvents");
  id.sends_midi = can_do(fx, "sendVstMidiEvent") || can_do(fx, "sendVstEvents");
  id.processes_float = fx->processReplacing != nullptr;
  id.processes_double = fx->processDoubleReplacing != nullptr;
  if(id.category == kPlugCategShell)
  {
    for(int i = 0; i < 1024; i++)
    {
      char name[256]{};
      const auto child = fx->dispatcher(fx, effShellGetNextPlugin, 0, 0, name, 0.f);
      if(child == 0)
        break;
      id.shell_children.push_back(name);
    }
  }
  fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
  id.status = "ok";
  return id;
}

std::string role_of(const identity& id)
{
  const bool ain = id.inputs > 0;
  const bool aout = id.outputs > 0;
  const bool synth = (id.flags & effFlagsIsSynth) || id.category == kPlugCategSynth;
  if(aout && (synth || (id.receives_midi && !ain)))
    return "instrument";
  if(ain && aout)
    return id.receives_midi ? "midi-controlled-effect" : "effect";
  if(aout)
    return "generator";
  if(id.receives_midi && id.sends_midi)
    return "midi-effect";
  if(id.sends_midi)
    return "midi-generator";
  if(ain)
    return "analyzer";
  return "other";
}

//! The node the executor of a VST process makes, its controls set as
//! Executor::setupNode sets them
template <bool UseDouble, bool IsSynth>
struct score_player
{
  using node_t = vst::vst_node<UseDouble, IsSynth>;

  static run_stats play(
      vst::Model& model, const stimulus& stim, int block, double seconds, bool trace)
  {
    run_stats res;
    res.audio.seconds = seconds;
    const int rate = stim.rate;
    ossia::execution_state st;
    st.sampleRate = rate;
    st.bufferSize = block;
    st.modelToSamplesRatio = rate / ossia::flicks_per_second<double>;
    st.samplesToModelRatio = ossia::flicks_per_second<double> / rate;

    // Made on the main thread, as the executor is
    auto node = std::make_shared<node_t>(model.fx, rate, block, model.sendsMidi());
    std::vector<std::unique_ptr<ossia::value_inlet>> keep;
    for(std::size_t i = VST_FIRST_CONTROL_INDEX(IsSynth); i < model.inlets().size(); i++)
    {
      auto ctrl = dynamic_cast<vst::ControlInlet*>(model.inlets()[i]);
      if(!ctrl)
        continue;
      auto inlet = new ossia::value_inlet;
      (**inlet).domain = ossia::domain_base<float>{0.f, 1.f};
      (**inlet).type = ossia::val_type::FLOAT;
      node->controls.push_back(
          {ctrl->fxNum, ossia::convert<float>(ctrl->value()),
           inlet->target<ossia::value_port>()});
      node->root_inputs().push_back(inlet);
    }

    const int ni = model.fx->fx->numInputs;
    // Played on a thread of its own, as the audio thread is
    std::thread audio{[&] {
      const int64_t total = int64_t(seconds * rate);
      const double flicks_per_frame = ossia::flicks_per_second<double> / rate;
      const auto t0 = std::chrono::steady_clock::now();
      for(int64_t frame = 0; frame < total; frame += block)
      {
        auto& a = node->root_inputs()[0]->template cast<ossia::audio_port>();
        a.set_channels(2);
        for(std::size_t c = 0; c < 2; c++)
        {
          a.channel(c).resize(block);
          for(int k = 0; k < block; k++)
            a.channel(c)[k] = stim.audio(int(c), frame + k);
        }
        if constexpr(IsSynth)
        {
          auto& m = node->root_inputs()[1]->template cast<ossia::midi_port>();
          m.messages.clear();
          for(auto& n : stim.notes(frame, block))
          {
            auto u = libremidi::ump_from_midi1(
                n.on ? libremidi::channel_events::note_on(1, n.pitch, 100)
                     : libremidi::channel_events::note_off(1, n.pitch, 0));
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
        tk.musical_start_position = double(frame) / rate * 2.;
        tk.musical_end_position = double(frame + block) / rate * 2.;
        tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
        node->process_time(tk, st);
        ossia::exec_state_facade fac{&st};
        static_cast<ossia::graph_node&>(*node).run(tk, fac);

        auto& out = node->root_outputs()[0]->template cast<ossia::audio_port>();
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
        if(model.fx->fx->numOutputs > 0)
        {
          for(std::size_t c = 0; c < std::min<std::size_t>(out.channels(), 2); c++)
          {
            if(int(c) >= model.fx->fx->numOutputs && model.fx->fx->numOutputs != 1)
              break;
            auto& ch = out.channel(c);
            for(std::size_t k = 0; k < ch.size() && k < std::size_t(block); k++)
            {
              const double v = ch[k];
              res.audio.add(v, double(frame + k) / rate);
              if(c == 0 && ni > 0)
              {
                res.audio.compared_input = true;
                if(ossia::safe_isfinite(v))
                  res.audio.max_diff_input = std::max(
                      res.audio.max_diff_input, std::abs(v - stim.audio(0, frame + k)));
              }
            }
          }
        }
        if(node->midi_out)
        {
          res.midi_out += node->midi_out->data.messages.size();
          node->midi_out->data.messages.clear();
        }
        for(auto& p : node->root_inputs())
          if(auto vp = p->template target<ossia::value_port>())
            vp->get_data().clear();
      }
      res.seconds = double(total) / rate;
      res.wall_seconds
          = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

      // As the transport does when it stops: ossia::node_process::stop()
      node->all_notes_off();
    }};
    audio.join();

    // Released on the main thread, with the executor
    node.reset();
    return res;
  }
};

run_stats play(vst::Model& model, const stimulus& stim, int block, double seconds)
{
  const bool trace = qEnvironmentVariableIsSet("SCORE_VST_SWEEP_TRACE");
  const auto flags = model.fx->fx->flags;
  const bool dbl = flags & effFlagsCanDoubleReplacing;
  const bool synth = model.takesMidi();
  if(dbl && synth)
    return score_player<true, true>::play(model, stim, block, seconds, trace);
  if(dbl)
    return score_player<true, false>::play(model, stim, block, seconds, trace);
  if(synth)
    return score_player<false, true>::play(model, stim, block, seconds, trace);
  return score_player<false, false>::play(model, stim, block, seconds, trace);
}

void write_identity(json& j, const identity& id)
{
  j.num("unique_id", id.unique_id);
  j.num("version", id.version);
  j.num("flags", id.flags);
  j.num("category", id.category);
  j.num("initial_delay", id.initial_delay);
  j.str("effect_name", id.effect_name);
  j.str("product", id.product);
  j.str("vendor", id.vendor);
  j.boolean("receives_midi", id.receives_midi);
  j.boolean("sends_midi", id.sends_midi);
  j.boolean("synth_flag", id.flags & effFlagsIsSynth);
  j.boolean("can_double", id.flags & effFlagsCanDoubleReplacing);
  j.boolean("can_replacing", id.flags & effFlagsCanReplacing);
  j.boolean("processes_float", id.processes_float);
  j.boolean("processes_double", id.processes_double);
  j.boolean("has_editor", id.flags & effFlagsHasEditor);
  j.boolean("program_chunks", id.flags & effFlagsProgramChunks);
  std::ostringstream ports;
  ports << "{\"audio_in\":" << id.inputs << ",\"audio_out\":" << id.outputs
        << ",\"params\":" << id.params << ",\"programs\":" << id.programs << "}";
  j.raw("ports", ports.str());
}

void probe(
    vst::ApplicationPlugin& app, results_file& out, const std::string& path, bool noref)
{
  json j;
  j.str("type", "result").str("path", path);
  const QString qpath = QString::fromStdString(path);
  j.str("name", QFileInfo(qpath).completeBaseName().toStdString());

  out.phase(path, "load");
  // score's loader, which the scan and the processes both go through
  std::unique_ptr<vst::Module> module;
  try
  {
    module = std::make_unique<vst::Module>(path);
  }
  catch(const std::exception& e)
  {
    out.write(j.str("status", "not-a-plugin").str("reason", e.what()).done());
    return;
  }
  auto main = module->getMain();
  if(!main)
  {
    out.write(
        j.str("status", "not-a-plugin").str("reason", "no VST 2 entry point").done());
    return;
  }

  out.phase(path, "identify");
  const auto id = identify(main);
  if(id.status != "ok")
  {
    out.write(j.str("status", id.status).str("reason", id.reason).done());
    return;
  }
  write_identity(j, id);
  const auto role = role_of(id);
  j.str("role", role);
  if(id.category == kPlugCategShell)
  {
    j.strings("shell_children", id.shell_children);
    out.write(j.str("status", "shell").done());
    return;
  }

  auto& audio = score::AppContext().settings<Audio::Settings::Model>();
  const int rate = audio.getRate();
  const int block = audio.getBufferSize();
  j.num("rate", rate);
  j.num("block", block);
  stimulus stim{rate};

  // First, so that a crash of score's host after it points at score
  out.phase(path, "reference");
  if(!noref)
  {
    run_stats ref;
    reference_host host;
    host.rate = rate;
    host.block = block;
    host.use_double = id.flags & effFlagsCanDoubleReplacing;
    const bool midi_in = (id.flags & effFlagsIsSynth) || id.receives_midi;
    const auto ref_skip = host.run(main, stim, midi_in, ref);
    if(ref_skip.empty())
    {
      ref.write(j, "ref_");
      std::vector<std::string> ops;
      for(int o : host.opcodes)
        ops.push_back(std::to_string(o));
      j.strings("ref_opcodes", ops);
      ops.clear();
      for(int o : host.audio_thread_opcodes)
        ops.push_back(std::to_string(o));
      j.strings("ref_audio_thread_opcodes", ops);
      j.strings("ref_can_dos", {host.can_dos.begin(), host.can_dos.end()});
      if(host.state_ran)
        j.num("ref_state_param_mismatch", host.state_mismatch);
    }
    else
      j.str("ref_skipped", ref_skip);
  }

  out.phase(path, "model");
  // What a scan of this library would have added to the library
  vst::VSTInfo info;
  info.path = qpath;
  info.prettyName = QFileInfo(qpath).completeBaseName();
  info.displayName = QString::fromStdString(id.product);
  info.author = QString::fromStdString(id.vendor);
  info.uniqueID = id.unique_id;
  info.controls = id.params;
  info.isSynth = id.flags & effFlagsIsSynth;
  info.isValid = true;
  app.vst_infos = {info};
  app.vst_modules.erase(id.unique_id);
  app.vst_modules.insert({id.unique_id, nullptr});

  QObject parent;
  const auto t0 = std::chrono::steady_clock::now();
  auto* model = new vst::Model{
      TimeVal::fromMsecs(10000), QString::number(id.unique_id),
      Id<Process::ProcessModel>{1}, &parent};
  j.num(
      "instantiate_ms",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
          .count());
  if(!model->fx || !model->fx->fx)
  {
    delete model;
    out.write(j.str("status", "instantiate-failed").done());
    return;
  }
  {
    int controls = 0;
    for(auto* p : model->inlets())
      controls += bool(dynamic_cast<vst::ControlInlet*>(p));
    j.num("score_controls", controls);
    j.boolean("score_midi_in", model->takesMidi());
    j.boolean("score_midi_out", model->sendsMidi());
    j.boolean("score_double", model->fx->fx->flags & effFlagsCanDoubleReplacing);
  }

  // A process is created some time before it plays: plug-ins loading on a
  // thread of their own are ready by then
  QThread::msleep(settle_ms);
  QCoreApplication::processEvents();

  out.phase(path, "play");
  const auto first = play(*model, stim, block, play_seconds);
  first.write(j, "play_");
  QCoreApplication::processEvents();

  out.phase(path, "replay");
  const auto second = play(*model, stim, block, replay_seconds);
  second.write(j, "replay_");
  QCoreApplication::processEvents();

  // Saved as a document saves it, loaded back as a document loads it
  out.phase(path, "state");
  {
    const auto saved
        = score::marshall<JSONObject>(static_cast<const Process::ProcessModel&>(*model));
    std::vector<float> before;
    for(int i = 0; i < model->fx->fx->numParams; i++)
      before.push_back(model->fx->getParameter(i));

    QObject parent2;
    const auto doc = readJson(saved.toByteArray());
    JSONObject::Deserializer des{doc};
    auto* loaded = new vst::Model{des, &parent2};
    int mismatch = 0;
    if(loaded->fx && loaded->fx->fx)
    {
      for(int i = 0; i < loaded->fx->fx->numParams && i < int(before.size()); i++)
        if(std::abs(loaded->fx->getParameter(i) - before[i]) > 1e-4f)
          mismatch++;
      j.boolean("state_loaded", true);
    }
    else
    {
      j.boolean("state_loaded", false);
    }
    j.num("state_param_mismatch", mismatch);
    // A document is not closed the moment it is loaded: plug-ins that apply
    // a state on a thread of their own have the time to
    QThread::msleep(settle_ms);
    QCoreApplication::processEvents();
    delete loaded;
    QCoreApplication::processEvents();
  }

  out.phase(path, "teardown");
  delete model;
  QCoreApplication::processEvents();
  score::test::flush_deferred_deletes();
  QCoreApplication::processEvents();
  out.write(j.str("status", "ran").done());
}
}

TEST_CASE("VST system sweep worker", "[vst][sweep]")
{
  const QString jobs = qEnvironmentVariable("SCORE_VST_SWEEP_JOBS");
  const QString results = qEnvironmentVariable("SCORE_VST_SWEEP_RESULTS");
  if(jobs.isEmpty() || results.isEmpty())
    return;

  QFile f{jobs};
  REQUIRE(f.open(QIODevice::ReadOnly));
  // path [\t noref]: noref when the reference host crashed on it before
  std::vector<std::pair<std::string, bool>> todo;
  for(const auto& line : QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts))
  {
    const auto parts = line.split('\t');
    todo.emplace_back(parts[0].toStdString(), parts.size() > 1 && parts[1] == "noref");
  }

  // No scanner processes: the libraries are loaded one by one
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
    results_file out{results};
    for(auto& [path, noref] : todo)
      probe(app, out, path, noref);
  });
}
