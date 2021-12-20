#include <Video/Rescale.hpp>

#if SCORE_HAS_LIBAV
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
struct SwsContext;
}

namespace Video
{

void Rescale::open(const VideoMetadata& src)
{
  // Allocate a rescale context
  qDebug() << "allocating a rescaler for format"
           << av_get_pix_fmt_name(src.pixel_format);
  m_rescale = sws_getContext(
      src.width,
      src.height,
      src.pixel_format,
      src.width,
      src.height,
      AV_PIX_FMT_RGBA,
      SWS_FAST_BILINEAR,
      NULL,
      NULL,
      NULL);
}

void Rescale::close()
{
  if (m_rescale)
  {
    sws_freeContext(m_rescale);
    m_rescale = nullptr;
  }
}

void Rescale::rescale(const VideoMetadata& src, FrameQueue& m_frames, AVFramePointer& frame, ReadFrame& read)
{
  // alloc an rgb frame
  auto rgb = m_frames.newFrame().release();
  // FIXME check if there isn't already a buffer allocated
  if(rgb->data[0] != nullptr)
  {
    qDebug() << "Warning ! frame buffer already allocated";
    av_frame_free(&rgb);
    rgb = av_frame_alloc();
  }
  av_frame_copy_props(rgb, read.frame);
  rgb->width = src.width;
  rgb->height = src.height;
  rgb->format = AV_PIX_FMT_RGBA;
  rgb->linesize[0] = 4 * src.width;
  av_frame_get_buffer(rgb, 0);

  // 2. Convert
  sws_scale(
      m_rescale,
      read.frame->data,
      read.frame->linesize,
      0,
      src.height,
      rgb->data,
      rgb->linesize);

  // 3. Free the old frame data
  frame.reset();

  // 4. Return the new frame
  read.frame = rgb;
}

}
#endif
