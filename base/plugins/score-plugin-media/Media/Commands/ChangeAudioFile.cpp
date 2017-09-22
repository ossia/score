#include "ChangeAudioFile.hpp"

#include <score/model/path/PathSerialization.hpp>
#include <Media/Sound/SoundModel.hpp>
namespace Media
{
namespace Commands
{

ChangeAudioFile::ChangeAudioFile(
        const Sound::ProcessModel& model,
        const QString& text):
    m_model{model},
    m_new{text}
{
    m_old = model.file().name();
}

void ChangeAudioFile::undo(const score::DocumentContext& ctx) const
{
    m_model.find(ctx).setFile(m_old);
}

void ChangeAudioFile::redo(const score::DocumentContext& ctx) const
{
    m_model.find(ctx).setFile(m_new);
}

void ChangeAudioFile::serializeImpl(DataStreamInput& s) const
{
    s << m_model << m_old << m_new;
}

void ChangeAudioFile::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_old >> m_new;
}

}
}
