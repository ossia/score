#include <Media/AudioDecoder.hpp>
#include <Media/AudioTrimmer.hpp>
#include <Media/MediaFileHandle.hpp>

#include <score/tools/ProjectFiles.hpp>

#include <QFileInfo>

#include <algorithm>

namespace Media
{

AudioTrimmer::~AudioTrimmer() = default;

bool AudioTrimmer::supports(const QString& absolutePath) const noexcept
{
  if(score::guessFileKind(absolutePath) != score::FileKind::Audio)
    return false;
  return AudioDecoder::do_probe(absolutePath).has_value();
}

QString AudioTrimmer::outputExtension() const noexcept
{
  return QStringLiteral("wav");
}

double AudioTrimmer::duration(const QString& absolutePath) const
{
  const auto info = AudioDecoder::do_probe(absolutePath);
  if(!info || info->fileRate <= 0 || info->fileLength <= 0)
    return 0.;
  return double(info->fileLength) / double(info->fileRate);
}

qint64 AudioTrimmer::estimatedSize(
    const QString& absolutePath, Process::MediaRange kept) const
{
  const auto info = AudioDecoder::do_probe(absolutePath);
  if(!info || info->fileRate <= 0 || info->channels <= 0)
    return 0;

  // What writeAudioArrayToFile actually produces: 32-bit float, one WAV
  // header. Exact rather than proportional, which matters because the source
  // is often 16-bit and so the result can be larger than a naive estimate.
  constexpr qint64 header = 44;
  constexpr qint64 bytes_per_sample = 4;
  const qint64 frames = qint64(kept.duration * double(info->fileRate));
  return header + frames * qint64(info->channels) * bytes_per_sample;
}

QString AudioTrimmer::trim(
    const QString& source, const QString& destination, Process::MediaRange range) const
{
  const auto info = AudioDecoder::do_probe(source);
  if(!info || info->fileRate <= 0)
    return QObject::tr("%1 cannot be read").arg(source);

  // Decode at the file's own rate: the point of trimming is to keep the same
  // audio, and asking for another rate would resample it.
  const auto decoded = AudioDecoder::decode_synchronous(source, info->fileRate);
  if(!decoded)
    return QObject::tr("%1 could not be decoded").arg(source);

  const auto& in = decoded->second;
  if(in.empty() || in[0].empty())
    return QObject::tr("%1 holds no audio").arg(source);

  const int64_t total = int64_t(in[0].size());
  const int64_t rate = info->fileRate;

  int64_t first = int64_t(range.start * double(rate));
  int64_t last = int64_t(range.end() * double(rate));
  first = std::clamp<int64_t>(first, 0, total);
  last = std::clamp<int64_t>(last, first, total);

  const int64_t frames = last - first;
  if(frames <= 0)
    return QObject::tr("the region to keep is empty");

  audio_array out;
  out.resize(in.size());
  for(std::size_t c = 0; c < in.size(); c++)
  {
    const auto& src = in[c];
    // A malformed file can hand back channels of different lengths; take what
    // is there rather than reading past the end of one of them.
    const int64_t available
        = std::clamp<int64_t>(int64_t(src.size()) - first, 0, frames);
    out[c].resize(frames, 0.f);
    if(available > 0)
      std::copy_n(src.begin() + first, available, out[c].begin());
  }

  writeAudioArrayToFile(destination, out, int(rate));

  if(QFileInfo info{destination}; !info.isFile() || info.size() <= 0)
    return QObject::tr("%1 could not be written").arg(destination);

  return {};
}
}
