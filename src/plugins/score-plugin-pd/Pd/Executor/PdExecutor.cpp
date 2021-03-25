#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Dataflow/Port.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/dataflow/nodes/sound.hpp>

#include <QFileInfo>

#include <Pd/Executor/PdExecutor.hpp>
#include <Pd/PdProcess.hpp>

// We do this to pevent picking up a potential system /usr/inclde/m_pd.h
#include "../../3rdparty/libpd/pure-data/src/m_pd.h"
#include <z_libpd.h>

#include <vector>
namespace Pd
{

struct ossia_to_pd_value
{
  const char* mess{};
  void operator()() const {}
  template <typename T>
  void operator()(const T&) const
  {
  }

  void operator()(float f) const { libpd_float(mess, f); }
  void operator()(int f) const { libpd_float(mess, f); }
  void operator()(const std::string& f) const
  {
    libpd_symbol(mess, f.c_str());
  }
  void operator()(const ossia::impulse& f) const { libpd_bang(mess); }

  // TODO convert other types
};

struct libpd_list_wrapper
{
  t_atom* impl{};
  int size{};

  template <typename Functor>
  void iterate(const Functor& f) const
  {
    for (auto it = impl; it; it = libpd_next_atom(it))
    {
      if (libpd_is_float(it))
        f(libpd_get_float(it));
      else if (libpd_is_symbol(it))
        f(libpd_get_symbol(it));
    }
  }

  struct value_visitor
  {
    std::vector<ossia::value>& v;
    void operator()(float f) const { v.push_back(f); }
    void operator()(const char* s) const { v.push_back(std::string(s)); }
  };

