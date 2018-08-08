#include "ChangeAudioFile.hpp"

#include <Media/Sound/SoundModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
namespace Media
{
ChangeAudioFile::ChangeAudioFile(
    const Sound::ProcessModel& model, const QString& text)
    : m_model{model}, m_new{text}
{
  m_old = model.file().path();
  if(auto p = qobject_cast<Scenario::IntervalModel*>(model.parent()))
  {
    m_olddur = p->duration.defaultDuration();
  }
}

void ChangeAudioFile::undo(const score::DocumentContext& ctx) const
{
  auto& snd = m_model.find(ctx);
  snd.setFile(m_old);
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(snd.parent()))
  {
    auto& s = *dynamic_cast<Scenario::ScenarioInterface*>(itv->parent());
    s.changeDuration(*itv, m_olddur);
  }
}

void ChangeAudioFile::redo(const score::DocumentContext& ctx) const
{
  auto& snd = m_model.find(ctx);
  snd.setFile(m_new);
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(snd.parent()))
  {
    auto& info = AudioDecoder::database()[m_new];

    auto& s = *dynamic_cast<Scenario::ScenarioInterface*>(itv->parent());
    if(info.length != 0)
    {
      s.changeDuration(*itv, TimeVal::fromMsecs(1000. * double(info.length) / double(info.rate)));
    }
  }
}

void ChangeAudioFile::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new << m_olddur;
}

void ChangeAudioFile::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new >> m_olddur;
}
}
