#include "MediaFileHandle.hpp"

#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>

namespace Media
{
MediaFileHandle::MediaFileHandle()
{
  m_hdl = std::make_shared<ossia::audio_data>();
}

MediaFileHandle::~MediaFileHandle() {}

void MediaFileHandle::load(
    const QString& path,
    const score::DocumentContext& ctx)
{
  m_originalFile = path;
  m_file = score::locateFilePath(path, ctx);
  QFile f{m_file};
  if (isAudioFile(f))
  {
    m_hdl = std::make_shared<ossia::audio_data>();
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

audio_sample** MediaFileHandle::audioData() const
{
  return const_cast<audio_sample**>(m_data.data());
}

bool MediaFileHandle::isAudioFile(const QFile& file)
{
  return file.exists()
         && file.fileName().contains(
                QRegExp(".(wav|aif|aiff|flac|ogg|mp3)", Qt::CaseInsensitive));
}

int64_t MediaFileHandle::samples() const
{
  return channels() > 0 ? m_hdl->data[0].size() : 0;
}

int64_t MediaFileHandle::channels() const
{
  return m_hdl->data.size();
}
}
