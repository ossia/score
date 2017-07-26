#pragma once
#include <QAudioDecoder>
#include <vector>
#include <atomic>
#include <Media/AudioArray.hpp>

namespace Media
{
struct AudioInfo
{
    bool ok = false;
    int64_t rate{};
    int64_t channels{};
    int64_t length{};
};

class AudioDecoder :
        public QObject
{
  Q_OBJECT
    public:
        AudioDecoder();
        AudioInfo probe(const QString& path);
        void decode(const QString& path);

        int64_t sampleRate{};
        AudioArray data;
        std::atomic_bool ready{false};

};

}
