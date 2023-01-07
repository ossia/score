#include <Audio/Settings/Model.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Media/RMSData.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/libav.hpp>

#include <QFileInfo>

namespace Media
{
void AudioFile::load_libav_stream()
{
  qDebug() << "AudioFileHandle::load_libav_stream(): " << m_file << this->m_track;
  LibavStreamReader r;
  r.path = this->m_file.toStdString();
  r.stream = this->m_track;
  {
    ossia::libav_handle av;
    av.open(r.path, r.stream, 0);
    if(av)
    {
      r.channels = av.channels();
      r.samples = av.totalPCMFrameCount();
      int64_t raw_megabytes = r.channels * r.samples * 4 / (1024 * 1024.);

      // Less than 100M: we load in ram instead
      if(raw_megabytes < 100)
      {
        av.cleanup();

        const auto& audioSettings
            = score::GUIAppContext().settings<Audio::Settings::Model>();
        const auto rate = audioSettings.getRate();
        load_libav(rate);
        return;
      }

      m_sampleRate = av.rate();
    }
    else
    {
      on_mediaChanged();
      on_finishedDecoding();
      return;
    }
  }

  m_impl = std::move(r);

  m_fullyDecoded = true;
  on_mediaChanged();
  on_finishedDecoding();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}
}
