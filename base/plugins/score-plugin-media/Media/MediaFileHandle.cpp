#include "MediaFileHandle.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <Media/AudioDecoder.hpp>
#include <QFileInfo>
#include <score/document/DocumentContext.hpp>
#include <core/document/Document.hpp>
#include <score/tools/File.hpp>
namespace Media
{
void MediaFileHandle::load(
    const QString &filename,
    const score::DocumentContext& ctx)
{
  m_file = score::locateFilePath(filename, ctx);
  QFile f(m_file);
  if(isAudioFile(f))
  {
    m_decoder.decode(m_file);

    m_sampleRate = 44100; // for now everything is reencoded

    m_data.resize(m_decoder.data.size());
    for(std::size_t i = 0; i < m_decoder.data.size(); i++)
      m_data[i] = m_decoder.data[i].data();

    mediaChanged();
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
