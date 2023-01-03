#include "CameraDevice.hpp"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
}
namespace Gfx
{
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
        auto dev = device_list->devices[i];
        QString devname = QString("%1 (%2: %3)")
                              .arg(dev->device_name)
                              .arg(fmt->long_name)
                              .arg(fmt->name);
        // TODO see AVDeviceCapabilitiesQuery and try to show some stream info ?
        func({QString(fmt->name).split(",").front(), dev->device_name}, devname);
      }
      avdevice_free_list_devices(&device_list);
      device_list = nullptr;
    }
  }
}
}
