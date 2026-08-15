#include "CameraDevice.hpp"

#include <Gfx/CameraDeviceEnumerator.hpp>

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

#include <Gfx/Graph/interop/V4L2Loader.hpp>
#include <Gfx/Graph/interop/V4L2PixelFormat.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>

namespace Gfx
{
namespace
{
// V4L2 exposes two kinds of fourcc: raw buffer layouts, and compressed
// streams. The layouts are described once, in interop/V4L2PixelFormat, shared
// with the DMA capture path -- this file used to carry a second, independently
// maintained copy of that mapping. Only the codec side stays here, because a
// codec is not a pixel format.
struct compressed_fmt
{
  uint32_t v4l2_fmt;
  AVCodecID codec_id;
};

const compressed_fmt compressed_formats[] = {
    {V4L2_PIX_FMT_MJPEG, AV_CODEC_ID_MJPEG},
    {V4L2_PIX_FMT_JPEG, AV_CODEC_ID_MJPEG},
#ifdef V4L2_PIX_FMT_H264
    {V4L2_PIX_FMT_H264, AV_CODEC_ID_H264},
#endif
#ifdef V4L2_PIX_FMT_MPEG4
    {V4L2_PIX_FMT_MPEG4, AV_CODEC_ID_MPEG4},
#endif
#ifdef V4L2_PIX_FMT_CPIA1
    {V4L2_PIX_FMT_CPIA1, AV_CODEC_ID_CPIA},
#endif
};

AVCodecID ff_fmt_v4l2codec(uint32_t v4l2_fmt)
{
  for(const auto& c : compressed_formats)
    if(c.v4l2_fmt == v4l2_fmt)
      return c.codec_id;
  return score::gfx::interop::fromV4L2PixelFormat(v4l2_fmt)
                 != score::gfx::interop::VideoPixelFormat::Unknown
             ? AV_CODEC_ID_RAWVIDEO
             : AV_CODEC_ID_NONE;
}

AVPixelFormat ff_fmt_v4l2ff(uint32_t v4l2_fmt, AVCodecID codec_id)
{
  if(codec_id != AV_CODEC_ID_RAWVIDEO)
    return AV_PIX_FMT_NONE;

  using namespace score::gfx::interop;
  const auto layout = fromV4L2PixelFormat(v4l2_fmt);
  if(const auto av = toAVPixelFormat(layout); av != AV_PIX_FMT_NONE)
    return av;
  // The V-before-U layouts have no AVPixelFormat of their own. Name the twin so
  // the format stays offered, as it was before this mapping was shared; a
  // consumer wanting correct chroma must exchange the U and V planes, which
  // chromaSwappedTwin() is what records.
  return toAVPixelFormat(chromaSwappedTwin(layout));
}

using libv4l2 = score::gfx::v4l2::Libv4l2;

static QString v4l2_pretty_name(const AVDeviceInfo& dev)
{
  QString desc_string = dev.device_description;

  // Some names are ridiculous like "HD Webcam: HD Webcam"
  if(auto h = desc_string.indexOf(':'); h != -1)
  {
    QString a = desc_string.mid(0, h);
    QString b = desc_string.mid(h + 2);
    if(a.startsWith(b))
      desc_string = a;
  }

  desc_string += QString(" (%1)").arg(dev.device_name);
  return desc_string;
}

struct v4l2_format_enumeration
{
  const libv4l2& v4l2 = libv4l2::instance();
  int fd = -1;
  CameraSettings current;

  QString fourcc;
  QString desc_string;

  v4l2_format_enumeration(const AVInputFormat& fmt, const AVDeviceInfo& dev)
  {
    // qDebug() << "dev.device_name: " << dev.device_name;
    // qDebug() << "dev.device_desc: " << dev.device_description;
    // qDebug() << "fmt.name: " << fmt.name;
    // qDebug() << "fmt.long_name: " << fmt.long_name;

    desc_string = v4l2_pretty_name(dev);

    current.input = QString(fmt.name).split(",").front();
    current.device = dev.device_name;

    fd = open(dev.device_name, O_RDONLY);
  }

  ~v4l2_format_enumeration() { close(fd); }

  void list_all_formats(const std::function<void(CameraSettings, QString)>& func)
  {
    // First loop level: the image formats: MJPEG, YUYV..
    struct v4l2_fmtdesc vfd = {};
    vfd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
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
    QString desc = QString("%1: %2x%3@%4")
                       .arg(fourcc)
                       .arg(res.width())
                       .arg(res.height())
                       .arg(std::round(1. / rate));
    func(this->current, desc);
  }
};
}

// weird type needed because things became const in ffmpeg 4.4...
using av_input_video_type = decltype(av_input_video_device_next(nullptr));
void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  av_input_video_type fmt = nullptr;

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

struct V4L2CameraEnumerator : public Device::DeviceEnumerator
{
  std::shared_ptr<CameraDeviceEnumerator> parent;
  av_input_video_type fmt{};
  AVDeviceInfo* dev{};
  explicit V4L2CameraEnumerator(
      std::shared_ptr<CameraDeviceEnumerator> parent, av_input_video_type fmt,
      AVDeviceInfo* dev)
      : parent{parent}
      , fmt{fmt}
      , dev{dev}
  {
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> func)
      const override
  {
    v4l2_format_enumeration e{*fmt, *dev};
    auto pretty_name = e.desc_string;

    e.list_all_formats([&](const CameraSettings& set, QString mode) {
      Device::DeviceSettings s;
      s.name = pretty_name;
      s.protocol = CameraProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(set);
      func(mode, s);
    });
  }
};

struct V4L2CameraDeviceEnumerator : public CameraDeviceEnumerator
{
  V4L2CameraDeviceEnumerator()
  {
    // weird type needed because things became const in ffmpeg 4.4...
    decltype(av_input_video_device_next(nullptr)) fmt = nullptr;

    while((fmt = av_input_video_device_next(fmt)))
    {
      AVDeviceInfoList* device_list = nullptr;
      avdevice_list_input_sources(fmt, nullptr, nullptr, &device_list);
      if(device_list)
      {
        infos.push_back({fmt, device_list});
      }
    }
  }

  ~V4L2CameraDeviceEnumerator()
  {
    for(auto [fmt, info] : infos)
    {
      avdevice_free_list_devices(&info);
    }
  }

  void registerAllEnumerators(Device::DeviceEnumerators& enums) override
  {
    auto self = shared_from_this();
    SCORE_ASSERT(self);

    for(auto [fmt, device_list] : infos)
    {
      for(int i = 0; i < device_list->nb_devices; i++)
      {
        auto device = device_list->devices[i];

        enums.push_back(
            {v4l2_pretty_name(*device), new V4L2CameraEnumerator{self, fmt, device}});
      }
    }
  }

  std::vector<std::pair<av_input_video_type, AVDeviceInfoList*>> infos;
};

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator()
{
  return std::make_shared<V4L2CameraDeviceEnumerator>();
}
}
