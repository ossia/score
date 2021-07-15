#pragma once
#include <Pd/PdInstance.hpp>
#include <Explorer/DeviceList.hpp>
#include <Pd/PdProcess.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/string_view.hpp>
#include <ossia/editor/scenario/time_process.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <boost/circular_buffer.hpp>

#include <QJSEngine>
#include <QJSValue>
#include <QString>

#include <memory>

namespace Pd
{

class ProcessModel;

class PdGraphNode final : public ossia::graph_node
{
public:
  PdGraphNode(
      std::shared_ptr<Instance> instance,
      ossia::string_view folder,
      ossia::string_view file,
      const Execution::Context& ctx,
      std::size_t audio_inputs,
      std::size_t audio_outputs,
      Process::Inlets inmess,
      Process::Outlets outmess,
      const Pd::PatchSpec& spec,
      bool midi_in = true,
      bool midi_out = true);

  ~PdGraphNode();

  ossia::outlet* get_outlet(const char* str) const;

  ossia::value_port* get_value_port(const char* str) const;

  ossia::midi_port* get_midi_in() const;
  ossia::midi_port* get_midi_out() const;

  void
  run(const ossia::token_request& t,
      ossia::exec_state_facade e) noexcept override;
  void add_dzero(std::string& s) const;

  std::shared_ptr<Instance> m_instance;

  std::size_t m_audioIns{};
  std::size_t m_audioOuts{};
  std::vector<Process::Port*> m_inport, m_outport;
  std::vector<std::string> m_inmess, m_outmess;

  std::vector<float> m_inbuf, m_outbuf;
  std::vector<boost::circular_buffer<float>> m_prev_outbuf;
  std::size_t m_firstInMessage{}, m_firstOutMessage{};
  ossia::audio_port* m_audio_inlet{};
  ossia::audio_port* m_audio_outlet{};
  ossia::midi_port* m_midi_inlet{};
  ossia::midi_port* m_midi_outlet{};
  std::string m_file;
};

class Component final : public Execution::ProcessComponent
{
  COMPONENT_METADATA("78657f42-3a2a-4b80-8736-8736463442b4")

public:
  using model_type = Pd::ProcessModel;
  Component(
      Pd::ProcessModel& element,
      const Execution::Context& ctx,
      QObject* parent);

  ~Component();
};

using ComponentFactory = Execution::ProcessComponentFactory_T<Pd::Component>;
}
