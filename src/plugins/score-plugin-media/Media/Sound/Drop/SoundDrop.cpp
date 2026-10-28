#include "SoundDrop.hpp"

#include <Process/Commands/Properties.hpp>
#include <Process/Drop/TakeDrop.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Audio/Settings/Model.hpp>
#include <Media/AudioDecoder.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Tempo.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/flicks.hpp>

#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <optional>

namespace Media
{
namespace Sound
{
namespace
{
std::optional<ossia::audio_stretch_mode> stretch_mode(halp::clip_stretch s) noexcept
{
  switch(s)
  {
    case halp::clip_stretch::none:
      return ossia::audio_stretch_mode::None;
    case halp::clip_stretch::repitch:
      return ossia::audio_stretch_mode::Repitch;
    case halp::clip_stretch::repitch_fast:
      return ossia::audio_stretch_mode::RepitchFastestQ;
    case halp::clip_stretch::timestretch:
      return ossia::audio_stretch_mode::RubberBandStandard;
    case halp::clip_stretch::timestretch_percussive:
      return ossia::audio_stretch_mode::RubberBandPercussive;
    case halp::clip_stretch::automatic:
    default:
      return std::nullopt;
  }
}

//! The mode the drop asks for `file`.
std::optional<ossia::audio_stretch_mode> requested_mode(
    const std::vector<Process::clip_file_metadata>& meta, const QString& file)
{
  if(const auto* m = Process::find_clip_metadata(meta, file.toStdString()))
    return stretch_mode(m->stretch);
  return std::nullopt;
}

//! How long a file played unstretched is heard, in the time of the score
//! where `proc` is: its own length, at the tempo there.
TimeVal heard_duration(const Sound::ProcessModel& proc, double seconds)
{
  return TimeVal::fromMsecs(
      1000. * seconds * tempoAtStartDate(proc) / ossia::root_tempo);
}
}

DroppedAudioFiles::DroppedAudioFiles(
    const score::DocumentContext& ctx, const QMimeData& mime)
{
  const auto& urls = mime.urls();
  for(const auto& url : urls)
  {
    QString filename = url.toLocalFile();
    if(!(AudioFile::isSupported(QFile{filename}) 
      || AudioFile::isSupportedVideo(QFile{filename})))
      continue;

    if(auto info_opt = probe(filename))
    {
      auto info = *info_opt;
      if(info.channels > 0 && info.fileLength > 0)
      {
        auto dur = info.duration();
        files.emplace_back(std::make_pair(filename, dur));
        maxDuration = std::max(maxDuration, dur);
      }
    }
  }
}

TimeVal DroppedAudioFiles::dropMaxDuration() const
{
  return maxDuration;
}

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"wav", "mp3", "m4a", "ogg", "flac", "aif",  "aiff", "aifc", "caf",
          "w64", "ape", "wv",  "wma", "aac",  "opus", "ac3",  "dts",  "dtshd"};
}

void DropHandler::dropCustom(
    std::vector<ProcessDrop>& vec, const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  DroppedAudioFiles drop{ctx, mime};
  if(!drop.valid())
    return;

  // A sound file loops by itself, at its own length: only how it follows
  // the tempo is read here.
  const auto meta = Process::clip_metadata_of(mime);

  for(auto&& file : drop.files)
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Sound::ProcessModel>::get();
    p.creation.prettyName = QFileInfo{file.first}.baseName();
    p.duration = file.second;

    // A file asked to play unstretched lasts as long as it was recorded,
    // whatever tempo it says it is at.
    const auto mode = requested_mode(meta, file.first);
    double seconds = 0.;
    if(mode == ossia::audio_stretch_mode::None)
    {
      if(auto info = probe(file.first); info && info->fileRate > 0)
      {
        seconds = double(info->fileLength) / info->fileRate;
        p.duration = TimeVal::fromMsecs(1000. * seconds);
      }
    }

    p.setup = [f = score::relativizeFilePath(file.first, ctx), mode, seconds,
               &ctx](Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& proc = static_cast<Sound::ProcessModel&>(m);
      disp.submit(new Media::ChangeAudioFile{proc, std::move(f), ctx});
      if(!mode)
        return;
      disp.submit(new Media::ChangeStretchMode{proc, *mode});
      if(*mode != ossia::audio_stretch_mode::None || seconds <= 0.)
        return;

      // ChangeAudioFile sized the box and the loop by the tempo the file
      // says it is at; unstretched, it is heard for its own length at the
      // tempo where it now is.
      const TimeVal heard = heard_duration(proc, seconds);
      if(auto itv = qobject_cast<Scenario::IntervalModel*>(proc.parent());
         itv && itv->processes.size() == 1 && itv->parent()
         && !qobject_cast<Scenario::ScenarioDocumentModel*>(itv->parent()->parent()))
      {
        if(auto cmd = ctx.app.interfaces<Scenario::IntervalResizerList>().make(*itv, heard))
          disp.submit(cmd);
      }
      if(proc.loops())
        disp.submit(new Process::SetLoopDuration{proc, heard});
    };
    vec.push_back(std::move(p));
  }

  return;
}

}
}
