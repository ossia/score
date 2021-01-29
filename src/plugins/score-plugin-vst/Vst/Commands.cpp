#include "Commands.hpp"

#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>

#include <score/model/path/PathSerialization.hpp>
namespace Vst
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Vst"};
  return key;
}

SetControl::SetControl(const ControlInlet& obj, float newval)
    : m_path{obj}, m_old{obj.value()}, m_new{newval}
{
}

SetControl::~SetControl() { }

void SetControl::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_old);
}

void SetControl::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_new);
}

void SetControl::update(const ControlInlet& obj, float newval)
{
  m_new = newval;
}

void SetControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_old << m_new;
}

void SetControl::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_old >> m_new;
}

CreateControl::CreateControl(const Model& obj, int fxNum, float value)
    : m_path{obj}, m_fxNum{fxNum}, m_val{value}
{
}

CreateControl::~CreateControl() { }

void CreateControl::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).removeControl(m_fxNum);
}

void CreateControl::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).on_addControl(m_fxNum, m_val);
}

void CreateControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_fxNum << m_val;
}

void CreateControl::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_fxNum >> m_val;
}

RemoveControl::RemoveControl(const Model& obj, Id<Process::Port> id)
    : m_path{obj}, m_id{std::move(id)}
{
  auto& inlet = *obj.inlet(m_id);
  m_control = score::marshall<DataStream>(inlet);
  m_cables = Dataflow::saveCables({&inlet}, score::IDocument::documentContext(obj));
}

RemoveControl::~RemoveControl() { }

void RemoveControl::undo(const score::DocumentContext& ctx) const
{
  auto& vst = m_path.find(ctx);
  DataStreamWriter wr{m_control};

  vst.on_addControl_impl(
      qobject_cast<Vst::ControlInlet*>(Process::load_value_inlet(wr, &vst).release()));

  Dataflow::restoreCables(m_cables, ctx);
}

void RemoveControl::redo(const score::DocumentContext& ctx) const
{
  Dataflow::removeCables(m_cables, ctx);

  m_path.find(ctx).removeControl(m_id);
}

void RemoveControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_id << m_control << m_cables;
}

void RemoveControl::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_id >> m_control >> m_cables;
}
}
