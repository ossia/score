#include "CameraDevice.hpp"

#include <ossia/detail/dylib_loader.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>

// libv4l2.h is sometimes not here...
extern "C"
{
int v4l2_open(const char *file, int oflag, ...);
int v4l2_close(int fd);
int v4l2_dup(int fd);
int v4l2_ioctl(int fd, unsigned long int request, ...);
ssize_t v4l2_read(int fd, void *buffer, size_t n);
ssize_t v4l2_write(int fd, const void *buffer, size_t n);
void *v4l2_mmap(void *start, size_t length, int prot, int flags,
		int fd, int64_t offset);
int v4l2_munmap(void *_start, size_t length);
}

namespace Gfx
{
namespace
{
// Imported from ffmpeg source code
struct fmt_map
{
  enum AVPixelFormat ff_fmt;
  enum AVCodecID codec_id;
  uint32_t v4l2_fmt;
};

const struct fmt_map ff_fmt_conversion_table[] = {
    //ff_fmt              codec_id              v4l2_fmt
    {AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV420},
    {AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YVU420},
    {AV_PIX_FMT_YUV422P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV422P},
    {AV_PIX_FMT_YUYV422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUYV},
    {AV_PIX_FMT_UYVY422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_UYVY},
    {AV_PIX_FMT_YUV411P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV411P},
    {AV_PIX_FMT_YUV410P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV410},
    {AV_PIX_FMT_YUV410P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YVU410},
    {AV_PIX_FMT_RGB555LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555},
    {AV_PIX_FMT_RGB555BE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB555X},
    {AV_PIX_FMT_RGB565LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565},
    {AV_PIX_FMT_RGB565BE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB565X},
    {AV_PIX_FMT_BGR24, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR24},
    {AV_PIX_FMT_RGB24, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB24},
#ifdef V4L2_PIX_FMT_XBGR32
    {AV_PIX_FMT_BGR0, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_XBGR32},
    {AV_PIX_FMT_0RGB, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_XRGB32},
    {AV_PIX_FMT_BGRA, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_ABGR32},
    {AV_PIX_FMT_ARGB, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_ARGB32},
#endif
    {AV_PIX_FMT_BGR0, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR32},
    {AV_PIX_FMT_0RGB, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB32},
    {AV_PIX_FMT_GRAY8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_GREY},
#ifdef V4L2_PIX_FMT_Y16
    {AV_PIX_FMT_GRAY16LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_Y16},
#endif
#ifdef V4L2_PIX_FMT_Z16
    {AV_PIX_FMT_GRAY16LE, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_Z16},
#endif
    {AV_PIX_FMT_NV12, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_NV12},
    {AV_PIX_FMT_NONE, AV_CODEC_ID_MJPEG, V4L2_PIX_FMT_MJPEG},
    {AV_PIX_FMT_NONE, AV_CODEC_ID_MJPEG, V4L2_PIX_FMT_JPEG},
#ifdef V4L2_PIX_FMT_H264
    {AV_PIX_FMT_NONE, AV_CODEC_ID_H264, V4L2_PIX_FMT_H264},
#endif
#ifdef V4L2_PIX_FMT_MPEG4
    {AV_PIX_FMT_NONE, AV_CODEC_ID_MPEG4, V4L2_PIX_FMT_MPEG4},
#endif
#ifdef V4L2_PIX_FMT_CPIA1
    {AV_PIX_FMT_NONE, AV_CODEC_ID_CPIA, V4L2_PIX_FMT_CPIA1},
#endif
#ifdef V4L2_PIX_FMT_SRGGB8
    {AV_PIX_FMT_BAYER_BGGR8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SBGGR8},
    {AV_PIX_FMT_BAYER_GBRG8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SGBRG8},
    {AV_PIX_FMT_BAYER_GRBG8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SGRBG8},
    {AV_PIX_FMT_BAYER_RGGB8, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_SRGGB8},
#endif
    {AV_PIX_FMT_NONE, AV_CODEC_ID_NONE, 0},
};

enum AVPixelFormat ff_fmt_v4l2ff(uint32_t v4l2_fmt, enum AVCodecID codec_id)
{
  int i;

  for(i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++)
  {
    if(ff_fmt_conversion_table[i].v4l2_fmt == v4l2_fmt
       && ff_fmt_conversion_table[i].codec_id == codec_id)
    {
      return ff_fmt_conversion_table[i].ff_fmt;
    }
  }

  return AV_PIX_FMT_NONE;
}

enum AVCodecID ff_fmt_v4l2codec(uint32_t v4l2_fmt)
{
  int i;

  for(i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++)
  {
    if(ff_fmt_conversion_table[i].v4l2_fmt == v4l2_fmt)
    {
      return ff_fmt_conversion_table[i].codec_id;
    }
  }

  return AV_CODEC_ID_NONE;
}

class libv4l2
{
public:
  decltype(&::v4l2_ioctl) ioctl{};
  decltype(&::v4l2_open) open{};
  decltype(&::v4l2_close) close{};
  static const libv4l2& instance()
  {
    static const libv4l2 self;
    return self;
  }

private:
  libv4l2()
      : library("libv4l2.so.0")
  {
    open = library.symbol<decltype(&::v4l2_open)>("v4l2_open");
    close = library.symbol<decltype(&::v4l2_close)>("v4l2_close");
    ioctl = library.symbol<decltype(&::v4l2_ioctl)>("v4l2_ioctl");

    assert(open);
    assert(close);
    assert(ioctl);
  }

