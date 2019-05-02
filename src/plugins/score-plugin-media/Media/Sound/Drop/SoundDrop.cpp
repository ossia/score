#include "SoundDrop.hpp"

#include <Audio/Settings/Model.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Media/AudioDecoder.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <QApplication>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
namespace Media
{
namespace Sound
{
DroppedAudioFiles::DroppedAudioFiles(const QMimeData& mime)
{
  for (const auto& url : mime.urls())
  {
    QString filename = url.toLocalFile();
    if (!AudioFileHandle::isSupported(QFile{filename}))
      continue;
    auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
    AudioDecoder dec(audioSettings.getRate());
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

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {
      "wav", "aif", "aiff", "flac", "ogg", "mp3", "ape", "wv", "m4a", "wma"};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::drop(
    const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

  DroppedAudioFiles drop{mime};
  if (!drop.valid())
    return {};

  for (auto&& file : drop.files)
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Sound::ProcessModel>::get();
    p.creation.prettyName = QFileInfo{file.first}.baseName();
    p.duration = TimeVal{file.second};
    p.setup = [f = std::move(file.first), song_t = *p.duration](
                  Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& proc = static_cast<Sound::ProcessModel&>(m);
      disp.submit(new Media::ChangeAudioFile{proc, std::move(f)});
    };
    vec.push_back(std::move(p));
  }

  return vec;
}

}
}
