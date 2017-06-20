#include "SetOutput.hpp"
#include <Midi/MidiProcess.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Midi
{

SetOutput::SetOutput(const ProcessModel& model, const QString& dev)
    : m_model{model}, m_old{model.device()}, m_new{dev}
{
}

void SetOutput::undo(const iscore::DocumentContext& ctx) const
{
  m_model.find(ctx).setDevice(m_old);
}

void SetOutput::redo(const iscore::DocumentContext& ctx) const
{
  m_model.find(ctx).setDevice(m_new);
}

void SetOutput::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void SetOutput::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}

SetChannel::SetChannel(const ProcessModel& model, int n)
    : m_model{model}, m_old{model.channel()}, m_new{n}
{
}

void SetChannel::undo(const iscore::DocumentContext& ctx) const
{
  m_model.find(ctx).setChannel(m_old);
}

void SetChannel::redo(const iscore::DocumentContext& ctx) const
{
  m_model.find(ctx).setChannel(m_new);
}

void SetChannel::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void SetChannel::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}
}
