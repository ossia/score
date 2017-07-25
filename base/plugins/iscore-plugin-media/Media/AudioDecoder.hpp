#pragma once
#include <QAudioDecoder>
#include <vector>
#include <atomic>
#include <Media/AudioArray.hpp>

namespace Media
{
class AudioDecoder :
        public QObject
{
  Q_OBJECT
    public:
        AudioDecoder();
        void decode(const QString& path);

        QAudioDecoder decoder;
        AudioArray data;
        std::atomic_bool ready{false};

    signals:
        void finished();
        void failed();
};

}
