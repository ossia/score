#pragma once
#include <Process/Commands/ProcessCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Device/Address/AddressSettings.hpp>

#include <State/Address.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>

namespace Process
{

class SCORE_LIB_PROCESS_EXPORT ChangePortSettings final : public score::Command
{
  SCORE_COMMAND_DECL(
      Process::CommandFactoryName(),
      ChangePortSettings,
      "Edit a node port")
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
    Process,
    SetPropagate,
    AudioOutlet::p_propagate,
    "Set port propagation")
SCORE_COMMAND_DECL_T(Process::SetPropagate)

PROPERTY_COMMAND_T(Process, ChangePortAddress, Port::p_address, "Set port address")
SCORE_COMMAND_DECL_T(Process::ChangePortAddress)

PROPERTY_COMMAND_T(Process, SetValue, ControlInlet::p_value, "Set port value")
SCORE_COMMAND_DECL_T(Process::SetValue)

PROPERTY_COMMAND_T(Process, SetGain, AudioOutlet::p_gain, "Set port gain")
SCORE_COMMAND_DECL_T(Process::SetGain)


PROPERTY_COMMAND_T(Process, SetPan, AudioOutlet::p_pan, "Set port pan")
SCORE_COMMAND_DECL_T(Process::SetPan)
