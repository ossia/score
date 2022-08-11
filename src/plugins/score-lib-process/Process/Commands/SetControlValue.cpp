#include "SetControlValue.hpp"

namespace Process
{

SetControlValue::SetControlValue(const ControlInlet& obj, ossia::value newval)
    : m_path{obj}
    , m_old{obj.value()}
    , m_new{newval}
{
}

SetControlValue::~SetControlValue() { }

void SetControlValue::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_old);
}

void SetControlValue::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_new);
}

void SetControlValue::update(const ControlInlet& obj, ossia::value newval)
{
  m_new = std::move(newval);
}

void SetControlValue::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_old << m_new;
}

void SetControlValue::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_old >> m_new;
}

SetControlOutletValue::SetControlOutletValue(
    const ControlOutlet& obj, ossia::value newval)
    : m_path{obj}
    , m_old{obj.value()}
    , m_new{newval}
{
}

SetControlOutletValue::~SetControlOutletValue() { }

void SetControlOutletValue::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_old);
}

void SetControlOutletValue::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setValue(m_new);
}

void SetControlOutletValue::update(const ControlOutlet& obj, ossia::value newval)
{
  m_new = std::move(newval);
}

void SetControlOutletValue::serializeImpl(DataStreamInput& stream) const
{
  stream << m_path << m_old << m_new;
}

void SetControlOutletValue::deserializeImpl(DataStreamOutput& stream)
{
  stream >> m_path >> m_old >> m_new;
}

}
