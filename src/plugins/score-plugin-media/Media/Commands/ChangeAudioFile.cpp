#include "ChangeAudioFile.hpp"

#include <Media/Sound/SoundModel.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/path/PathSerialization.hpp>
namespace Media
{
ChangeAudioFile::ChangeAudioFile(const Sound::ProcessModel& model, const QString& text)
    : m_model{model}, m_new{text}
{
  m_old = model.file()->originalFile();
  m_oldloop = model.loopDuration();
  if (auto p = qobject_cast<Scenario::IntervalModel*>(model.parent()))
  {
    m_olddur = p->duration.defaultDuration();
  }

  auto& info = AudioDecoder::database()[m_new];
  if (info.length != 0)
  {
    m_newdur = info.duration();
  }
}

void ChangeAudioFile::undo(const score::DocumentContext& ctx) const
{
  if (m_newdur != TimeVal::zero())
  {
    auto& snd = m_model.find(ctx);
    snd.setFile(m_old);
    if (auto itv = qobject_cast<Scenario::IntervalModel*>(snd.parent()))
    {
      if (itv->processes.size() == 1)
      {
        if (auto fact = ctx.app.interfaces<Scenario::IntervalResizerList>().find(*itv))
        {
          auto cmd = fact->make(*itv, m_olddur);
          cmd->redo(ctx);
          delete cmd;
        }
      }
    }
    snd.setLoopDuration(m_olddur);
  }
}

void ChangeAudioFile::redo(const score::DocumentContext& ctx) const
{
  if (m_newdur != TimeVal::zero())
  {
    auto& snd = m_model.find(ctx);
    snd.setFile(m_new);
    if (auto itv = qobject_cast<Scenario::IntervalModel*>(snd.parent()))
    {
      if (itv->processes.size() == 1)
      {
        if (auto fact = ctx.app.interfaces<Scenario::IntervalResizerList>().find(*itv))
        {
          auto cmd = fact->make(*itv, m_newdur);
          cmd->redo(ctx);
          delete cmd;
        }
      }
    }
    snd.setLoopDuration(m_newdur);
  }
}

void ChangeAudioFile::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new << m_olddur << m_newdur << m_oldloop;
}

void ChangeAudioFile::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new >> m_olddur >> m_newdur >> m_oldloop;
}
}
