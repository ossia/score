#include "VSTCommands.hpp"
#include <Media/Effect/VST/VSTEffectModel.hpp>
namespace Media::VST
{
SetVSTControl::SetVSTControl(const VSTControlInlet& obj, float newval)
: m_path{obj}
, m_old{obj.value()}
, m_new{newval}
{
}

SetVSTControl::~SetVSTControl() { }

void SetVSTControl::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_old);
}

void SetVSTControl::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_new);
}

void SetVSTControl::update(const VSTControlInlet& obj, float newval)
{
  m_new = newval;
}

void SetVSTControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_old << m_new;
}

void SetVSTControl::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_old >> m_new;
}


CreateVSTControl::CreateVSTControl(const VSTEffectModel& obj, int fxNum, float value)
: m_path{obj}
, m_fxNum{fxNum}
, m_val{value}
{
}

CreateVSTControl::~CreateVSTControl() { }

void CreateVSTControl::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).removeControl(m_fxNum);
}

void CreateVSTControl::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).on_addControl(m_fxNum, m_val);
}

void CreateVSTControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_fxNum << m_val;
}

void CreateVSTControl::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_fxNum >> m_val;
}



RemoveVSTControl::RemoveVSTControl(const VSTEffectModel& obj, Id<Process::Port> id)
: m_path{obj}
, m_id{std::move(id)}
, m_control{score::marshall<DataStream>(*obj.inlet(m_id))}
{
}

RemoveVSTControl::~RemoveVSTControl() { }

void RemoveVSTControl::undo(const score::DocumentContext& ctx) const
{
  auto& vst = m_path.find(ctx);
  DataStreamWriter wr{m_control};

  vst.on_addControl_impl(qobject_cast<Media::VST::VSTControlInlet*>(Process::make_inlet(wr, &vst).release()));
}

void RemoveVSTControl::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).removeControl(m_id);
}

void RemoveVSTControl::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_id << m_control;
}

void RemoveVSTControl::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_id >> m_control;
}

}
