// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/ExecutionSetup.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <YSFX/ProcessModel.hpp>

#include <score/serialization/AnySerialization.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_edge_helpers.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>

#include <QEventLoop>

#include <utility>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTimer>

#include <libremidi/detail/conversion.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace YSFX
{
namespace Executor
{
class ysfx_node final : public ossia::graph_node
{
public:
  ysfx_node(
      bool audio_in, bool midi_in, bool audio_out, bool midi_out,
      std::vector<uint32_t> sliders, std::shared_ptr<ysfx_t>, std::shared_ptr<StateGuard>,
      ossia::execution_state& st);

  void run(const ossia::token_request& t, ossia::exec_state_facade) noexcept override;
  [[nodiscard]] std::string label() const noexcept override
  {
    return fmt::format("ysfx ({})", ysfx_get_name(fx.get()));
  }

  void all_notes_off() noexcept override;
  void release_notes() noexcept;
  //! The events of the input this tick takes, [from; to[ in the buffer
  std::pair<int64_t, int64_t>
  midi_span(ossia::exec_state_facade estate, int64_t tick_start, int64_t d) noexcept;
  void stash_midi(int64_t from, int64_t to) noexcept;
  void clear() noexcept override
  {
    graph_node::clear();
    generation = -1;
  }

  std::shared_ptr<ysfx_t> fx;
  std::shared_ptr<StateGuard> m_guard;
  ossia::execution_state& m_st;

  ossia::audio_inlet* audio_in{};
  ossia::midi_inlet* midi_in{};

  //! Events from ticks that covered no whole sample, waiting for the next block
  //! that does cover one.
  ossia::small_vector<libremidi::ump, 8> m_deferred_midi;
  //! Every tick of a buffer sees all of the buffer's events: the buffer the
  //! last tick was in, where it started, and up to where it took them
  int64_t m_midiBuffer{-1};
  int64_t m_midiTickStart{};
  int64_t m_midiFrom{};
  bool m_midiTickEmpty{};
  ossia::midi_outlet* midi_out{};
  ossia::audio_outlet* audio_out{};
  //! One port per control of the process, in the same order; slider_index is
  //! the number of the script's slider each one sets
  std::vector<ossia::value_port*> sliders;
  std::vector<uint32_t> slider_index;
  int generation = 0;
  //! An all-notes-off that came while the script's state was in use
  bool m_pendingNotesOff{};
};

Component::Component(
    YSFX::ProcessModel& proc, const ::Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<YSFX::ProcessModel, ossia::node_process>{
        proc, ctx, "YSFXComponent", parent}
{
  // Handle live-coding: when the program changes, recreate the node
  connect(&proc, &YSFX::ProcessModel::programChanged, this, [this] {
    ++generation;
    for(auto& c : this->m_controlConnections)
      QObject::disconnect(c);
    m_controlConnections.clear();

    auto& ctx = system();
    Execution::SetupContext& setup = ctx.setup;
    auto old_node = this->node;

    Execution::Transaction commands{ctx};
    if(old_node)
    {
      setup.unregister_node(process(), this->node, commands);
    }

    reload(commands);

    if(this->node)
    {
      setup.register_node(process(), this->node, commands);
      nodeChanged(old_node, this->node, &commands);
    }

    commands.run_all();
  }, Qt::DirectConnection);

  Execution::Transaction commands{ctx};
  reload(commands);
  commands.run_all();
}

void Component::reload(Execution::Transaction& transaction)
{
  auto& proc = process();
  auto& ctx = system();

  if(!proc.fx)
    return;

  const int N_in = proc.inlets().size();
  const bool has_audio_in
      = !proc.inlets().empty()
        && bool(qobject_cast<Process::AudioInlet*>(proc.inlets()[0]));
  bool has_midi_in = false;
  for(int i = has_audio_in ? 1 : 0; i < std::min(2, N_in); i++)
  {
    has_midi_in |= bool(qobject_cast<Process::MidiInlet*>(proc.inlets()[i]));
  }

  const int N_out = proc.outlets().size();
  const bool has_audio_out
      = !proc.outlets().empty()
        && bool(qobject_cast<Process::AudioOutlet*>(proc.outlets()[0]));
  bool has_midi_out = false;
  for(int i = has_audio_out ? 1 : 0; i < std::min(2, N_out); i++)
  {
    has_midi_out |= bool(qobject_cast<Process::MidiOutlet*>(proc.outlets()[i]));
  }

  // A control sets the slider its id names (see ProcessModel::recreatePorts):
  // a script may skip slider numbers, and the controls of a loaded document
  // are the ones it was saved with.
  const int firstControlIndex = int(has_audio_in) + int(has_midi_in);
  std::vector<uint32_t> sliders;
  for(int i = firstControlIndex; i < N_in; i++)
    sliders.push_back(uint32_t(proc.inlets()[i]->id().val() - 4));

  std::shared_ptr<ysfx_node> node = ossia::make_node<ysfx_node>(
      *ctx.execState, has_audio_in, has_midi_in, has_audio_out, has_midi_out,
      std::move(sliders), proc.fx, proc.stateGuard, *ctx.execState);
  node->generation = this->generation;
  this->node = node;

  if(!m_ossia_process)
    m_ossia_process = std::make_shared<ossia::node_process>(node);
  else
    ctx.setup.replace_node(m_ossia_process, node, transaction);

  for(std::size_t i = firstControlIndex; i < N_in; i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);

    auto inl = node->sliders[i - firstControlIndex];
    inlet->setupExecution(*node->root_inputs()[i], this);
    auto c = connect(
        inlet, &Process::ControlInlet::valueChanged, this,
        [this, inl, weak_node = std::weak_ptr{node}, gen = this->generation](const ossia::value& v) {
      if(auto node = weak_node.lock())
        system().executionQueue.enqueue([inl, val = v, weak_node, gen]() mutable {
          if(auto n = weak_node.lock(); n && n->generation == gen)
            inl->write_value(std::move(val), 0);
        });
    });
    m_controlConnections.push_back(c);
  }

  auto c = con(
      ctx.doc.coarseUpdateTimer, &QTimer::timeout, this,
      [weak_node = std::weak_ptr{node}, p = QPointer{&proc}] {
    if(!p)
      return;
    auto node = weak_node.lock();
    if(!node)
      return;
    auto y = p->fx.get();
    if(!y)
      return;

#if __has_include(<ysfx-s.h>)
    if(std::bitset<64> res = ysfx_fetch_slider_changes(y, 0); res.any())
#else
    if(std::bitset<64> res = ysfx_fetch_slider_changes(y); res.any())
#endif
    {
      for(int i = 0; i < 64; i++)
      {
        if(res.test(i))
        {
          // See ProcessModel.hpp around the loop:
          // for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
          int idx = 4 + i;
          if(auto inl
             = static_cast<Process::ControlInlet*>(p->inlet(Id<Process::Port>{idx})))
            inl->setExecutionValue(ysfx_slider_get_value(y, i));
          else
            qDebug() << "Error while trying to access inlet " << idx;
        }
      }
    }
  });
  m_controlConnections.push_back(c);
}

Component::~Component() { }

ysfx_node::ysfx_node(
    bool has_audio_in, bool has_midi_in, bool has_audio_out, bool has_midi_out,
    std::vector<uint32_t> slider_numbers, std::shared_ptr<ysfx_t> ffx,
    std::shared_ptr<StateGuard> guard, ossia::execution_state& st)
    : fx{std::move(ffx)}
    , m_guard{std::move(guard)}
    , m_st{st}
{
  auto y = this->fx.get();

  m_deferred_midi.reserve(1024);
  slider_index = std::move(slider_numbers);

  // The node it replaces may still be running the same script
  {
    StateLock lock{*m_guard};
    ysfx_set_block_size(y, st.bufferSize);
    ysfx_set_sample_rate(y, st.sampleRate);
    ysfx_set_midi_capacity(y, 512, true);
    ysfx_init(y);
  }

  if(has_audio_in)
    this->m_inlets.push_back(audio_in = new ossia::audio_inlet);

  if(has_midi_in)
    this->m_inlets.push_back(midi_in = new ossia::midi_inlet);

  if(has_audio_out)
    this->m_outlets.push_back(audio_out = new ossia::audio_outlet);

  if(has_midi_out)
    this->m_outlets.push_back(midi_out = new ossia::midi_outlet);

  for(const uint32_t i : slider_index)
  {
    auto inl = new ossia::value_inlet;
    this->m_inlets.push_back(inl);
    this->sliders.push_back(&**inl);
    if(ysfx_slider_exists(y, i) && !ysfx_slider_is_enum(y, i)
       && !ysfx_slider_is_path(y, i))
    {
      ysfx_slider_range_t range{};
      ysfx_slider_get_range(y, i, &range);

      (*inl)->domain = ossia::make_domain(range.min, range.max);
    }
  }
}

struct ysfx_midi_event_impl : ysfx_midi_event_t
{
  unsigned char bytes[4];
};

void ysfx_node::run(
    const ossia::token_request& tk, ossia::exec_state_facade estate) noexcept
{
  assert(fx);
  auto y = this->fx.get();

  const auto [tick_start, d] = estate.timings(tk);
  const auto [midi_from, midi_to] = midi_span(estate, tick_start, d);
  if(d <= 0)
  {
    // Nothing to render, but the tick's events still belong to the sample the
    // span starts on, which is offset 0 of the next block that covers one.
    stash_midi(midi_from, midi_to);
    return;
  }

  // A preset, an undo or a save is reading or writing the script's state:
  // this tick is silent, its events go to the next one.
  if(!m_guard->try_enter())
  {
    stash_midi(midi_from, midi_to);
    if(audio_out)
      for(auto& chan : audio_out->data)
      {
        if(chan.size() < std::size_t(tick_start + d))
          chan.resize(tick_start + d);
        std::fill_n(chan.data() + tick_start, d, 0.);
      }
    return;
  }
  struct leave_guard
  {
    StateGuard& guard;
    ~leave_guard() { guard.leave(); }
  } leave{*m_guard};

  if(std::exchange(m_pendingNotesOff, false))
    release_notes();

  // Setup audio input
  double** ins{};
  int in_count{};
  if(audio_in)
  {
    in_count = this->audio_in->data.channels();
    if(in_count < (int)ysfx_get_num_inputs(y))
    {
      in_count = ysfx_get_num_inputs(y);
      this->audio_in->data.set_channels(in_count);
    }
    ins = (double**)alloca(sizeof(double*) * in_count);
    for(int i = 0; i < in_count; i++)
    {
      {
        this->audio_in->data.get()[i].resize(estate.bufferSize());
        ins[i] = this->audio_in->data.channel(i).data() + tick_start;
      }
    }
  }

  // Setup audio output
  double** outs{};
  int out_count{};
  if(audio_out)
  {
    audio_out->data.set_channels(ysfx_get_num_outputs(y));
    out_count = this->audio_out->data.channels();
    outs = (double**)alloca(sizeof(double*) * out_count);
    for(int i = 0; i < out_count; i++)
    {
      auto& chan = this->audio_out->data.get()[i];
      if(tick_start == 0 && d == estate.bufferSize())
      {
        chan.resize(estate.bufferSize(), boost::container::default_init);
      }
      else if(chan.size() != std::size_t(estate.bufferSize()))
      {
        // The other ticks of this buffer write the rest of it
        chan.resize(estate.bufferSize());
      }
      outs[i] = chan.data() + tick_start;
    }
  }

  // ysfx writes the outputs only from @sample: without one, the signal goes
  // through as it came in, as in a host that processes in place
  for(int i = 0; i < out_count; i++)
  {
    if(i < in_count)
      std::copy_n(ins[i], d, outs[i]);
    else
      std::fill_n(outs[i], d, 0.);
  }

  // Setup controls
  for(std::size_t i = 0; i < sliders.size(); i++)
  {
    auto& vp = *sliders[i];
    auto& dat = vp.get_data();

    if(!dat.empty())
    {
      auto& val = dat.back().value;
      const uint32_t index = slider_index[i];
      if(float* v = val.target<float>())
      {
#if __has_include(<ysfx-s.h>)
        ysfx_slider_set_value(y, index, *v, true);
#else
        ysfx_slider_set_value(y, index, *v);
#endif
      }
      else if(int* v = val.target<int>())
      {
#if __has_include(<ysfx-s.h>)
        ysfx_slider_set_value(y, index, *v, true);
#else
        ysfx_slider_set_value(y, index, *v);
#endif
      }
    }
  }

  // Setup midi. Anything held back from a tick that covered no whole sample
  // goes first, at offset 0.
  if(midi_in)
  {
    const auto& msgs = this->midi_in->data.messages;
    if(!m_deferred_midi.empty() || !msgs.empty())
    {
      ossia::small_vector<std::pair<const libremidi::ump*, int64_t>, 16> to_send;
      for(const auto& mess : m_deferred_midi)
        to_send.push_back({&mess, 0});
      for(const auto& mess : msgs)
        if(mess.timestamp >= midi_from && mess.timestamp < midi_to)
          to_send.push_back(
              {&mess,
               std::clamp<int64_t>(mess.timestamp - tick_start, 0, d > 0 ? d - 1 : 0)});

      // alloca is function-scoped
      auto msg_space
          = (ysfx_midi_event_impl*)alloca(sizeof(ysfx_midi_event_impl) * (1 + to_send.size()));
      int i = 0;
      for(const auto& [mess_ptr, sample_offset] : to_send)
      {
        ysfx_midi_event_impl& ev = msg_space[i];

        ev.bus = 0; // FIXME
        ev.offset = sample_offset;
        ev.data = ev.bytes;
        ev.size = cmidi2_convert_single_ump_to_midi1(
            (uint8_t*)ev.data, sizeof(ysfx_midi_event_impl::bytes),
            (cmidi2_ump*)mess_ptr->data);
        if(ev.size > 0)
        {
          ysfx_send_midi(y, &ev);
          i++;
        }
      }
      m_deferred_midi.clear();
    }
  }

  ysfx_time_info_t info;
  info.time_position = tk.prev_date.impl * estate.modelToSamples() / estate.sampleRate();
  info.time_signature[0] = tk.signature.upper;
  info.time_signature[1] = tk.signature.lower;
  info.beat_position = tk.musical_start_position;
  info.playback_state = ysfx_playback_playing;
  info.tempo = tk.tempo;

  ysfx_set_time_info(y, &info);
  ysfx_process_double(y, ins, outs, in_count, out_count, d);

  if(midi_out)
  {
    ysfx_midi_event_t ev;
    while(ysfx_receive_midi(y, &ev))
    {
      libremidi::ump msg;
      msg.timestamp = tick_start + ev.offset;

      if(cmidi2_midi1_channel_voice_to_midi2(ev.data, ev.size, msg.data))
      {
        this->midi_out->data.messages.push_back(msg);
      }
    }
  }
}

std::pair<int64_t, int64_t> ysfx_node::midi_span(
    ossia::exec_state_facade estate, int64_t tick_start, int64_t d) noexcept
{
  // The ticks of a buffer come in order and do not overlap: each takes the
  // events from where the one before it ended, and the first one of a buffer
  // also those before it, which no tick of this node covers. A tick that
  // covers no sample takes those of the sample it is on.
  const int64_t buffer = estate.samplesSinceStart();
  const bool new_buffer
      = buffer != m_midiBuffer || tick_start < m_midiTickStart
        || (tick_start == m_midiTickStart && !m_midiTickEmpty);
  if(new_buffer)
    m_midiFrom = std::numeric_limits<int64_t>::min();
  m_midiBuffer = buffer;
  m_midiTickStart = tick_start;
  m_midiTickEmpty = d <= 0;

  const int64_t from = m_midiFrom;
  m_midiFrom = tick_start + std::max<int64_t>(d, 1);
  return {from, m_midiFrom};
}

void ysfx_node::stash_midi(int64_t from, int64_t to) noexcept
{
  if(!this->midi_in)
    return;
  for(const auto& mess : this->midi_in->data.messages)
  {
    if(m_deferred_midi.size() >= 1024)
      break;
    if(mess.timestamp >= from && mess.timestamp < to)
      m_deferred_midi.push_back(mess);
  }
}

void ysfx_node::all_notes_off() noexcept
{
  if(!m_guard->try_enter())
  {
    m_pendingNotesOff = true;
    return;
  }
  release_notes();
  m_guard->leave();
}

void ysfx_node::release_notes() noexcept
{
  auto y = this->fx.get();
  for(int channel = 0; channel < 16; channel++)
  {
    ysfx_midi_event_impl all_notes_off;

    all_notes_off.bus = 0;
    all_notes_off.offset = 0;
    all_notes_off.size = 3;
    all_notes_off.data = all_notes_off.bytes;

    all_notes_off.bytes[0] = (char)(uint8_t)176 + channel;
    all_notes_off.bytes[1] = (char)(uint8_t)123;
    all_notes_off.bytes[2] = 0;
    ysfx_send_midi(y, &all_notes_off);

    ysfx_midi_event_impl all_sounds_off;
    all_sounds_off.bus = 0;
    all_sounds_off.offset = 0;
    all_sounds_off.size = 3;
    all_sounds_off.data = all_sounds_off.bytes;

    all_sounds_off.bytes[0] = (char)(uint8_t)176 + channel;
    all_sounds_off.bytes[1] = (char)(uint8_t)121;
    all_sounds_off.bytes[2] = 0;
    ysfx_send_midi(y, &all_sounds_off);
  }

  for(int note = 0; note < 127; note++)
  {
    ysfx_midi_event_impl note_off;

    note_off.bus = 0;
    note_off.offset = 0;
    note_off.size = 3;
    note_off.data = note_off.bytes;

    note_off.bytes[0] = (char)(uint8_t)128;
    note_off.bytes[1] = (char)(uint8_t)note;
    note_off.bytes[2] = 0;
    ysfx_send_midi(y, &note_off);
  }

  // Silence in, output discarded, for 16 * 4096 frames: long enough for the
  // releases to finish. Small blocks keep the stack bounded with many channels.
  constexpr int block = 256;
  int ins = ysfx_get_num_inputs(y);
  auto ins_p = (double**)alloca(sizeof(double*) * (ins + 1));
  for(int i = 0; i < ins + 1; i++)
  {
    ins_p[i] = (double*)alloca(sizeof(double) * block);
    std::fill_n(ins_p[i], block, 0.);
  }
  int outs = ysfx_get_num_outputs(y);
  auto outs_p = (double**)alloca(sizeof(double*) * (outs + 1));
  for(int i = 0; i < outs + 1; i++)
  {
    outs_p[i] = (double*)alloca(sizeof(double) * block);
    std::fill_n(outs_p[i], block, 0.);
  }

  for(int i = 0; i < 16 * 4096 / block; i++)
    ysfx_process_double(y, ins_p, outs_p, ins, outs, block);
}
}
}
