#pragma once
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>
#include <iscore_plugin_scenario_export.h>
#include <ossia/dataflow/graph_node.hpp>
namespace Dataflow
{
class Slider;
class constraint_node : public ossia::graph_node
{
public:
  constraint_node()
  {
    // todo maybe we can optimize by having m_outlets == m_inlets
    // this way no copy.
    m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
    m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
    m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());

    m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
    m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
  }

  void run(ossia::execution_state&) override
  {
    {
      auto i = m_inlets[0]->data.target<ossia::audio_port>();
      auto o = m_outlets[0]->data.target<ossia::audio_port>();
      o->samples = std::move(i->samples);
    }

    {
      auto i = m_inlets[1]->data.target<ossia::value_port>();
      auto o = m_outlets[1]->data.target<ossia::value_port>();
      o->data = std::move(i->data);
    }

    {
      auto i = m_inlets[1]->data.target<ossia::midi_port>();
      auto o = m_outlets[1]->data.target<ossia::midi_port>();
      o->messages = std::move(i->messages);
    }
  }
};

/*
class ConstraintBase :
        public Scenario::GenericConstraintComponent<Dataflow::DocumentPlugin>
{
    COMMON_COMPONENT_METADATA("eab98b28-5b0f-4754-aa3a-8d3622eedeea")
public:
    using parent_t = Scenario::GenericConstraintComponent<Dataflow::DocumentPlugin>;
    using DocumentPlugin = Dataflow::DocumentPlugin;
    using model_t = Process::ProcessModel;
    using component_t = Dataflow::ProcessComponent;
    using component_factory_list_t = Dataflow::ProcessComponentFactoryList;

    ConstraintBase(
            const Id<iscore::Component>& id,
            Scenario::ConstraintModel& constraint,
            DocumentPlugin& doc,
            QObject* parent_comp);

    struct ProcessData
    {
        Dataflow::ProcessComponent* component{};
        Slider* mix{};
    };
    ProcessComponent* make(
            const Id<iscore::Component> & id,
            ProcessComponentFactory& factory,
            Process::ProcessModel &process);

    bool removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

    template <typename... Args>
    void removed(Args&&...)
    {
    }

    ConstraintNode& mainNode() { return m_node; }
    void preparePlay();

private:
    void setupProcess(Process::ProcessModel &c, ProcessComponent *comp);
    void teardownProcess(const Process::ProcessModel &c, const ProcessComponent&);

    std::unordered_map<const Process::ProcessModel*, ProcessData> m_processes;
    QVector<Process::Node*> m_sliders;
    ConstraintNode m_node;
};

class Constraint final :
        public iscore::PolymorphicComponentHierarchy<ConstraintBase>
{
public:
    using iscore::PolymorphicComponentHierarchy<ConstraintBase>::PolymorphicComponentHierarchyManager;
};
*/
}
