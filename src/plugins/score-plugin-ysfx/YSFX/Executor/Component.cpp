// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

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
#include <QQmlComponent>
#include <QQmlContext>
#include <QTimer>

#include <libremidi/detail/conversion.hpp>

#include <vector>

namespace YSFX
{
namespace Executor
{
class ysfx_node final : public ossia::graph_node
{
public:
  ysfx_node(std::shared_ptr<ysfx_t>, ossia::execution_state& st);

  void run(const ossia::token_request& t, ossia::exec_state_facade) noexcept override;
  [[nodiscard]] std::string label() const noexcept override
  {
    return fmt::format("ysfx ({})", ysfx_get_name(fx.get()));
  }

  void all_notes_off() noexcept override;

  std::shared_ptr<ysfx_t> fx;
  ossia::execution_state& m_st;

  ossia::audio_inlet* audio_in{};
  ossia::midi_inlet* midi_in{};
  ossia::midi_outlet* midi_out{};
  ossia::audio_outlet* audio_out{};
  std::vector<ossia::value_port*> sliders;
};

Component::Component(
    YSFX::ProcessModel& proc, const ::Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<YSFX::ProcessModel, ossia::node_process>{
        proc, ctx, "YSFXComponent", parent}
{
  std::shared_ptr<ysfx_node> node
      = ossia::make_node<ysfx_node>(*ctx.execState, proc.fx, *ctx.execState);
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  int firstControlIndex = ysfx_get_num_inputs(proc.fx.get()) > 0 ? 2 : 1;
  for(std::size_t i = firstControlIndex, N = proc.inlets().size(); i < N; i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    // *node->controls[i - firstControlIndex].second
    //     = ossia::convert<float>(inlet->value());

    auto inl = node->sliders[i - firstControlIndex];
    inlet->setupExecution(*node->root_inputs()[i], this);
    connect(
        inlet, &Process::ControlInlet::valueChanged, this,
        [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue(
          [inl, val = v]() mutable { inl->write_value(std::move(val), 0); });
        });
  }

  auto c = con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [node, &proc] {
    auto y = proc.fx.get();
    if(std::bitset<64> res = ysfx_fetch_slider_changes(y); res.any())
    {
      for(int i = 0; i < 64; i++)
      {
        if(res.test(i))
        {
          // See ProcessModel.hpp around the loop:
          // for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
          int idx = 4 + i;
          if(auto inl
             = static_cast<Process::ControlInlet*>(proc.inlet(Id<Process::Port>{idx})))
            inl->setExecutionValue(ysfx_slider_get_value(y, i));
          else
            qDebug() << "Error while trying to access inlet " << idx;
        }
      }
    }
  });
}

Component::~Component() { }

ysfx_node::ysfx_node(std::shared_ptr<ysfx_t> ffx, ossia::execution_state& st)
    : fx{std::move(ffx)}
    , m_st{st}
{
  auto y = this->fx.get();

  ysfx_set_block_size(y, st.bufferSize);
  ysfx_set_sample_rate(y, st.sampleRate);
  ysfx_set_midi_capacity(y, 512, true);
  ysfx_init(y);

  if(ysfx_get_num_inputs(y) > 0)
  {
    this->m_inlets.push_back(audio_in = new ossia::audio_inlet);
  }

  this->m_inlets.push_back(midi_in = new ossia::midi_inlet);

  if(ysfx_get_num_outputs(y) > 0)
  {
    this->m_outlets.push_back(audio_out = new ossia::audio_outlet);
  }

  this->m_outlets.push_back(midi_out = new ossia::midi_outlet);

  for(uint32_t i = 0; i < ysfx_max_sliders; ++i)
  {
    auto inl = new ossia::value_inlet;
    this->m_inlets.push_back(inl);
    this->sliders.push_back(&**inl);
    if(ysfx_slider_is_enum(y, i))
    {
    }
    else if(ysfx_slider_is_path(y, i))
    {
    }
    else
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
        this->audio_in->data.get()[i].resize(d);
        ins[i] = this->audio_in->data.channel(i).data();
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
      this->audio_out->data.get()[i].resize(
          estate.bufferSize(), boost::container::default_init);
      outs[i] = this->audio_out->data.channel(i).data();
    }
  }

  // Setup controls
  for(std::size_t i = 0; i < sliders.size(); i++)
  {
    auto& vp = *sliders[i];
    auto& dat = vp.get_data();

    if(!dat.empty())
    {
      auto& val = dat.back().value;
      if(float* v = val.target<float>())
      {
        ysfx_slider_set_value(y, i, *v);
      }
      else if(int* v = val.target<int>())
      {
        ysfx_slider_set_value(y, i, *v);
      }
    }
  }

  // Setup midi
  if(midi_in)
  {
    if(!this->midi_in->data.messages.empty())
    {
      // alloca is function-scoped
      auto msg_space = (ysfx_midi_event_impl*)alloca(
          sizeof(ysfx_midi_event_impl) * (1 + this->midi_in->data.messages.size()));
      int i = 0;
      for(auto& mess : this->midi_in->data.messages)
      {
        ysfx_midi_event_impl& ev = msg_space[i];

        ev.bus = 0; // FIXME
        ev.offset = mess.timestamp;
        ev.data = ev.bytes;
        ev.size = cmidi2_convert_single_ump_to_midi1(
            (uint8_t*)ev.data, sizeof(ysfx_midi_event_impl::bytes), mess.data);
        if(ev.size > 0)
        {
          ysfx_send_midi(y, &ev);
          i++;
        }
      }
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

  ysfx_midi_event_t ev;
  while(ysfx_receive_midi(y, &ev))
  {
    libremidi::ump msg;
    msg.timestamp = ev.offset;

    if(cmidi2_midi1_channel_voice_to_midi2(ev.data, ev.size, msg.data))
    {
      this->midi_out->data.messages.push_back(msg);
    }
  }
}

void ysfx_node::all_notes_off() noexcept
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

  int ins = ysfx_get_num_inputs(y);
  auto ins_p = (double**)alloca(sizeof(double*) * (ins + 1));
  for(int i = 0; i < ins + 1; i++)
  {
    ins_p[i] = (double*)alloca(sizeof(double) * 4096);
    std::fill_n(ins_p[i], 8, 0.);
  }
  int outs = ysfx_get_num_outputs(y);
  auto outs_p = (double**)alloca(sizeof(double*) * (outs + 1));
  for(int i = 0; i < outs + 1; i++)
  {
    outs_p[i] = (double*)alloca(sizeof(double) * 4096);
    std::fill_n(outs_p[i], 8, 0.);
  }

  for(int i = 0; i < 16; i++)
    ysfx_process_double(y, ins_p, outs_p, ins, outs, 4096);
}
}
}
