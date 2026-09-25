#pragma once
#include <Process/Preset.hpp>

#include <score/command/Command.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/tools/Unused.hpp>

#include <ossia/network/value/value.hpp>

#include <score_lib_process_export.h>

namespace Process
{
class ProcessModel;
class ControlInlet;
class SCORE_LIB_PROCESS_EXPORT LoadPresetCommandFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(LoadPresetCommandFactory, "4a1a228c-16af-4647-917e-09cdf63fe167")
public:
  virtual ~LoadPresetCommandFactory();
  virtual bool matches(
      const Process::ProcessModel& obj, unused_t newval,
      const score::DocumentContext& ctx) const noexcept
      = 0;
  virtual score::Command* make(
      const Process::ProcessModel& obj, Process::Preset newval,
      const score::DocumentContext& ctx) const
      = 0;
};

class SCORE_LIB_PROCESS_EXPORT LoadPresetCommandFactoryList final
    : public score::MatchingFactory<LoadPresetCommandFactory>
{
public:
  ~LoadPresetCommandFactoryList();
};

//! Sets a control that changes the ports of its process, keeping the removed
//! ports and cables for undo. The value can change until it is pushed.
class SCORE_LIB_PROCESS_EXPORT ChangePortsCommand : public score::Command
{
public:
  using score::Command::Command;
  ~ChangePortsCommand() override;
  virtual void setNewValue(ossia::value v) = 0;
};

class SCORE_LIB_PROCESS_EXPORT ChangePortsCommandFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ChangePortsCommandFactory, "0f5dcb0e-7e8a-4a4e-9a86-3a0b8d3c52e1")
public:
  virtual ~ChangePortsCommandFactory();
  virtual bool matches(
      const Process::ControlInlet& obj, unused_t newval,
      const score::DocumentContext& ctx) const noexcept
      = 0;
  virtual ChangePortsCommand* make(
      const Process::ControlInlet& obj, ossia::value newval,
      const score::DocumentContext& ctx) const
      = 0;
};

class SCORE_LIB_PROCESS_EXPORT ChangePortsCommandFactoryList final
    : public score::MatchingFactory<ChangePortsCommandFactory>
{
public:
  ~ChangePortsCommandFactoryList();
};

//! Null unless the control changes the ports of its process and a factory matches
SCORE_LIB_PROCESS_EXPORT ChangePortsCommand* makeChangePortsCommand(
    const Process::ControlInlet& obj, ossia::value newval,
    const score::DocumentContext& ctx);

//! A ChangePortsCommand when the control changes the ports of its process
SCORE_LIB_PROCESS_EXPORT score::Command* makeSetControlCommand(
    const Process::ControlInlet& obj, ossia::value newval,
    const score::DocumentContext& ctx);
}
