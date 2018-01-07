#pragma once
#include <QFile>
#include <array>
#include <Media/AudioDecoder.hpp>
#include <score_plugin_media_export.h>
namespace score { struct DocumentContext; }
namespace Media
{
// TODO store them in an application-wide cache to prevent loading / unloading
// TODO memmap
struct SCORE_PLUGIN_MEDIA_EXPORT MediaFileHandle : public QObject
{
  Q_OBJECT
    public:
        MediaFileHandle() = default;

        void load(const QString& filename,
                  const score::DocumentContext&);

        QString name() const
        { return m_file; }

        const AudioDecoder& decoder() const
        {
          return m_decoder;
        }
        const AudioArray& data() const
        { return m_decoder.data; }

        float** audioData() const;

        int sampleRate() const
        { return m_sampleRate; }

        static bool isAudioFile(const QFile& f);

        // Number of samples in a channel.
        int64_t samples() const;
        int64_t channels() const;

        bool empty() const
        { return channels() == 0 || samples() == 0; }

    signals:
        void mediaChanged();
    private:
        QString m_file;
        AudioDecoder m_decoder;
        std::vector<float*> m_data;
        int m_sampleRate;
};
}
