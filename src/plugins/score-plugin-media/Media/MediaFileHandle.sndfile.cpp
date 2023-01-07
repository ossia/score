#include <Media/MediaFileHandle.hpp>
#include <Media/RMSData.hpp>

#include <QFileInfo>
#include <QDebug>

namespace Media
{
void AudioFile::load_sndfile()
{
  qDebug() << "AudioFileHandle::load_sndfile(): " << m_file;

  // Loading with drwav is done when the file can be
  // mmapped directly in to memory.

  SndfileReader r;

  r.handle = std::make_shared<ossia::audio_data>();
  r.decoder.decode(m_file, r.handle);

  m_rms->load(
      m_file, r.decoder.channels, r.decoder.fileSampleRate,
      TimeVal::fromMsecs(1000. * r.decoder.decoded / r.decoder.fileSampleRate));

  for(auto& channel : r.handle->data)
  {
    r.data.push_back(channel.data());
  }

  m_rms->newData();
  m_rms->finishedDecoding();
  /* FIXME
  if (!m_rms->exists())
  {
    m_rms->decode(r.wav);
  }
  */

  QFileInfo fi{m_file};
  m_fileName = fi.fileName();
  m_sampleRate = r.decoder.fileSampleRate;

  m_impl = std::move(r);

  m_fullyDecoded = true;
  on_mediaChanged();
  on_finishedDecoding();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}
}
