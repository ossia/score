#pragma once
/*
#include <Automation/AutomationModel.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Process/Process.hpp>

#include <Pd/Executor/PdExecutor.hpp>
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/value/value.hpp>

#include <iscore_addon_pd_export.h>

namespace Dataflow
{
class AutomNode : public Process::Node
{
  public:
    AutomNode(
          Dataflow::DocumentPlugin& doc,
          Automation::ProcessModel& proc,
          Id<Node> c,
          QObject* parent);
    Automation::ProcessModel& process;
    QString getText() const override { return process.address().toString(); }
    std::size_t audioInlets() const override { return 0; }
    std::size_t messageInlets() const override { return 0; }
    std::size_t midiInlets() const override { return 0; }

    std::size_t audioOutlets() const override { return 0; }
    std::size_t messageOutlets() const override{ return 1; }
    std::size_t midiOutlets() const override{ return 0; }

    std::vector<Process::Port> inlets() const override { return {}; }
    std::vector<Process::Port> outlets() const override {
      std::vector<Process::Port> v(1);
      Process::Port& p = v[0];
      p.address = process.address();
      p.type = Process::PortType::Message;
      p.propagate = true;
      return v;
    }

    ~AutomNode()
    {
      cleanup();
    }

    std::vector<Id<Process::Cable>> cables() const override
    { return m_cables; }
    void addCable(Id<Process::Cable> c) override
    { m_cables.push_back(c); }
    void removeCable(Id<Process::Cable> c) override
    { m_cables.erase(ossia::find(m_cables, c)); }

  private:
    std::vector<Id<Process::Cable>> m_cables;
};

class AutomationComponent :
    public ProcessComponent_T<Automation::ProcessModel>
{
    COMPONENT_METADATA("17b049e1-222e-4f86-b879-eccb05937cbb")
    public:
      AutomationComponent(
        Automation::ProcessModel& autom,
        DocumentPlugin& doc,
        const Id<iscore::Component>& id,
        QObject* parent_obj);

    AutomNode& mainNode() override { return m_node; }
  private:
    AutomNode m_node;
};

using AutomationComponentFactory = ProcessComponentFactory_T<AutomationComponent>;

class ISCORE_ADDON_PD_EXPORT AutomationGraphNode final :
    public ossia::graph_node
{
  public:
    AutomationGraphNode(
        std::shared_ptr<ossia::curve_abstract> curve,
        ossia::val_type addr)
      : m_curve{curve}
      , m_type{addr}
    {
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }

    ~AutomationGraphNode()
    {

    }

  private:
    void run(ossia::execution_state& e) override;

    std::shared_ptr<ossia::curve_abstract> m_curve{};
    ossia::val_type m_type{};
};

class AutomExecComponent final
    : public ::Engine::Execution::
    ProcessComponent_T<Automation::ProcessModel, ossia::node_process>
{
    COMPONENT_METADATA("f3ac9746-e994-42cc-a3d5-cfef89bcb7aa")
    public:
      AutomExecComponent(
        Engine::Execution::ConstraintComponent& parentConstraint,
        Automation::ProcessModel& element,
        const Dataflow::DocumentPlugin& df,
        const ::Engine::Execution::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent);

    ~AutomExecComponent()
    {
      if(node) node->clear();
    }

  private:
    void recompute();

    std::shared_ptr<ossia::curve_abstract>
    on_curveChanged(ossia::val_type, const optional<ossia::Destination>&);

    template <typename T>
    std::shared_ptr<ossia::curve_abstract>
    on_curveChanged_impl(const optional<ossia::Destination>&);

    ossia::node_ptr node;
    const Dataflow::DocumentPlugin& m_df;
};
using AutomExecComponentFactory
= Pd::ProcessComponentFactory_T<AutomExecComponent>;

}*/
