// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetOutput.hpp"

#include <Midi/MidiProcess.hpp>

#include <score/model/path/PathSerialization.hpp>

namespace Midi
{

SetChannel::SetChannel(const ProcessModel& model, int n)
    : m_model{model}, m_old{model.channel()}, m_new{n}
{
}

void SetChannel::undo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setChannel(m_old);
}

void SetChannel::redo(const score::DocumentContext& ctx) const
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

SetRange::SetRange(const ProcessModel& model, int min, int max)
    : m_model{model}
    , m_oldmin{model.range().first}
    , m_newmin{min}
    , m_oldmax{model.range().second}
    , m_newmax{max}
{
}

void SetRange::undo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setRange(m_oldmin, m_oldmax);
}

void SetRange::redo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setRange(m_newmin, m_newmax);
}

void SetRange::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_oldmin << m_newmin << m_oldmax << m_newmax;
}

void SetRange::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_oldmin >> m_newmin >> m_oldmax >> m_newmax;
}
}
