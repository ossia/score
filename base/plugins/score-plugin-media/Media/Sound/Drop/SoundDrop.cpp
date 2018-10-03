#include "SoundDrop.hpp"

#include <Loop/LoopProcessModel.hpp>
#include <Media/AudioDecoder.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Commands/CreateSoundBox.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Application/Drops/DropHelpers.hpp>

#include <QApplication>
#include <QMimeData>
#include <QUrl>
namespace Media
{
namespace Sound
{
DroppedAudioFiles::DroppedAudioFiles(const QMimeData& mime)
{
  for (const auto& url : mime.urls())
  {
    QString filename = url.toLocalFile();
    if (!MediaFileHandle::isAudioFile(QFile{filename}))
      continue;

    AudioDecoder dec;
    if (auto info_opt = dec.probe(filename))
    {
      auto info = *info_opt;
      if (info.channels > 0 && info.length > 0)
      {
        files.emplace_back(std::make_pair(filename, info.length));
        maxDuration = std::max((int64_t)maxDuration, (int64_t)info.length);
      }
    }
  }
}

TimeVal DroppedAudioFiles::dropMaxDuration() const
{
  // TODO use settings Samplerate
  return TimeVal::fromMsecs(maxDuration / 44.1);
}

bool DropHandler::drop(
    const Scenario::ScenarioPresenter& pres, QPointF pos,
    const QMimeData& mime)
{
  DroppedAudioFiles drop{mime};
  if (!drop.valid())
  {
    return false;
  }

  Scenario::DropProcessInScenario<Media::CreateSoundBoxMacro> dropper{pres, pos, drop.dropMaxDuration()};

  for (auto&& file : drop.files)
  {
    dropper.addProcess(
       [&] (Scenario::Command::Macro& m, Scenario::IntervalModel& interval) -> Process::ProcessModel&
       {
         auto& proc = m.createProcess<Media::Sound::ProcessModel>(interval, {});
         m.submit(new Media::ChangeAudioFile{proc, std::move(file.first)});
         return proc;
       }
     , TimeVal{file.second});
  }

  dropper.commit();
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


  // If the interval has no processes and nothing after, we will resize it
  if (interval.processes.empty())
  {
    // TODO dynamic_safe_cast ? for non-static-castable types, have the compiler
    // enforce dynamic_cast ?
    auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(interval.parent());
    SCORE_ASSERT(scenar);

    auto resizer = doc.app.interfaces<Scenario::IntervalResizerList>().make(interval, drop.dropMaxDuration());
    m.submit(resizer);
  }

  // Add the processes
  for (auto&& file : drop.files)
  {
    auto& proc = m.createProcess<Media::Sound::ProcessModel>(interval, {});
    m.submit(new Media::ChangeAudioFile{proc, std::move(file.first)});
  }
  // Show the rack
  m.showRack(interval);

  m.commit();

  return false;
}
}
}
