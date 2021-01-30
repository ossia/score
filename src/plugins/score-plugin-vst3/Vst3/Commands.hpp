#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>

#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <score/command/Command.hpp>

namespace Process
{
class Port;
class Cable;
class Inlet;
}
namespace vst3
{
const CommandGroupKey& CommandFactoryName();
class Model;
class ControlInlet;
class SetControl final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetControl, "Set a control")

public:
  SetControl(const ControlInlet& obj, float newval);
  virtual ~SetControl();

  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;
  void update(const ControlInlet& obj, float newval);

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<ControlInlet> m_path;
  float m_old, m_new;
};

class CreateControl final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateControl, "Create a control")

public:
  CreateControl(const Model& obj, Steinberg::Vst::ParamID fxNum, float value);
  virtual ~CreateControl();
  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<Model> m_path;
  uint32_t m_fxNum{};
  static_assert(sizeof(Steinberg::Vst::ParamID) == sizeof(uint32_t));

  float m_val{};
};

class RemoveControl final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveControl, "Remove a control")

public:
  RemoveControl(const Model& obj, Id<Process::Port> id);
  virtual ~RemoveControl();
  void undo(const score::DocumentContext& ctx) const final override;
  void redo(const score::DocumentContext& ctx) const final override;

protected:
  void serializeImpl(DataStreamInput& stream) const final override;
  void deserializeImpl(DataStreamOutput& stream) final override;

private:
  Path<Model> m_path;
  Id<Process::Port> m_id;
  QByteArray m_control;
  Dataflow::SerializedCables m_cables;
};
}
