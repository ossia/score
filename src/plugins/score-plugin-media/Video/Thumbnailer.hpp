#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Video/VideoInterface.hpp>

#include <score_plugin_media_export.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <QImage>
#include <QObject>

#include <cinttypes>
#include <verdigris>

namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT VideoThumbnailer
    : public QObject
    , public VideoMetadata
{
  W_OBJECT(VideoThumbnailer)
public:
  explicit VideoThumbnailer(QString filePath);
  ~VideoThumbnailer();

  void requestThumbnails(int64_t req, QVector<int64_t> flicks)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, requestThumbnails, req, flicks)

  //! Decode the thumbnails at that height; the width follows the video's aspect.
  void requestHeight(int height)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, requestHeight, height)

  void thumbnailReady(int64_t req, int64_t flicks, QImage thumb)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, thumbnailReady, req, flicks, thumb)

  QImage process(int64_t flicks);

  double aspectRatio() const noexcept { return m_aspect; }

  //! Resolution the frames are currently decoded at, for the thumbnailer's own
  //! thread: the layer asks for a height and scales what it gets to the slot.
  QSize thumbnailSize() const noexcept { return {smallWidth, smallHeight}; }

  static constexpr int defaultThumbnailHeight = 55;

private:
  void onRequest(int64_t req, QVector<int64_t> flicks);
  void onHeightRequest(int height);
  void setupRescale(int height);
  void processNext();

  int smallWidth{};
  int smallHeight{};

  QVector<int64_t> m_requests;
  int64_t m_requestIndex{};
  int m_currentIndex{};

  AVFormatContext* m_formatContext{};
  AVCodecContext* m_codecContext{};
  SwsContext* m_rescale{};
  const AVCodec* m_codec{};
  AVFrame* m_rgb{};
  int64_t m_last_dts = 0;

  int m_stream{-1};
  double m_aspect{1.};
};
}

W_REGISTER_ARGTYPE(QVector<int64_t>)
#endif
