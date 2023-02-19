#include "SoundDrop.hpp"

#include <Process/TimeValueSerialization.hpp>

#include <Audio/Settings/Model.hpp>
#include <Media/AudioDecoder.hpp>
#include <Media/Commands/ChangeAudioFile.hpp>
#include <Media/Sound/SoundModel.hpp>

#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace Media
{
namespace Sound
{
DroppedAudioFiles::DroppedAudioFiles(
    const score::DocumentContext& ctx, const QMimeData& mime)
{
  const auto& urls = mime.urls();
  for(const auto& url : urls)
  {
    QString filename = url.toLocalFile();
    if(!AudioFile::isSupported(QFile{filename}))
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
  return {"wav", "mp3", "m4a", "ogg", "flac", "aif", "aiff", "w64", "ape", "wv", "wma"};
}

void DropHandler::dropCustom(
    std::vector<ProcessDrop>& vec, const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  DroppedAudioFiles drop{ctx, mime};
  if(!drop.valid())
    return;

  for(auto&& file : drop.files)
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Sound::ProcessModel>::get();
    p.creation.prettyName = QFileInfo{file.first}.baseName();
    p.duration = file.second;
    p.setup = [f = score::relativizeFilePath(file.first, ctx), song_t = *p.duration,
               &ctx](Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& proc = static_cast<Sound::ProcessModel&>(m);
      disp.submit(new Media::ChangeAudioFile{proc, std::move(f), ctx});
    };
    vec.push_back(std::move(p));
  }

  return;
}

}
}
