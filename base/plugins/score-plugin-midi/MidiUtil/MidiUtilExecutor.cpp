// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiUtilExecutor.hpp"
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/midi/detail/midi_impl.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <MidiUtil/MidiUtilProcess.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace MidiUtil
{
namespace Executor
{
enum class scale
{
  ionian, dorian, phyrgian, lydian, mixolydian, aeolian, locrian,

  C, D, E, F, G, A, B
};

class midi_node
    : public ossia::graph_node
{
  public:
    midi_node()
    {
      // input
      m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());

      // scale
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

      // mode
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

      // transpose
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());


      // output
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
    }

    ~midi_node() override
    {

    }

    void set_scale(scale c)
    {

    }

    void set_base(int base)
    {

    }

    void set_transpose(int x)
    {

    }

  private:
    void run(ossia::token_request t, ossia::execution_state& e) override
    {
    }

};

Component::Component(
    MidiUtil::ProcessModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<MidiUtil::ProcessModel, ossia::node_process>{
        element, ctx, id, "MidiComponent", parent}
{
/*
  auto node = std::make_shared<midi_node>();
  auto proc = std::make_shared<ossia::node_process>(node);
  m_ossia_process = proc;
  m_node = node;
  ctx.plugin.outlets.insert({element.outlet.get(), {m_node, node->outputs()[0]}});
  ctx.plugin.execGraph->add_node(m_node);
*/
}

Component::~Component()
{
  m_node->clear();
  system().plugin.execGraph->remove_node(m_node);
}
}
}
