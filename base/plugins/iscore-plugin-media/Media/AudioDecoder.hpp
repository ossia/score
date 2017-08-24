#pragma once
#include <QAudioDecoder>
#include <vector>
#include <QThread>
#include <atomic>
#include <Media/AudioArray.hpp>
#include <ossia/detail/optional.hpp>

struct AVFrame;
struct SwrContext;

namespace Media
{
struct AudioInfo
{
    int64_t rate{};
    int64_t channels{};
    int64_t length{};
    int64_t max_arr_length{};
};

class AudioDecoder :
    public QObject
{
    Q_OBJECT

  public:
    AudioDecoder();
    ossia::optional<AudioInfo> probe(const QString& path);
    void decode(const QString& path);

    int64_t sampleRate{};
    std::size_t decoded{};
    AudioArray data;

  signals:
    void newData();
    void finishedDecoding();

    void startDecode(QString);
  public slots:
    void on_startDecode(QString);

  private:
    std::size_t read_length(const QString& path);
    static QHash<QString, AudioInfo>& database();

    QThread* m_baseThread{};
    QThread m_decodeThread;

    template<typename Decoder>
    void decodeFrame(Decoder dec, AVFrame& frame);
    void decodeRemaining();
    std::vector<SwrContext*> resampler;
    void initResample();
};

}
