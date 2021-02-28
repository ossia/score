#pragma once
#if !defined(__EMSCRIPTEN__)

#if __has_include(<libavcodec/avcodec.h>) && \
    __has_include(<libavformat/avformat.h>) && \
    __has_include(<libavdevice/avdevice.h>) && \
    __has_include(<libavutil/frame.h>) && \
    __has_include(<libswresample/swresample.h>) && \
    __has_include(<libswscale/swscale.h>)

#define SCORE_HAS_LIBAV 1

#endif

#endif
