#include "SoundDrop.hpp"

#include <Media/AudioDecoder.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <Audio/Settings/Model.hpp>

namespace Media
{
namespace Sound
{
DroppedAudioFiles::DroppedAudioFiles(const score::DocumentContext& ctx, const QMimeData& mime)
{
  for (const auto& url : mime.urls())
  {
    QString filename = url.toLocalFile();
    if (!AudioFile::isSupported(QFile{filename}))
      continue;
    auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
    AudioDecoder dec(audioSettings.getRate());
    if (auto info_opt = dec.probe(filename))
    {
      auto info = *info_opt;
      if (info.channels > 0 && info.length > 0)
      {
        const auto& file = AudioFileManager::instance().get(filename, ctx);

        auto dur = info.duration();
        if (auto tempo = estimateTempo(*file))
        {
          AudioDecoder::database()[filename].tempo = *tempo;
        }
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
  return {"wav", "aif", "aiff", "flac", "ogg", "mp3", "ape", "wv", "m4a", "wma", "w64"};
}

std::vector<Process::ProcessDropHandler::ProcessDrop>
DropHandler::drop(const QMimeData& mime, const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

  DroppedAudioFiles drop{ctx, mime};
  if (!drop.valid())
    return {};

  for (auto&& file : drop.files)
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Sound::ProcessModel>::get();
    p.creation.prettyName = QFileInfo{file.first}.baseName();
    p.duration = file.second;
    p.setup = [f = std::move(file.first),
               song_t = *p.duration,
               &ctx](Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& proc = static_cast<Sound::ProcessModel&>(m);
      disp.submit(new Media::ChangeAudioFile{proc, std::move(f), ctx});
    };
    vec.push_back(std::move(p));
  }

  return vec;
}

}
}
