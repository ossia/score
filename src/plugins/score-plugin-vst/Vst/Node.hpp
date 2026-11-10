#pragma once
#include <Process/Dataflow/TimeSignature.hpp>

#include <Vst/EffectModel.hpp>
#include <Vst/MidiEvents.hpp>

#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_signature.hpp>

#include <libremidi/detail/conversion.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>
namespace vst
{

class vst_node_base : public ossia::graph_node
{
public:
  std::shared_ptr<AEffectWrapper> fx{};

protected:
  explicit vst_node_base(std::shared_ptr<AEffectWrapper>&& ptr)
      : fx{std::move(ptr)}
  {
    this->set_not_fp_safe();
    m_inlets.reserve(10);
    controls.reserve(10);
  }

  struct vst_control
  {
    int idx{};
    float value{};
    ossia::value_port* port{};
  };

  inline void dispatch(
      int32_t opcode, int32_t index = 0, intptr_t value = 0, void* ptr = nullptr,
      float opt = 0.0f)
  {
    fx->dispatch(opcode, index, value, ptr, opt);
  }

public:
  ossia::small_vector<vst_control, 16> controls;

  //! The MIDI output of the process, when it has one
  ossia::midi_outlet* midi_out{};

  //! The events the plug-in sends (audioMasterProcessEvents) while it
  //! processes a block, dated in the tick. Sent from any other thread, or at
  //! any other time, they have no date and are dropped.
  void receiveEvents(const VstEvents* events) noexcept
  {
    if(!midi_out || !events
       || m_processThread.load(std::memory_order_relaxed) != std::this_thread::get_id())
      return;
    auto& out = midi_out->data.messages;
    const auto push = [&](const libremidi::ump& u) {
      if(out.size() < 4096)
        out.push_back(u);
    };
    for(int i = 0; i < events->numEvents; i++)
    {
      const VstEvent* e = events->events[i];
      if(!e)
        continue;
      const int64_t date = m_blockStart + std::max(0, e->deltaFrames);
      if(e->type == kVstMidiType)
      {
        const auto* m = reinterpret_cast<const VstMidiEvent*>(e);
        const auto* bytes = reinterpret_cast<const unsigned char*>(m->midiData);
        m_toUmp.convert_packets(bytes, midiMessageSize(bytes[0]), date, push);
      }
      else if(e->type == kVstSysExType)
      {
        const auto* s = reinterpret_cast<const VstMidiSysexEvent*>(e);
        if(s->sysexDump && s->dumpBytes > 0)
          m_toUmp.convert_packets(
              reinterpret_cast<const unsigned char*>(s->sysexDump),
              std::size_t(s->dumpBytes), date, push);
      }
    }
  }

  static std::size_t midiMessageSize(unsigned char status) noexcept
  {
    switch(status & 0xF0)
    {
      case 0xC0:
      case 0xD0:
        return 2;
      case 0xF0:
        switch(status)
        {
          case 0xF1:
          case 0xF3:
            return 2;
          case 0xF2:
            return 3;
          default:
            return 1;
        }
      default:
        return 3;
    }
  }

protected:
  //! The thread that is in a process call of the plug-in, and where in the
  //! tick the block it processes starts
  std::atomic<std::thread::id> m_processThread{};
  int64_t m_blockStart{};
  libremidi::midi1_to_midi2 m_toUmp;

public:

  void setControls()
  {
    for(vst_control& p : controls)
    {
      const auto& vec = p.port->get_data();
      if(vec.empty())
        continue;
      auto t = ossia::convert<float>(last(vec));
      {
        p.value = ossia::clamp<float>(t, 0.f, 1.f);
        fx->fx->setParameter(fx->fx, p.idx, p.value);
      }
    }
  }

  auto& prepareInput(int64_t offset, int64_t samples)
  {
    const auto bs = offset + samples;
    auto& p = *m_inlets[0]->template target<ossia::audio_port>();
    switch(p.channels())
    {
      case 0: {
        p.set_channels(2);
        p.channel(0).resize(bs);
        p.channel(1).resize(bs);
        break;
      }
      case 1: {
        p.set_channels(2);
        p.channel(0).resize(bs);
        p.channel(1).assign(p.channel(0).begin(), p.channel(0).end());
        break;
      }
      default: {
        for(auto& i : p)
          i.resize(bs);
        break;
      }
    }

    return p.get();
  }

