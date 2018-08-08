#include "SoundDrop.hpp"

#include <Loop/LoopProcessModel.hpp>
#include <Media/AudioDecoder.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Commands/CreateSoundBox.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <QApplication>
#include <QMimeData>
#include <QUrl>
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
namespace Media
{
namespace Sound
{
static void createSoundProcesses(
    Scenario::Command::Macro& m,
    const Scenario::IntervalModel& interval,
    DroppedAudioFiles& drop)
{
  for (auto&& file : drop.files)
  {
    // TODO set file directly
    // Create sound process
    auto& proc = m.createProcess<Media::Sound::ProcessModel>(interval, {});
    m.submit(new Media::ChangeAudioFile{proc, std::move(file)});
    m.addLayerInNewSlot(interval, proc);
  }
}

DroppedAudioFiles::DroppedAudioFiles(const QMimeData& mime)
{
  for (auto url : mime.urls())
  {
    QString filename = url.toLocalFile();
    if (!MediaFileHandle::isAudioFile(QFile{filename}))
      continue;

    files.emplace_back(filename);

    AudioDecoder dec;
    if (auto info_opt = dec.probe(filename))
    {
      auto info = *info_opt;
      if (info.channels > 0 && info.length > 0)
      {
        maxDuration = std::max((int64_t)maxDuration, (int64_t)info.length);
      }
    }
  }
}

TimeVal DroppedAudioFiles::dropMaxDuration() const
{
  return TimeVal::fromMsecs(
      maxDuration / 44.1); // TODO use settings Samplerate
}

bool DropHandler::drop(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  DroppedAudioFiles drop{mime};
  if (!drop.valid())
  {
    return false;
  }

  // We add the files in sequence, or in the same interval
  // if shift isn't pressed.
  if (qApp->keyboardModifiers() & Qt::ShiftModifier)
  {
    return createInSequence(pres, pos, std::move(drop));
  }
  else
  {
    return createInParallel(pres, pos, std::move(drop));
  }
}

bool DropHandler::createInSequence(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    DroppedAudioFiles&& audio)
{
  SCORE_TODO;
  return true;
}

bool DropHandler::createInParallel(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    DroppedAudioFiles&& drop)
{
  Scenario::Command::Macro m{
    new Media::CreateSoundBoxMacro,
    pres.context().context};

  // Create a box.
  const Scenario::ProcessModel& scenar = pres.model();
  Scenario::Point pt = pres.toScenarioPoint(pos);

  TimeVal t = drop.dropMaxDuration();

  // Create the beginning
  auto& interval = m.createBox(scenar, pt.date, pt.date + t, pt.y);

  // Add sound processes as fit.
  createSoundProcesses(m, interval, drop);

  // Finally we show the newly created rack
  m.showRack(interval);

  m.commit();

  return true;
}

// TODO put me in some "algorithms" file.
static bool intervalHasNoFollowers(
    const Scenario::ProcessModel& scenar, const Scenario::IntervalModel& cst)
{
  auto& tn = Scenario::endTimeSync(cst, scenar);
  for (auto& event_id : tn.events())
  {
    Scenario::EventModel& event = scenar.events.at(event_id);
    for (auto& state_id : event.states())
    {
      Scenario::StateModel& state = scenar.states.at(state_id);
      if (state.nextInterval())
        return false;
    }
  }
  return true;
}

bool IntervalDropHandler::drop(
    const Scenario::IntervalModel& interval, const QMimeData& mime)
{
  DroppedAudioFiles drop{mime};
  if (!drop.valid())
  {
    return false;
  }

  auto& doc = score::IDocument::documentContext(interval);
  Scenario::Command::Macro m{new Media::CreateSoundBoxMacro, doc};

  // TODO dynamic_safe_cast ? for non-static-castable types, have the compiler
  // enforce dynamic_cast ?
  auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(interval.parent());
  SCORE_ASSERT(scenar);

  // If the interval has no processes and nothing after, we will resize it
  if (interval.processes.empty())
  {
    // TODO refactor me in a general "move event in scenario interface" way.
    if (auto loop = dynamic_cast<Loop::ProcessModel*>(scenar))
    {
      auto resize_cmd
          = new Scenario::Command::MoveBaseEvent<Loop::ProcessModel>{
              *loop,
              loop->endEvent().id(),
              drop.dropMaxDuration(),
              interval.heightPercentage(),
              ExpandMode::GrowShrink,
              LockMode::Free};
      m.submit(resize_cmd);
    }
    else if (
        auto scenar = dynamic_cast<Scenario::ProcessModel*>(interval.parent()))
    {
      // First check that the end time sync has nothing afterwards :
      // all its states must not have next intervals
      if (intervalHasNoFollowers(*scenar, interval))
      {
        auto& ev = Scenario::endState(interval, *scenar).eventId();
        auto resize_cmd = new Scenario::Command::MoveEventMeta{
            *scenar,
            ev,
            interval.date() + drop.dropMaxDuration(),
            interval.heightPercentage(),
            ExpandMode::GrowShrink,
            LockMode::Free};
        m.submit(resize_cmd);
      }
    }
  }

  // Add the processes
  createSoundProcesses(m, interval, drop);

  // Show the rack
  m.showRack(interval);

  m.commit();

  return false;
}
}
}
