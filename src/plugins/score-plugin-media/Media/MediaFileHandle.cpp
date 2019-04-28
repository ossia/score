#include "MediaFileHandle.hpp"
#include <Media/RMSData.hpp>

#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>


#include <core/document/Document.hpp>

#include <QFileInfo>
namespace Media
{

FFMPEGAudioFileHandle::FFMPEGAudioFileHandle()
{
  m_hdl = std::make_shared<ossia::audio_data>();
  m_rms = new RMSData{};
}

FFMPEGAudioFileHandle::~FFMPEGAudioFileHandle() {}

void FFMPEGAudioFileHandle::load(
    const QString& path,
    const score::DocumentContext& ctx)
{
  m_originalFile = path;
  m_file = score::locateFilePath(path, ctx);
  QFile f{m_file};
  if (isSupported(f))
  {
    m_hdl = std::make_shared<ossia::audio_data>();
    m_rms->load(m_file);

    if(!m_rms->exists())
    {
      connect(&m_decoder, &AudioDecoder::newData,
              this, [=] {
        std::vector<gsl::span<const audio_sample>> samples;
        auto& handle = m_hdl->data;
        auto decoded = this->decoder().decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(channel.data(), gsl::span<ossia::audio_sample>::index_type(decoded));
        }
        m_rms->decode(samples);

        on_newData();
      }, Qt::QueuedConnection);

      connect(&m_decoder, &AudioDecoder::finishedDecoding,
              this, [=] {
        std::vector<gsl::span<const audio_sample>> samples;
        auto& handle = m_hdl->data;
        auto decoded = this->decoder().decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(channel.data(), gsl::span<ossia::audio_sample>::index_type(decoded));
        }
        m_rms->decodeLast(samples);
        m_rms->finishedDecoding(m_hdl);

        on_finishedDecoding();
      }, Qt::QueuedConnection);
    }

    m_decoder.decode(m_file, m_hdl);

    m_sampleRate = 44100; // for now everything is reencoded

    m_data.resize(m_hdl->data.size());
    for (std::size_t i = 0; i < m_hdl->data.size(); i++)
      m_data[i] = m_hdl->data[i].data();

    QFileInfo fi{f};
    m_fileName = fi.fileName();
    on_mediaChanged();
  }
}

int64_t FFMPEGAudioFileHandle::decodedSamples() const
{
  return m_decoder.decoded;
}

bool FFMPEGAudioFileHandle::isSupported(const QFile& file)
{
  return file.exists()
      && file.fileName().contains(
        QRegExp(".(wav|aif|aiff|flac|ogg|mp3|m4a)", Qt::CaseInsensitive));
}

int64_t FFMPEGAudioFileHandle::samples() const
{
  return channels() > 0 ? m_hdl->data[0].size() : 0;
}

int64_t FFMPEGAudioFileHandle::channels() const
{
  return m_hdl->data.size();
}

}
