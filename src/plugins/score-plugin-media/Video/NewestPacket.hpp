#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/error.h>
}

#include <chrono>

namespace Video
{
/**
 * @brief Reads a live device up to the newest packet of stream @p stream it
 * has buffered, dropping the ones before it. Only for intra-only codecs.
 *
 * @p read is av_read_frame's contract on @p scratch. A device that answers
 * EAGAIN when empty ends the backlog that way; one that blocks instead ends
 * it with a read longer than @p blocked. @p wait is called on EAGAIN while
 * nothing was read yet and returns false to give up.
 */
template <typename Read, typename Wait>
bool readNewestPacket(
    Read&& read, Wait&& wait, AVPacket& newest, AVPacket& scratch, int stream,
    int maxBacklog, std::chrono::steady_clock::duration blocked)
{
  using clock = std::chrono::steady_clock;
  bool have = false;
  for(int n = 0; n < maxBacklog;)
  {
    const auto t0 = clock::now();
    const int res = read(&scratch);
    if(res >= 0)
    {
      n++;
      if(scratch.stream_index != stream)
      {
        av_packet_unref(&scratch);
        continue;
      }
      av_packet_unref(&newest);
      av_packet_move_ref(&newest, &scratch);
      have = true;
      if(clock::now() - t0 > blocked)
        break;
      continue;
    }

    av_packet_unref(&scratch);
    if(res != AVERROR(EAGAIN) || have)
      break;
    if(!wait())
      break;
  }
  return have;
}
}
#endif
