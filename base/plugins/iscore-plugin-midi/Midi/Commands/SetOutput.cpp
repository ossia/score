#include "SetOutput.hpp"
#include <Midi/MidiProcess.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Midi
{

SetOutput::SetOutput(
        const ProcessModel& model,
        const QString& dev):
  m_model{model},
  m_old{model.device()},
  m_new{dev}
{
}

void SetOutput::undo() const
{
    m_model.find().setDevice(m_old);
}

void SetOutput::redo() const
{
    m_model.find().setDevice(m_new);
}

void SetOutput::serializeImpl(DataStreamInput& s) const
{
    s << m_model << m_old << m_new;
}

void SetOutput::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_old >> m_new;
}


}
