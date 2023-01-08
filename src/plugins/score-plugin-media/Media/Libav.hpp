#pragma once
#if __has_include(<libavcodec/avcodec.h>) && \
    __has_include(<libavformat/avformat.h>) && \
    __has_include(<libavdevice/avdevice.h>) && \
    __has_include(<libavutil/frame.h>) && \
    __has_include(<libswresample/swresample.h>) && \
    __has_include(<libswscale/swscale.h>)

#include <QString>
#define SCORE_HAS_LIBAV 1

extern "C" int av_strerror(int errnum, char* errbuf, size_t errbuf_size);

static inline QString av_to_string(int errnum)
{
  thread_local char err[512];
  av_strerror(errnum, err, 512);
  return QString::fromUtf8(err);
}

#endif
