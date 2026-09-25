#pragma once
#include <State/Address.hpp>

#include <Device/Address/AddressSettings.hpp>

#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>

namespace Process
{

class SCORE_LIB_PROCESS_EXPORT ChangePortSettings final : public score::Command
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(), ChangePortSettings, "Edit a node port")
public:
  ChangePortSettings(const Process::Port& p, Device::FullAddressAccessorSettings stgs);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Process::Port> m_model;

  Device::FullAddressAccessorSettings m_old, m_new;
};

}

PROPERTY_COMMAND_T(
    Process, SetPropagate, AudioOutlet::p_propagate, "Set port propagation")
SCORE_COMMAND_DECL_T(Process::SetPropagate)

namespace Process
{
//! The address is re-anchored to what its name publishes on construction
class SCORE_LIB_PROCESS_EXPORT ChangePortAddress final
    : public score::PropertyCommand_T<Port::p_address>
{
  SCORE_COMMAND_DECL(Process::CommandFactoryName(), ChangePortAddress, "Set port address")
public:
  using PropertyCommand_T::PropertyCommand_T;
  ChangePortAddress(const Port& port, State::AddressAccessor address);
};
}
namespace score
{
template <>
template <>
struct PropertyCommand_T<Process::Port::p_address>::command<void>
{
  using type = Process::ChangePortAddress;
};
}

PROPERTY_COMMAND_T(Process, SetValue, ControlInlet::p_value, "Set port value")
SCORE_COMMAND_DECL_T(Process::SetValue)

PROPERTY_COMMAND_T(
    Process, SetPortScriptable, Port::p_scriptable, "Set port scriptable")
SCORE_COMMAND_DECL_T(Process::SetPortScriptable)

PROPERTY_COMMAND_T(
    Process, SetPortScriptingName, Port::p_exposed, "Set port scripting name")
SCORE_COMMAND_DECL_T(Process::SetPortScriptingName)

PROPERTY_COMMAND_T(Process, SetGain, AudioOutlet::p_gain, "Set port gain")
SCORE_COMMAND_DECL_T(Process::SetGain)

PROPERTY_COMMAND_T(Process, SetPan, AudioOutlet::p_pan, "Set port pan")
SCORE_COMMAND_DECL_T(Process::SetPan)