  std::vector<ossia::value> to_list()
  {
    std::vector<ossia::value> vals;
    vals.reserve(size);
    iterate(value_visitor{vals});
    return vals;
  }
};

PdGraphNode::PdGraphNode(
    ossia::string_view folder,
    ossia::string_view file,
    const Execution::Context& ctx,
    std::size_t audio_inputs,
    std::size_t audio_outputs,
    Process::Inlets inport,
    Process::Outlets outport,
    bool midi_in,
    bool midi_out)
    : m_audioIns{audio_inputs}, m_audioOuts{audio_outputs}, m_file{file}
{
  m_currentInstance = nullptr;

  for (auto proc : inport)
  {
    if (proc->type() == Process::PortType::Message)
      m_inmess.push_back(proc->customData().toStdString());
  }
  for (auto proc : outport)
  {
    if (proc->type() == Process::PortType::Message)
      m_outmess.push_back(proc->customData().toStdString());
  }
  // Set-up buffers
  const std::size_t bs = libpd_blocksize();
  m_inbuf.resize(m_audioIns * bs);
  m_outbuf.resize(m_audioOuts * bs);
  m_prev_outbuf.resize(m_audioOuts);
  for (auto& circ : m_prev_outbuf)
    circ.set_capacity(8192);

  // Create instance
  m_instance = pdinstance_new();
  pd_setinstance(m_instance);

  // Enable audio
  libpd_init_audio(m_audioIns, m_audioOuts, ctx.execState->sampleRate);

  libpd_start_message(1);
  libpd_add_float(1.0f);
  libpd_finish_message("pd", "dsp");

  // Open
  auto handle = libpd_openfile(file.data(), folder.data());
  m_dollarzero = libpd_getdollarzero(handle);

  for (auto& mess : m_inmess)
    add_dzero(mess);
  for (auto& mess : m_outmess)
    add_dzero(mess);

  // Create correct I/Os
  bool hasAudioIn = m_audioIns > 0;
  bool hasAudioOut = m_audioOuts > 0;
  int audioInlets = hasAudioIn ? 1 : 0;
  int audioOutlets = hasAudioOut ? 1 : 0;

  m_inlets.reserve(audioInlets + m_inmess.size() + int(midi_in));
  if (hasAudioIn)
  {
    auto port = new ossia::audio_inlet;
    m_audio_inlet = port->target<ossia::audio_port>();
    m_inlets.push_back(std::move(port));
  }
  if (midi_in)
  {
    auto port = new ossia::midi_inlet;
    m_midi_inlet = port->target<ossia::midi_port>();
    m_inlets.push_back(std::move(port));
  }
  m_firstInMessage = m_inlets.size();
  for (std::size_t i = 0; i < m_inmess.size(); i++)
  {
    auto port = new ossia::value_inlet;
    m_inlets.push_back(port);
  }

  m_outlets.reserve(audioOutlets + m_outmess.size() + int(midi_out));
  if (hasAudioOut)
  {
    auto port = new ossia::audio_outlet;
    m_audio_outlet = port->target<ossia::audio_port>();
    m_outlets.push_back(std::move(port));
  }
  if (midi_out)
  {
    auto port = new ossia::midi_outlet;
    m_midi_outlet = port->target<ossia::midi_port>();
    m_outlets.push_back(std::move(port));
  }
  m_firstOutMessage = m_outlets.size();
  for (std::size_t i = 0; i < m_outmess.size(); i++)
  {
    libpd_bind(m_outmess[i].c_str());
    auto port = new ossia::value_outlet;
    m_outlets.push_back(port);
  }

  // Set-up message callbacks
  libpd_set_printhook([](const char* s) { qDebug() << "string: " << s; });

  libpd_set_floathook([](const char* recv, float f) {
    if (auto v = m_currentInstance->get_value_port(recv))
    {
      v->write_value(f, {}); // TODO set correct offset
    }
  });
  libpd_set_banghook([](const char* recv) {
    if (auto v = m_currentInstance->get_value_port(recv))
    {
      v->write_value(ossia::impulse{}, {}); // TODO set correct offset
    }
  });
  libpd_set_symbolhook([](const char* recv, const char* sym) {
    if (auto v = m_currentInstance->get_value_port(recv))
    {
      v->write_value(std::string(sym), {}); // TODO set correct offset
    }
  });

  libpd_set_listhook([](const char* recv, int argc, t_atom* argv) {
    if (auto v = m_currentInstance->get_value_port(recv))
    {
      v->write_value(
          libpd_list_wrapper{argv, argc}.to_list(),
          {}); // TODO set correct offset
    }
  });
  libpd_set_messagehook(
      [](const char* recv, const char* msg, int argc, t_atom* argv) {
        if (auto v = m_currentInstance->get_value_port(recv))
        {
          v->write_value(
              libpd_list_wrapper{argv, argc}.to_list(),
              {}); // TODO set correct offset
        }
      });

  libpd_set_noteonhook([](int channel, int pitch, int velocity) {
    if (auto v = m_currentInstance->get_midi_out())
    {
      v->messages.push_back(
          (velocity > 0)
              ? libremidi::message::note_on(channel, pitch, velocity)
              : libremidi::message::note_off(channel, pitch, velocity));
    }
  });
  libpd_set_controlchangehook([](int channel, int controller, int value) {
    if (auto v = m_currentInstance->get_midi_out())
    {
      v->messages.push_back(
          libremidi::message::control_change(channel, controller, value + 8192));
    }
  });

  libpd_set_programchangehook([](int channel, int value) {
    if (auto v = m_currentInstance->get_midi_out())
    {
      v->messages.push_back(libremidi::message::program_change(channel, value));
    }
  });
  libpd_set_pitchbendhook([](int channel, int value) {
    if (auto v = m_currentInstance->get_midi_out())
    {
      v->messages.push_back(libremidi::message::pitch_bend(channel, value));
    }
  });
  libpd_set_aftertouchhook([](int channel, int value) {
    if (auto v = m_currentInstance->get_midi_out())
    {
      v->messages.push_back(libremidi::message::aftertouch(channel, value));
    }
  });
  libpd_set_polyaftertouchhook([](int channel, int pitch, int value) {
    if (auto v = m_currentInstance->get_midi_out())
    {
      v->messages.push_back(
          libremidi::message::poly_pressure(channel, pitch, value));
    }
  });
  libpd_set_midibytehook([](int port, int byte) {
    // TODO
  });
}

PdGraphNode::~PdGraphNode()
{
  pdinstance_free(m_instance);
}

ossia::outlet* PdGraphNode::get_outlet(const char* str) const
{
  ossia::string_view s = str;
  auto it = ossia::find(m_outmess, s);
  if (it != m_outmess.end())
    return m_outlets[std::distance(m_outmess.begin(), it) + m_audioOuts];

  return nullptr;
}

ossia::value_port* PdGraphNode::get_value_port(const char* str) const
{
  return get_outlet(str)->target<ossia::value_port>();
}

ossia::midi_port* PdGraphNode::get_midi_in() const
{
  return m_midi_inlet;
}

ossia::midi_port* PdGraphNode::get_midi_out() const
{
  return m_midi_outlet;
}

void PdGraphNode::run(
    const ossia::token_request& t_,
    ossia::exec_state_facade e) noexcept
{
  auto t = t_;
  // Setup
  pd_setinstance(m_instance);
  m_currentInstance = this;
  libpd_init_audio(m_audioIns, m_audioOuts, e.sampleRate());

  const uint64_t bs = libpd_blocksize();

  // Clear audio inputs
  ossia::fill(m_inbuf, 0.);

  // Copy audio inputs
  if (m_audioIns > 0)
  {
    for (std::size_t i = 0U;
         i < std::min(m_audioIns, m_audio_inlet->samples.size());
         i++)
    {
      std::copy_n(
          m_audio_inlet->samples[i].begin(),
          std::min(
              (std::size_t)bs, (std::size_t)m_audio_inlet->samples[i].size()),
          m_inbuf.begin() + i * bs);
    }
  }

  // Copy midi inputs
  if (m_midi_inlet)
  {
    auto& dat = m_midi_inlet->messages;
    for (const auto& mess : dat)
    {
      switch (mess.get_message_type())
      {
        case libremidi::message_type::NOTE_OFF:
          libpd_noteon(mess.get_channel() - 1, mess.bytes[1], 0);
          break;
        case libremidi::message_type::NOTE_ON:
          libpd_noteon(mess.get_channel() - 1, mess.bytes[1], mess.bytes[2]);
          break;
        case libremidi::message_type::POLY_PRESSURE:
          libpd_polyaftertouch(
              mess.get_channel() - 1, mess.bytes[1], mess.bytes[2]);
          break;
        case libremidi::message_type::CONTROL_CHANGE:
          libpd_controlchange(
              mess.get_channel() - 1, mess.bytes[1], mess.bytes[2]);
          break;
        case libremidi::message_type::PROGRAM_CHANGE:
          libpd_programchange(mess.get_channel() - 1, mess.bytes[1]);
          break;
        case libremidi::message_type::AFTERTOUCH:
          libpd_aftertouch(mess.get_channel() - 1, mess.bytes[1]);
          break;
        case libremidi::message_type::PITCH_BEND:
          libpd_pitchbend(mess.get_channel() - 1, mess.bytes[1] - 8192);
          break;
        case libremidi::message_type::INVALID:
        default:
          break;
      }
    }

    dat.clear();
  }

  // Copy message inputs
  for (std::size_t i = 0; i < m_inmess.size(); ++i)
  {
    auto& dat = m_inlets[m_firstInMessage + i]
                    ->target<ossia::value_port>()
                    ->get_data();

    auto mess = m_inmess[i].c_str();

    for (auto& val : dat)
    {
      val.value.apply(ossia_to_pd_value{mess});
    }
  }

  // Compute number of samples to process
  uint64_t req_samples = t.physical_write_duration(e.modelToSamples());
  if (m_audioOuts == 0)
  {
    libpd_process_raw(m_inbuf.data(), m_outbuf.data());
  }
  else if (req_samples > m_prev_outbuf[0].size())
  {
    int64_t additional_samples
        = std::max(bs, req_samples - m_prev_outbuf[0].size());
    while (additional_samples > 0)
    {
      // TODO remove the first n samples of audio input so that it makes sense
      libpd_process_raw(m_inbuf.data(), m_outbuf.data());
      for (std::size_t i = 0; i < m_audioOuts; ++i)
      {
        for (std::size_t j = 0; j < bs; j++)
        {
          m_prev_outbuf[i].push_back(m_outbuf[i * bs + j]);
        }
      }
      additional_samples -= bs;
    }
  }

  if (m_audioOuts > 0)
  {
    // Copy audio outputs. Message inputs are copied in callbacks.
    // Note: due to Pd processing samples 64 by 64 this is not sample-accurate.
    // i.e. we always start copying from the beginning of the latest buffer
    // computed by pd. The solution is to store the last N samples computed and
    // read them if necessary, but then this causes problems if messages &
    // parameters changed in between.

    auto& ap = m_audio_outlet->samples;
    ap.resize(m_audioOuts);
    for (std::size_t i = 0U; i < m_audioOuts; ++i)
    {
      ap[i].resize(std::max(uint64_t(ap[i].size()), uint64_t(t.offset.impl)));
      for (std::size_t j = 0U; j < req_samples; j++)
      {
        ap[i].push_back(m_prev_outbuf[i].front());
        m_prev_outbuf[i].pop_front();
      }

      ossia::snd::do_fade(
          t.start_discontinuous,
          t.end_discontinuous,
          ap[i],
          t.offset.impl,
          t.offset.impl + req_samples);
    }
  }

  // Teardown
  m_currentInstance = nullptr;
}

void PdGraphNode::add_dzero(std::string& s) const
{
  s = std::to_string(m_dollarzero) + "-" + s;
}

class pd_process final : public ossia::node_process
{
public:
  using ossia::node_process::node_process;
  void start() override {}
};

Component::Component(
    Pd::ProcessModel& element,
    const ::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::ProcessComponent{element, ctx, id, "PdComponent", parent}
{
  QFileInfo f(element.script());
  if (!f.exists())
  {
    qDebug() << "Missing script. Returning";
    return;
  }

  const auto& model_inlets = element.inlets();
  const auto& model_outlets = element.outlets();

  std::vector<std::string> in_mess, out_mess;
  for (std::size_t i = 0; i < model_inlets.size(); i++)
  {
    const Process::Port& e = *model_inlets[i];
    if (e.type() == Process::PortType::Message)
      in_mess.push_back(e.customData().toStdString());
  }

  for (std::size_t i = 0; i < model_outlets.size(); i++)
  {
    const Process::Port& e = *model_outlets[i];
    if (e.type() == Process::PortType::Message)
      out_mess.push_back(e.customData().toStdString());
  }

  node = std::make_shared<PdGraphNode>(
      f.canonicalPath().toStdString(),
      f.fileName().toStdString(),
      ctx,
      element.audioInputs(),
      element.audioOutputs(),
      model_inlets,
      model_outlets,
      element.midiInput(),
      element.midiOutput());

  m_ossia_process = std::make_shared<pd_process>(node);
}

Component::~Component() {}

PdGraphNode* PdGraphNode::m_currentInstance{};
}
