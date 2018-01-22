#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>
#include <Process/Dataflow/Port.hpp>


namespace Scenario
{
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT SetControlValue final : public score::Command
{
    SCORE_COMMAND_DECL(
        ScenarioCommandFactoryName(), SetControlValue, "Set a control")

public:

    SetControlValue(const Process::ControlInlet& obj, ossia::value newval)
        : m_path{obj}
        , m_old{obj.value()}
        , m_new{newval}
    {
    }

    virtual ~SetControlValue() { }

    void undo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).setValue(m_old);
    }

    void redo(const score::DocumentContext& ctx) const final override
    {
      m_path.find(ctx).setValue(m_new);
    }

    void update(const Process::ControlInlet& obj, ossia::value newval)
    {
      m_new = std::move(newval);
    }

protected:
    void serializeImpl(DataStreamInput& stream) const final override
    {
      stream << m_path << m_old << m_new;
    }
    void deserializeImpl(DataStreamOutput& stream) final override
    {
      stream >> m_path >> m_old >> m_new;
    }

  private:
    Path<Process::ControlInlet> m_path;
    ossia::value m_old, m_new;
};

}
}
