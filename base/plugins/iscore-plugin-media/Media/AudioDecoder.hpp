#pragma once
#include <QAudioDecoder>
#include <vector>
#include <atomic>
#include <Media/AudioArray.hpp>
#include <ossia/detail/optional.hpp>

namespace Media
{
struct AudioInfo
{
    int64_t rate{};
    int64_t channels{};
    int64_t length{};
};

class AudioDecoder :
        public QObject
{
  Q_OBJECT
  std::size_t read_length(const QString& path);
public:
        AudioDecoder();
        ossia::optional<AudioInfo> probe(const QString& path);
        void decode(const QString& path);

        int64_t sampleRate{};
        AudioArray data;
        std::atomic_bool ready{false};

};

}
