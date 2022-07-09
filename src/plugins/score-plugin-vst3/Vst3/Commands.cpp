#include "Commands.hpp"

#include <Process/Dataflow/PortSerialization.hpp>
#include <Vst3/Control.hpp>
#include <Vst3/EffectModel.hpp>

#include <score/model/path/PathSerialization.hpp>
namespace vst3
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"3"};
  return key;
}

SetControl::SetControl(const ControlInlet& obj, float newval)
    : m_path{obj}
    , m_old{obj.value()}
    , m_new{newval}
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

CreateControl::CreateControl(const Model& obj, Steinberg::Vst::ParamID fxNum)
    : m_path{obj}
    , m_fxNum{fxNum}
{
}

CreateControl::~CreateControl() { }

void CreateControl::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).removeControl(m_fxNum);
}

void CreateControl::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).addControlFromEditor(m_fxNum);
}

void CreateControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << static_cast<uint32_t>(m_fxNum);
}

void CreateControl::deserializeImpl(DataStreamOutput& stream)
{
  uint32_t fxNum;
  stream >> m_path >> fxNum;
  m_fxNum = fxNum;
}

RemoveControl::RemoveControl(const Model& obj, Id<Process::Port> id)
    : m_path{obj}
    , m_id{std::move(id)}
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

  auto inlet = Process::load_inlet(wr, &vst).release();
  SCORE_ASSERT(inlet);

  auto vst_inlet = qobject_cast<ControlInlet*>(inlet);
  SCORE_ASSERT(vst_inlet);
  vst.on_addControl_impl(vst_inlet);

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
