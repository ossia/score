#pragma once
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ControlSurface/CommandFactory.hpp>
#include <ControlSurface/Process.hpp>

namespace ControlSurface
{

class AddControlMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddControlMacro, "Add controls")
};

class AddControl : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddControl, "Add a control")
public:
  AddControl(const score::DocumentContext& ctx, Id<Process::Port> id, const Model& proc, const State::Message& p)
      : m_model{proc}
      , m_id{std::move(id)}
      , m_addr{Explorer::makeFullAddressAccessorSettings(p.address, ctx, 0., 1.)}
  {
    m_addr.value = p.value;
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& proc = m_model.find(ctx);
    proc.removeControl(m_id);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& proc = m_model.find(ctx);
    proc.addControl(m_id, m_addr);
  }

private:
  void serializeImpl(DataStreamInput& s) const override { s << m_model << m_id << m_addr; }

  void deserializeImpl(DataStreamOutput& s) override { s >> m_model >> m_id >> m_addr; }

  Path<Model> m_model;
  Id<Process::Port> m_id;
  Device::FullAddressAccessorSettings m_addr;
};

class RemoveControl : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveControl, "Remove a control")
public:
  RemoveControl(const Model& proc, const Process::Port& p)
      : m_model{proc}
      , m_id{p.id()}
      , m_addr{proc.outputAddresses().at(p.id().val())}
      , m_data{DataStreamReader::marshall(p)}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& proc = m_model.find(ctx);

    static auto& cl = ctx.app.components.interfaces<Process::PortFactoryList>();
    Process::Port* ctl = deserialize_interface(cl, DataStreamWriter{m_data}, &proc);

    proc.setupControl(safe_cast<Process::ControlInlet*>(ctl), m_addr);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& proc = m_model.find(ctx);
    proc.removeControl(m_id);
  }

private:
  void serializeImpl(DataStreamInput& s) const override { s << m_model << m_id << m_data; }

  void deserializeImpl(DataStreamOutput& s) override { s >> m_model >> m_id >> m_data; }

  Path<Model> m_model;
  Id<Process::Port> m_id;
  State::AddressAccessor m_addr;
  QByteArray m_data;
};

}