  ossia::dylib_loader library;
};

struct v4l2_format_enumeration
{
  const libv4l2& v4l2 = libv4l2::instance();
  int fd = -1;
  CameraSettings current;

  QString fourcc;
  QString desc_string;

  v4l2_format_enumeration(const AVInputFormat& fmt, const AVDeviceInfo& dev)
  {
    qDebug() << "dev.device_name: " << dev.device_name;
    qDebug() << "dev.device_desc: " << dev.device_description;
    qDebug() << "fmt.name: " << fmt.name;
    qDebug() << "fmt.long_name: " << fmt.long_name;

    desc_string = dev.device_description;

    // Some names are ridiculous like "HD Webcam: HD Webcam"
    if(auto h = desc_string.indexOf(':'); h != -1)
    {
      QString a = desc_string.mid(0, h);
      QString b = desc_string.mid(h + 2);
      if(a == b)
        desc_string = a;
    }

    desc_string += QString(" (%1)").arg(dev.device_name);

    current.input = QString(fmt.name).split(",").front();
    current.device = dev.device_name;

    fd = open(dev.device_name, O_RDONLY);
  }

  ~v4l2_format_enumeration() { close(fd); }

  void list_all_formats(const std::function<void(CameraSettings, QString)>& func)
  {
    // First loop level: the image formats: MJPEG, YUYV..
    struct v4l2_fmtdesc vfd = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    while(!v4l2.ioctl(fd, VIDIOC_ENUM_FMT, &vfd))
    {
      current.codec = ff_fmt_v4l2codec(vfd.pixelformat);
      current.pixelformat = ff_fmt_v4l2ff(vfd.pixelformat, (AVCodecID)current.codec);

      std::string str(4, '\0');
      memcpy(str.data(), &vfd.pixelformat, 4);
      fourcc = QString::fromStdString(str);

      list_resolutions(func, vfd.pixelformat);

      vfd.index++;
    }
  }

  void list_resolutions(
      const std::function<void(CameraSettings, QString)>& func, uint32_t pixelformat)
  {
    static const auto& v4l2 = libv4l2::instance();

    v4l2_frmsizeenum frame_size;
    memset(&frame_size, 0, sizeof(frame_size));
    frame_size.pixel_format = pixelformat;
    // Second loop level: the possible resolutions for a given format
    {
      for(; v4l2.ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0; ++frame_size.index)
      {
        if(frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE)
        {
          list_rates(
              func, pixelformat,
              QSize(frame_size.discrete.width, frame_size.discrete.height));
        }
        else if(
            frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE
            || frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
        {
          list_rates(
              func, pixelformat,
              QSize(frame_size.stepwise.min_width, frame_size.stepwise.min_height));
          list_rates(
              func, pixelformat,
              QSize(frame_size.stepwise.max_width, frame_size.stepwise.max_height));
          break;
        }
      }
    }
  }

  void list_rates(
      const std::function<void(CameraSettings, QString)>& func, uint32_t pixelformat,
      QSize res)
  {
    v4l2_frmivalenum frame_ival;
    memset(&frame_ival, 0, sizeof(frame_ival));
    frame_ival.pixel_format = pixelformat;
    frame_ival.width = res.width();
    frame_ival.height = res.height();

    // Third loop level: the possible refresh rates for a given {format, resolution}
    for(; v4l2.ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_ival) == 0;
        ++frame_ival.index)
    {
      if(frame_ival.type == V4L2_FRMSIZE_TYPE_DISCRETE)
      {
        double rate = frame_ival.discrete.numerator;
        rate /= frame_ival.discrete.denominator;

        add_format(func, res, rate);
      }
      else if(
          frame_ival.type == V4L2_FRMSIZE_TYPE_STEPWISE
          || frame_ival.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
      {
        double min_rate = frame_ival.stepwise.min.numerator;
        min_rate /= frame_ival.stepwise.min.numerator;
        double max_rate = frame_ival.stepwise.max.numerator;
        max_rate /= frame_ival.stepwise.max.numerator;

        add_format(func, res, min_rate);
        add_format(func, res, max_rate);
        break;
      }
    }
  }

  void
  add_format(std::function<void(CameraSettings, QString)> func, QSize res, double rate)
  {
    this->current.size = res;
    this->current.fps = 1. / rate;

    // Finally call our callback when we know everything...
    QString desc = desc_string;
    desc += QString(" %1: %2x%3@%4")
                .arg(fourcc)
                .arg(res.width())
                .arg(res.height())
                .arg(std::round(1. / rate));
    func(this->current, desc);
  }
};
}

void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  // weird type needed because things became const in ffmpeg 4.4...
  decltype(av_input_video_device_next(nullptr)) fmt = nullptr;

  while((fmt = av_input_video_device_next(fmt)))
  {
    AVDeviceInfoList* device_list = nullptr;
    avdevice_list_input_sources(fmt, nullptr, nullptr, &device_list);

    if(device_list)
    {
      for(int i = 0; i < device_list->nb_devices; i++)
      {
        v4l2_format_enumeration e{*fmt, *device_list->devices[i]};
        e.list_all_formats(func);
      }
      avdevice_free_list_devices(&device_list);
      device_list = nullptr;
    }
  }
}

}
