#include <Media/MediaFileHandle.hpp>
#include <Media/RMSData.hpp>

#include <QFileInfo>
#include <QDebug>

namespace Media
{
void AudioFile::load_libav(int rate)
{
  qDebug() << "AudioFileHandle::load_ffmpeg(): " << m_file << rate;
  // Loading with libav is used :
  // - when resampling is required
  // - when the file is not a .wav
  auto ptr = std::make_shared<LibavReader>(rate);
  auto& r = *ptr;
  QFile f{m_file};
  if(isSupported(f) || m_track != -1)
  {
    r.handle = std::make_shared<ossia::audio_data>();

    auto info = probe(m_file);
    if(!info)
    {
      m_impl = Handle{};
      return;
    }

    m_rms->load(m_file, info->channels, rate, info->duration());

    // TODO remove comment when rms works again if(!m_rms->exists())
    {
      connect(
          &r.decoder, &AudioDecoder::newData, this,
          [=] {
        const auto& r = **m_impl.target<std::shared_ptr<LibavReader>>();
        std::vector<tcb::span<const audio_sample>> samples;
        auto& handle = r.handle->data;
        const auto decoded = r.decoder.decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(
              channel.data(), tcb::span<ossia::audio_sample>::size_type(decoded));
        }
        m_rms->decode(samples);

        on_newData();
          },
          Qt::QueuedConnection);

      connect(
          &r.decoder, &AudioDecoder::finishedDecoding, this,
          [=] {
        const auto& r = **m_impl.target<std::shared_ptr<LibavReader>>();
        std::vector<tcb::span<const audio_sample>> samples;
        auto& handle = r.handle->data;
        auto decoded = r.decoder.decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(
              channel.data(), tcb::span<ossia::audio_sample>::size_type(decoded));
        }
        m_rms->decodeLast(samples);

        m_fullyDecoded = true;
        on_finishedDecoding();
          },
          Qt::QueuedConnection);
    }

    r.decoder.decode(m_file, m_track, r.handle);

    m_sampleRate = rate;

    QFileInfo fi{f};

    // Assign pointers to the audio data
    r.data.resize(r.handle->data.size());
    for(std::size_t i = 0; i < r.handle->data.size(); i++)
      r.data[i] = r.handle->data[i].data();

    m_fileName = fi.fileName();
    m_impl = std::move(ptr);
  }
  else
  {
    m_impl = Handle{};
  }
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
  on_mediaChanged();
}
}