  auto& prepareOutput(int64_t offset, int64_t samples)
  {
    const auto bs = offset + samples;
    auto& p = *m_outlets[0]->template target<ossia::audio_port>();
    p.set_channels(2);
    for(auto& chan : p)
      chan.resize(bs, boost::container::default_init);
    return p.get();
  }

  void setupTimeInfo(const ossia::token_request& tk, ossia::exec_state_facade st)
  {
    auto& time_info = fx->info;
    // The playhead at the first sample of this block. m_transport_frames is
    // only updated once a node has run: it is that of the previous tick.
    time_info.samplePos = tk.speed != 0. ? tk.start_date_to_physical(st.modelToSamples())
                                         : this->m_transport_frames;
    time_info.sampleRate = st.sampleRate();
    time_info.nanoSeconds = st.currentDate() - st.startDate();
    time_info.ppqPos = tk.musical_start_position; // * ppq_reference;
    time_info.tempo = tk.tempo;
    time_info.barStartPos = tk.musical_start_last_bar; // * ppq_reference;
    time_info.cycleStartPos = 0.;
    time_info.cycleEndPos = 0.;
    time_info.timeSigNumerator = tk.signature.upper;
    time_info.timeSigDenominator = tk.signature.lower;
    time_info.smpteOffset = 0;
    time_info.smpteFrameRate = 0;
    time_info.samplesToNextClock = 0;
    time_info.flags = kVstTransportPlaying | kVstNanosValid | kVstPpqPosValid
                      | kVstTempoValid | kVstBarsValid | kVstTimeSigValid
                      | kVstClockValid;
  }
};

template <bool UseDouble, bool IsSynth>
class vst_node final : public vst_node_base
{
public:
  static constexpr bool synth = IsSynth;
  VstSpeakerArrangement i_arr{};
  VstSpeakerArrangement o_arr{};
  int m_bs{};

  vst_node(
      std::shared_ptr<AEffectWrapper> dat, int sampleRate, int bs, bool midiOutput = false)
      : vst_node_base{std::move(dat)}
      , m_bs{bs}
  {
    // Midi or audio input
    m_inlets.push_back(new ossia::audio_inlet);
    if constexpr(IsSynth)
      m_inlets.push_back(new ossia::midi_inlet);

    // audio output
    m_outlets.push_back(new ossia::audio_outlet);
    if(midiOutput)
    {
      midi_out = new ossia::midi_outlet;
      m_outlets.push_back(midi_out);
    }

    {
      memset(&i_arr, 0, sizeof(i_arr));
      memset(&o_arr, 0, sizeof(o_arr));
      i_arr.type = kSpeakerArrStereo;
      i_arr.numChannels = 2;
      i_arr.speakers[0].type = kSpeakerL;
      i_arr.speakers[1].type = kSpeakerR;

      o_arr.type = kSpeakerArrStereo;
      o_arr.numChannels = 2;
      o_arr.speakers[0].type = kSpeakerL;
      o_arr.speakers[1].type = kSpeakerR;
      dispatch(effSetSpeakerArrangement, 0, (intptr_t)&i_arr, (void*)&o_arr, 0);
    }

    dispatch(effSetSampleRate, 0, sampleRate, nullptr, sampleRate);
    dispatch(effSetBlockSize, 0, bs, nullptr, bs); // Generalize what's in pd
    dispatch(
        effSetProcessPrecision, 0,
        UseDouble ? kVstProcessPrecision64 : kVstProcessPrecision32);
    dispatch(effMainsChanged, 0, 1);
    dispatch(effStartProcess);

    fx->fx->resvd2 = reinterpret_cast<intptr_t>(static_cast<vst_node_base*>(this));
  }

  ~vst_node()
  {
    fx->fx->resvd2 = 0;
    fx->info.flags = (fx->info.flags & ~kVstTransportPlaying) | kVstTransportChanged;
    dispatch(effStopProcess);
    dispatch(effMainsChanged, 0, 0);
  }

  std::string label() const noexcept override
  {
    char paramName[512] = {0};
    fx->fx->dispatcher(fx->fx, effGetProductString, 0, 0, paramName, 0.0f);
    return paramName;
  }

