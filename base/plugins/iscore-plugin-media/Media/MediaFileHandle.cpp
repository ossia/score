#include "MediaFileHandle.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <Media/AudioDecoder.hpp>


template <>
void DataStreamReader::read(
        const Media::MediaFileHandle& lm)
{
    m_stream << lm.name();
}

template <>
void DataStreamWriter::write(
        Media::MediaFileHandle& lm)
{
    QString name;
    m_stream >> name;
    lm.load(name);
}



template <>
void JSONObjectReader::read(
        const Media::MediaFileHandle& lm)
{
    obj["File"] = lm.name();
}

template <>
void JSONObjectWriter::write(
        Media::MediaFileHandle& lm)
{
    lm.load(obj["File"].toString());
}


namespace Media
{
void MediaFileHandle::load(const QString &filename)
{
  m_file = filename;
  if(isAudioFile(QFile(m_file)))
  {
    auto decoder = new AudioDecoder;

    connect(decoder, &AudioDecoder::finished,
            this, [=] {
      m_array = std::move(decoder->data);
      m_sampleRate = decoder->decoder.audioFormat().sampleRate();

      if(m_array.size() == 2)
        if(m_array[1].empty())
          m_array.resize(1);

      m_data[0] = m_array[0].data();
      if(m_array.size() == 2)
        m_data[1] = m_array[1].data();
      decoder->deleteLater();
      emit mediaChanged();
    } );
    connect(decoder, &AudioDecoder::failed,
            this, [=] {
      m_array.clear();
      decoder->deleteLater();
      emit mediaChanged();
    });

    decoder->decode(m_file);
  }
}

float**MediaFileHandle::audioData() const
{
    return const_cast<float**>(m_data.data());
}

bool MediaFileHandle::isAudioFile(const QFile& file)
{
    return file.exists() && file.fileName().contains(QRegExp(".(wav|aif|aiff|flac|ogg|mp3)"));
}

int64_t MediaFileHandle::samples() const
{
    return channels() > 0 ? m_array[0].size() : 0;
}

int64_t MediaFileHandle::channels() const
{
    return m_array.size();
}

}
