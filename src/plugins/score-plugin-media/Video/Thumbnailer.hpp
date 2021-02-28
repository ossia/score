#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Video/VideoInterface.hpp>
#include <score_plugin_media_export.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}


#include <QObject>
#include <QImage>

#include <verdigris>
#include <cinttypes>

namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT VideoThumbnailer
    : public QObject
    , public VideoMetadata
{
  W_OBJECT(VideoThumbnailer)
public:
  VideoThumbnailer(QString filePath);
  ~VideoThumbnailer();

  void requestThumbnails(int64_t req, QVector<int64_t> flicks)
  E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, requestThumbnails, req, flicks)

  void thumbnailReady(int64_t req, int64_t flicks, QImage thumb)
  E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, thumbnailReady, req, flicks, thumb)

  QImage process(int64_t flicks);

  int smallWidth{};
  int smallHeight{};

private:
  void onRequest(int64_t req, QVector<int64_t> flicks);
  void processNext();

  QVector<int64_t> m_requests;
  int64_t m_requestIndex{};
  int m_currentIndex{};

  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  SwsContext* m_rescale{};
  AVCodec* m_codec{};
  AVFrame* m_rgb{};
  int64_t m_last_dts = 0;

  int m_stream{-1};
  double m_aspect{1.};
};
}

W_REGISTER_ARGTYPE(QVector<int64_t>)
#endif