  void all_notes_off() noexcept override
  {
    if constexpr(IsSynth)
    {
      // copy midi data
      // should be 32 but some VSTs read a bit out of bounds apparently ! so we allocate a bit more memory
      constexpr auto sz = sizeof(VstEvents) + sizeof(void*) * 32 * 2;
      VstEvents* events = (VstEvents*)alloca(sz);
      std::memset(events, 0, sz);

      events->numEvents = 32;

      VstMidiEvent ev[64] = {};
      memset(&ev, 0, sizeof(ev));

      // All notes off
      for(int i = 0; i < 16; i++)
      {
        auto& e = ev[i];
        e.type = kVstMidiType;
        e.flags = kVstMidiEventIsRealtime;
        e.byteSize = sizeof(VstMidiEvent);

        e.midiData[0] = (char)(uint8_t)176 + i;
        e.midiData[1] = (char)(uint8_t)123;
        e.midiData[2] = 0;
        e.midiData[3] = 0;

        events->events[i] = reinterpret_cast<VstEvent*>(&e);
      }

      // All sound off
      for(int i = 0; i < 16; i++)
      {
        auto& e = ev[16 + i];
        e.type = kVstMidiType;
        e.flags = kVstMidiEventIsRealtime;
        e.byteSize = sizeof(VstMidiEvent);

        e.midiData[0] = (char)(uint8_t)176 + i;
        e.midiData[1] = (char)(uint8_t)121;
        e.midiData[2] = 0;
        e.midiData[3] = 0;

        events->events[16 + i] = reinterpret_cast<VstEvent*>(&e);
      }

      dispatch(effProcessEvents, 0, 0, events, 0.f);

      if constexpr(!UseDouble)
      {
        float* dummy = (float*)alloca(sizeof(float) * m_bs);
        std::fill_n(dummy, m_bs, 0.f);

        float** input
            = (float**)alloca(sizeof(float*) * std::max(2, this->fx->fx->numInputs));
        for(int i = 0; i < this->fx->fx->numInputs; i++)
          input[i] = dummy;

        float** output
            = (float**)alloca(sizeof(float*) * std::max(2, this->fx->fx->numOutputs));
        for(int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processReplacing(fx->fx, input, output, m_bs);
      }
      else
      {
        double* dummy = (double*)alloca(sizeof(double) * m_bs);
        std::fill_n(dummy, m_bs, 0.);

        double** input
            = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numInputs));
        for(int i = 0; i < this->fx->fx->numInputs; i++)
          input[i] = dummy;

        double** output
            = (double**)alloca(sizeof(double*) * std::max(2, this->fx->fx->numOutputs));
        for(int i = 0; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;

        fx->fx->processDoubleReplacing(fx->fx, input, output, m_bs);
      }
    }
  }

  // Note: the function that does the actual tick is passed as an argument
  // since some plug-ins only store pointers to VstEvents struct,
  // which would go out of scope if this function was just called like this.
  //! Keep this tick's events for the next block rather than dropping them: the
  //! span covers no whole sample, so every date in the tick maps to the sample
  //! the span starts on, which is offset 0 of the next block that covers one.
  void stashMidi()
  {
    for(const libremidi::ump& mess :
        static_cast<ossia::midi_inlet*>(m_inlets[1])->data.messages)
    {
      if(m_deferred.size() >= 1024)
        return;
      m_deferred.push_back(mess);
    }
  }

