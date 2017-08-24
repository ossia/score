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
    m_decoder.decode(m_file);

    qDebug() << "Decode started";

    m_sampleRate = 44100; // for now everything is reencoded

    m_data.resize(m_decoder.data.size());
    for(int i = 0; i < m_decoder.data.size(); i++)
      m_data[i] = m_decoder.data[i].data();

    emit mediaChanged();
  }
}

float**MediaFileHandle::audioData() const
{
    return const_cast<float**>(m_data.data());
}

bool MediaFileHandle::isAudioFile(const QFile& file)
{
    return file.exists() && file.fileName().contains(QRegExp(".(wav|aif|aiff|flac|ogg|mp3)", Qt::CaseInsensitive));
}

int64_t MediaFileHandle::samples() const
{
    return channels() > 0 ? m_decoder.data[0].size() : 0;
}

int64_t MediaFileHandle::channels() const
{
    return m_decoder.data.size();
}

}
