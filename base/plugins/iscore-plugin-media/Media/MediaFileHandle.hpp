#pragma once
#include <QFile>
#include <QAudioDecoder>
#include <array>
#include <Media/AudioArray.hpp>
namespace Media
{
// TODO store them in an application-wide cache to prevent loading / unloading
// TODO memmap
struct MediaFileHandle : public QObject
{
  Q_OBJECT
    public:
        MediaFileHandle() = default;

        void load(const QString& filename);

        QString name() const
        { return m_file; }

        const AudioArray& data() const
        { return m_array; }

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
        AudioArray m_array;
        std::array<float*, 2> m_data;
        int m_sampleRate;
};
}