  //! Sends the events dated in [offset, offset + samples), then calls f. The
  //! first block of a tick also takes the earlier ones and those held back
  //! from a tick that covered no whole sample, at its start; the last also
  //! takes the later ones, at its end.
  template <typename Fun>
  void dispatchMidi(int64_t offset, int64_t samples, bool first, bool last, Fun&& f)
  {
    auto& ip = static_cast<ossia::midi_inlet*>(m_inlets[1])->data.messages;
    const auto in_block = [=](const libremidi::ump& mess) {
      return (first || mess.timestamp >= offset)
             && (last || mess.timestamp < offset + samples);
    };
    const auto n_mess
        = (first ? m_deferred.size() : 0) + ossia::count_if(ip, in_block);
    if(n_mess == 0)
    {
      f();
      return;
    }

    // -2 since two are already available ?
    const auto sz = sizeof(VstEvents) + sizeof(void*) * n_mess * 2;
    VstEvents* events = (VstEvents*)alloca(sz);
    std::memset(events, 0, sz);

    ossia::small_vector<std::pair<const libremidi::ump*, int64_t>, 16> to_send;
    if(first)
      for(const libremidi::ump& mess : m_deferred)
        to_send.push_back({&mess, 0});
    for(const libremidi::ump& mess : ip)
      if(in_block(mess))
        to_send.push_back(
            {&mess, std::clamp<int64_t>(
                 mess.timestamp - offset, 0, samples > 0 ? samples - 1 : 0)});

    // The plug-in may read the events, sysex bytes included, until f() has
    // returned.
    ossia::small_vector<VstMidiEvent, 16> vec;
    vec.resize(n_mess);
    m_sysexArena.clear();
    std::size_t sysex_count = 0;
    std::size_t i = 0;
    for(const auto& [mess_ptr, delta] : to_send)
    {
      VstMidiSysexEvent* sysex
          = sysex_count < m_sysexEvents.size() ? &m_sysexEvents[sysex_count] : nullptr;
      VstEvent* e = toVstEvent(
          *mess_ptr, int32_t(delta), vec[i], sysex, m_sysexArena, m_sysexStream);
      if(!e)
        continue;
      if(e == reinterpret_cast<VstEvent*>(sysex))
        sysex_count++;
      events->events[i++] = e;
    }
    events->numEvents = int32_t(i);
    if(first)
      m_deferred.clear();
    dispatch(effProcessEvents, 0, 0, events, 0.f);
    f();
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    if(!muted() && !tk.paused())
    {
      const auto timings = st.timings(tk);
      if(timings.length <= 0)
      {
        // Never call a plug-in with an empty block, but do not lose what the
        // tick carried: it goes out at the top of the next real one.
        if constexpr(IsSynth)
          this->stashMidi();
        return;
      }

      this->setControls();
      this->setupTimeInfo(tk, st);

      if constexpr(UseDouble)
      {
        if constexpr(IsSynth)
        {
          const int64_t end = timings.start_sample + timings.length;
          forEachBlock(timings.start_sample, timings.length, [&](int64_t o, int64_t n) {
            dispatchMidi(o, n, o == timings.start_sample, o + n == end, [&] {
              processDoubleBlock(o, n);
            });
          });
        }
        else
        {
          processDouble(timings.start_sample, timings.length);
        }
      }
      else
      {
        if constexpr(IsSynth)
        {
          const int64_t end = timings.start_sample + timings.length;
          forEachBlock(timings.start_sample, timings.length, [&](int64_t o, int64_t n) {
            dispatchMidi(o, n, o == timings.start_sample, o + n == end, [&] {
              processFloatBlock(o, n);
            });
          });
        }
        else
        {
          processFloat(timings.start_sample, timings.length);
        }
      }

      // upmix mono VSTs to stereo
      if(this->fx->fx->numOutputs == 1)
      {
        auto& op = m_outlets[0]->template target<ossia::audio_port>()->get();
        op[1].assign(op[0].begin(), op[0].end());
      }
    }
  }

  //! The plug-in was told at most m_bs frames per call (effSetBlockSize), but
  //! the engine's ticks can outgrow the block size it had then. Each block
  //! sees the transport where it starts.
  template <typename F>
  void forEachBlock(int64_t offset, int64_t samples, F&& f)
  {
    auto& time_info = fx->info;
    const double ppq_per_frame = time_info.sampleRate > 0 && time_info.tempo > 0
                                     ? time_info.tempo / (60. * time_info.sampleRate)
                                     : 0.;
    const int64_t block = std::max(m_bs, 1);
    m_processThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
    for(int64_t done = 0; done < samples;)
    {
      const int64_t n = std::min(block, samples - done);
      m_blockStart = offset + done;
      f(offset + done, n);
      done += n;
      if(done < samples)
      {
        time_info.samplePos += n;
        if(ppq_per_frame > 0.)
        {
          time_info.ppqPos += n * ppq_per_frame;
          // The bar the block starts in
          const double bar = time_info.timeSigDenominator > 0
                                 ? 4. * time_info.timeSigNumerator
                                       / time_info.timeSigDenominator
                                 : 0.;
          if(bar > 0. && time_info.ppqPos >= time_info.barStartPos + bar)
            time_info.barStartPos
                += std::floor((time_info.ppqPos - time_info.barStartPos) / bar) * bar;
        }
      }
    }
    m_processThread.store(std::thread::id{}, std::memory_order_relaxed);
  }

