#include <Media/MediaFileHandle.hpp>
#include <Media/RMSData.hpp>

#include <QFileInfo>
#include <QDebug>
#include <QStorageInfo>

#define DR_WAV_NO_STDIO
#include <dr_wav.h>

namespace Media
{
void AudioFile::load_drwav()
{
  qDebug() << "AudioFileHandle::load_drwav(): " << m_file;

  // Loading with drwav is done when the file can be
  // mmapped directly in to memory.

  MmapReader r;
  r.file = std::make_shared<QFile>();
  r.file->setFileName(m_file);

  bool ok = r.file->open(QIODevice::ReadOnly);
  if(!ok)
  {
    qDebug() << "Cannot open file" << m_file;
    m_impl = Handle{};
    on_mediaChanged();
  }

  r.data = r.file->map(0, r.file->size());
  if(!r.data)
  {
    qDebug() << "Cannot open file" << m_file;
    m_impl = Handle{};
    on_mediaChanged();
  }
  r.wav.open_memory(r.data, r.file->size());
  if(!r.wav || r.wav.channels() == 0 || r.wav.sampleRate() == 0)
  {
    qDebug() << "Cannot open file" << m_file;
    m_impl = Handle{};
    on_mediaChanged();
  }

  m_rms->load(
      m_file, r.wav.channels(), r.wav.sampleRate(),
      TimeVal::fromMsecs(1000. * r.wav.totalPCMFrameCount() / r.wav.sampleRate()));

  if(!m_rms->exists())
  {
    m_rms->decode(r.wav);
  }

  QFileInfo fi{*r.file};
  m_fileName = fi.fileName();
  m_sampleRate = r.wav.sampleRate();

  m_impl = std::move(r);

  m_fullyDecoded = true;
  on_mediaChanged();
  on_finishedDecoding();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}

std::optional<AudioInfo> probe_drwav(const QFileInfo& fi)
{
  QFile f{fi.absoluteFilePath()};
  // Do a quick pass if it'as a wav file to check for ACID tags
  if(f.open(QIODevice::ReadOnly))
  {
    if(auto data = f.map(0, f.size()))
    {
      ossia::drwav_handle h;
      h.open_memory(data, f.size());

      AudioInfo info;
      info.fileRate = h.sampleRate();
      info.channels = h.channels();
      info.fileLength = h.totalPCMFrameCount();
      info.max_arr_length = h.totalPCMFrameCount();

      if(h.acid().tempo > 0.0001)
        info.tempo = h.acid().tempo;
      else
        info.tempo = estimateTempo(f.fileName());

      return info;
    }
  }

  return std::nullopt;
}

void writeAudioArrayToFile(const QString& path, const ossia::audio_array& arr, int fs)
{
  if(arr.empty())
  {
    qDebug() << "Not writing" << path << ": no data to write.";
    return;
  }

  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
  {
    qDebug() << "Not writing" << path << ": cannot open file for writing.";
    return;
  }

  const int channels = std::ssize(arr);
  const int64_t frames = arr[0].size();
  const int64_t samples = frames * channels;
  const int64_t data_bytes = samples * sizeof(float);
  const int64_t header_bytes = 44;

  // Let's try to not fill the hard drive
  const int64_t minimum_disk_space = 2 * (header_bytes + data_bytes);
  if(QStorageInfo(path).bytesAvailable() < minimum_disk_space)
  {
    qDebug() << "Not writing" << path << ": not enough disk space.";
    return;
  }

  drwav_data_format format;
  drwav wav;

  format.container = drwav_container_riff;
  format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  format.channels = arr.size();
  format.sampleRate = fs;
  format.bitsPerSample = 32;

  auto onWrite = [](void* pUserData, const void* pData, size_t bytesToWrite) -> size_t {
    auto& file = *(QFile*)pUserData;
    return file.write(reinterpret_cast<const char*>(pData), bytesToWrite);
  };

  if(!drwav_init_write_sequential(
         &wav, &format, samples, onWrite, &f, &ossia::drwav_handle::drwav_allocs))
  {
    qDebug() << "Not writing" << path << ": could not initialize writer.";
    return;
  }

  auto buffer = std::make_unique<float[]>(samples);
  for(int64_t i = 0; i < frames; i++)
  {
    for(int c = 0; c < channels; c++)
    {
      buffer[i * channels + c] = arr[c][i];
    }
  }

  drwav_write_pcm_frames(&wav, frames, buffer.get());
  drwav_uninit(&wav);
  f.flush();
}
}