  void processFloat(int64_t offset, int64_t samples)
  {
    forEachBlock(offset, samples, [this](int64_t o, int64_t n) { processFloatBlock(o, n); });
  }

  void processDouble(int64_t offset, int64_t samples)
  {
    forEachBlock(offset, samples, [this](int64_t o, int64_t n) { processDoubleBlock(o, n); });
  }

  void processFloatBlock(int64_t offset, int64_t samples)
  {
    if constexpr(!UseDouble)
    {
      if(samples <= 0)
        return;

      const auto max_i = std::max(2, this->fx->fx->numInputs);
      const auto max_o = std::max(2, this->fx->fx->numOutputs);
      const auto max_io = std::max(max_i, max_o);

      const auto max_samples = std::max(samples, (int64_t)m_bs); // * 16;

      //qDebug() << samples << m_bs << max_samples << offset << " ::: " << max_i << max_o << max_io;

      // prepare ossia::graph_node buffers
      auto& ip = prepareInput(offset, samples);
      SCORE_ASSERT(ip.size() >= 2);

      auto& op = prepareOutput(offset, samples);
      SCORE_ASSERT(op.size() >= 2);

      // copy io
      float_v[0].resize(max_samples);
      float_v[1].resize(max_samples);

      std::copy_n(ip[0].data() + offset, samples, float_v[0].data());
      std::copy_n(ip[1].data() + offset, samples, float_v[1].data());

      float** io = (float**)alloca(sizeof(float*) * max_io);
      io[0] = float_v[0].data();
      io[1] = float_v[1].data();

      if(max_io > 2)
      {
        // Note that alloca has *function* scope, not block scope so it is
        // freed at the end
        float* dummy = (float*)alloca(sizeof(float) * max_samples);
        std::fill_n(dummy, max_samples, 0.f);

        for(int i = 2; i < max_io; i++)
          io[i] = dummy;
      }

      fx->fx->processReplacing(fx->fx, io, io, samples);

      std::copy_n(float_v[0].data(), samples, op[0].data() + offset);
      std::copy_n(float_v[1].data(), samples, op[1].data() + offset);

      float_v[0].clear();
      float_v[1].clear();
    }
  }

  void processDoubleBlock(int64_t offset, int64_t samples)
  {
    if constexpr(UseDouble)
    {
      if(samples <= 0)
        return;

      const auto max_i = std::max(2, this->fx->fx->numInputs);
      const auto max_o = std::max(2, this->fx->fx->numOutputs);
      const auto max_io = std::max(max_i, max_o);

      // copy audio data
      auto& ip = prepareInput(offset, samples);
      SCORE_ASSERT(ip.size() >= 2);

      auto& op = prepareOutput(offset, samples);
      SCORE_ASSERT(op.size() >= 2);

      double** input = (double**)alloca(sizeof(double*) * max_i);
      input[0] = ip[0].data() + offset;
      input[1] = ip[1].data() + offset;

      double** output = (double**)alloca(sizeof(double*) * max_o);
      output[0] = op[0].data() + offset;
      output[1] = op[1].data() + offset;

      if(max_io > 2)
      {
        // Note that alloca has *function* scope, not block scope so it is
        // freed at the end
        double* dummy = (double*)alloca(sizeof(double) * m_bs);
        std::fill_n(dummy, m_bs, 0.);
        for(int i = 2; i < this->fx->fx->numInputs; i++)
          input[i] = dummy;
        for(int i = 2; i < this->fx->fx->numOutputs; i++)
          output[i] = dummy;
      }

      fx->fx->processDoubleReplacing(fx->fx, input, output, samples);
    }
  }

  struct dummy_t
  {
  };
  std::conditional_t<!UseDouble, std::array<ossia::float_vector, 2>, dummy_t> float_v;

  //! Events from ticks that covered no whole sample, waiting for the next block
  //! that does cover one.
  ossia::small_vector<libremidi::ump, 8> m_deferred;

  //! The system-exclusive messages of one process call: the VST 2 events and
  //! the bytes they point at stay put until the plug-in has processed them.
  Process::MidiSysexArena::stream m_sysexStream;
  Process::MidiSysexArena m_sysexArena;
  std::array<VstMidiSysexEvent, 128> m_sysexEvents{};
};

template <bool b1, bool b2, typename... Args>
auto make_vst_fx(Args&... args)
{
  return ossia::make_node<vst_node<b1, b2>>(args...);
}
}
