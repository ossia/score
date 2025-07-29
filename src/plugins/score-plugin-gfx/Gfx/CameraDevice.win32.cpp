#include "CameraDevice.hpp"

#include <Gfx/CameraDeviceEnumerator.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}
#include <dshow.h>
#include <dvdmedia.h>
#include <wmcodecdsp.h>
namespace Gfx
{

static int guidToPixelFormat(const GUID& subtype)
{
  if(subtype == MEDIASUBTYPE_YUY2)
    return AV_PIX_FMT_YUYV422;
  else if(subtype == MEDIASUBTYPE_RGB24)
    return AV_PIX_FMT_BGR24;
  else if(subtype == MEDIASUBTYPE_RGB32)
    return AV_PIX_FMT_BGR0;
  else if(subtype == MEDIASUBTYPE_UYVY)
    return AV_PIX_FMT_UYVY422;
  else if(subtype == MEDIASUBTYPE_I420)
    return AV_PIX_FMT_YUV420P;
  else if(subtype == MEDIASUBTYPE_IYUV)
    return AV_PIX_FMT_YUV420P;
  else if(subtype == MEDIASUBTYPE_YV12)
    return AV_PIX_FMT_YUV420P;
  else if(subtype == MEDIASUBTYPE_NV12)
    return AV_PIX_FMT_NV12;
  else if(subtype == MEDIASUBTYPE_MJPG)
    return AV_PIX_FMT_YUVJ420P;
  else if(subtype == MEDIASUBTYPE_RGB555)
    return AV_PIX_FMT_BGR555LE;
  else if(subtype == MEDIASUBTYPE_RGB565)
    return AV_PIX_FMT_BGR565LE;
  else if(subtype == MEDIASUBTYPE_ARGB32)
    return AV_PIX_FMT_BGRA;
  else if(subtype == MEDIASUBTYPE_YVU9)
    return AV_PIX_FMT_YUV410P;
  else
    return AV_PIX_FMT_NONE;
}

static void enumerateCameraFormat(
    const AM_MEDIA_TYPE* pmt, const VIDEOINFOHEADER* pVih,
    const VIDEO_STREAM_CONFIG_CAPS& caps, CameraSettings& source,
    std::function<void()> f)
{
  const auto w = pVih->bmiHeader.biWidth;
  const auto h = pVih->bmiHeader.biHeight;
  source.codec = AV_CODEC_ID_RAWVIDEO;
  source.pixelformat = guidToPixelFormat(pmt->subtype);
  if(w <= 0 || h <= 0 || source.pixelformat == AV_PIX_FMT_NONE)
    return;
  if(source.pixelformat == AV_PIX_FMT_YUVJ420P)
  {
    source.codec = AV_CODEC_ID_MJPEG;
    source.pixelformat = AV_PIX_FMT_NONE;
  }

  source.size = {w, h};

  if(caps.MinFrameInterval > 0 && caps.MaxFrameInterval > 0)
  {
    const auto maxFps = 10000000.0 / caps.MinFrameInterval;
    const auto minFps = 10000000.0 / caps.MaxFrameInterval;

    static constexpr auto common_fps
        = std::array{240.0, 200.0, 120.0, 100.0, 90.0, 75.0, 60.0,
                     50.0,  30.0,  25.0,  24.0,  15.0, 10.0, 5.0};

    for(auto fps : common_fps)
    {
      if(fps >= minFps && fps <= maxFps)
      {
        source.fps = fps;
        f();
      }
    }
  }
  else if(pVih->AvgTimePerFrame > 0)
  {
    source.fps = 10000000.0 / pVih->AvgTimePerFrame;
    f();
  }
  else
  {
    if(caps.MaxFrameInterval > 0)
    {
      source.fps = 10000000.0 / caps.MaxFrameInterval;
      f();
    }
    else
    {
      source.fps = 30.0;
      f();
    }
  }
}

static void enumerateCameraFormats(
    IMoniker* moniker, CameraSettings& source, std::function<void()> f)
{
  IBaseFilter* pFilter = nullptr;
  HRESULT hr
      = moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&pFilter);
  if(FAILED(hr))
    return;

  IEnumPins* pEnumPins = nullptr;
  hr = pFilter->EnumPins(&pEnumPins);
  if(FAILED(hr))
  {
    pFilter->Release();
    return;
  }

  IPin* pPin = nullptr;
  while(pEnumPins->Next(1, &pPin, nullptr) == S_OK)
  {
    PIN_DIRECTION pinDir;
    hr = pPin->QueryDirection(&pinDir);
    if(SUCCEEDED(hr) && pinDir == PINDIR_OUTPUT)
    {
      IAMStreamConfig* pStreamConfig = nullptr;
      hr = pPin->QueryInterface(IID_IAMStreamConfig, (void**)&pStreamConfig);
      if(SUCCEEDED(hr))
      {
        int count = 0, size = 0;
        hr = pStreamConfig->GetNumberOfCapabilities(&count, &size);
        if(SUCCEEDED(hr))
        {
          for(int i = 0; i < count; i++)
          {
            AM_MEDIA_TYPE* pmt = nullptr;
            VIDEO_STREAM_CONFIG_CAPS caps;

            hr = pStreamConfig->GetStreamCaps(i, &pmt, (BYTE*)&caps);
            if(SUCCEEDED(hr) && pmt)
            {
              if(pmt->majortype == MEDIATYPE_Video
                 && pmt->formattype == FORMAT_VideoInfo)
              {
                if(VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)pmt->pbFormat)
                {
                  // Finally we reach the camera formats
                  enumerateCameraFormat(pmt, pVih, caps, source, f);
                }
              }

              if(pmt->cbFormat != 0)
                CoTaskMemFree((PVOID)pmt->pbFormat);
              if(pmt->pUnk != nullptr)
                pmt->pUnk->Release();
              CoTaskMemFree(pmt);
            }
          }
        }
        pStreamConfig->Release();
      }
    }
    pPin->Release();
  }

  pEnumPins->Release();
  pFilter->Release();
}

struct DShowCameraEnumerator : public Device::DeviceEnumerator
{
  std::shared_ptr<CameraDeviceEnumerator> parent;
  IMoniker* moniker{};
  IPropertyBag* prop_bag{};
  QString device;
  explicit DShowCameraEnumerator(
      std::shared_ptr<CameraDeviceEnumerator> parent, IMoniker* mk, IPropertyBag* dev,
      QString deviceName)
      : parent{parent}
      , moniker{mk}
      , prop_bag{dev}
      , device{deviceName}
  {
  }

  ~DShowCameraEnumerator()
  {
    prop_bag->Release();
    moniker->Release();
  }

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> func)
      const override
  {
    CameraSettings settings;
    settings.input = "dshow";
    settings.device = "video=" + device;

    Device::DeviceSettings s;
    s.protocol = CameraProtocolFactory::static_concreteKey();
    s.name = device;

    enumerateCameraFormats(moniker, settings, [&]() {
      s.deviceSpecificSettings = QVariant::fromValue(settings);

      std::string str;
      if(settings.codec == AV_CODEC_ID_MJPEG)
        str = "mjpeg";
      else
        str = av_get_pix_fmt_name((AVPixelFormat)settings.pixelformat);

      QString desc = QString("%1: %2x%3@%4")
                         .arg(str.c_str())
                         .arg(settings.size.width())
                         .arg(settings.size.height())
                         .arg(std::round(settings.fps));

      func(desc, s);
    });
  }
};

struct DShowCameraDeviceEnumerator : public CameraDeviceEnumerator
{
  ICreateDevEnum* pDevEnum{};
  IEnumMoniker* pEnum{};
  DShowCameraDeviceEnumerator()
  {
    // Create the System Device Enumerator.
    static constexpr REFGUID category = CLSID_VideoInputDeviceCategory;

    HRESULT hr = CoCreateInstance(
        CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
    if(!SUCCEEDED(hr))
      return;

    // Create an enumerator for the category.
    hr = pDevEnum->CreateClassEnumerator(category, &pEnum, 0);
    if(!SUCCEEDED(hr))
      pEnum = nullptr;

    pDevEnum->Release();
  }

  ~DShowCameraDeviceEnumerator() { }

  void registerAllEnumerators(Device::DeviceEnumerators& enums) override
  {
    if(!pEnum)
      return;

    auto self = shared_from_this();
    SCORE_ASSERT(self);

    IMoniker* pMoniker = nullptr;
    while(pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
      IPropertyBag* pPropBag{};
      HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
      if(FAILED(hr))
      {
        pMoniker->Release();
        continue;
      }

      VARIANT var;
      VariantInit(&var);

      hr = pPropBag->Read(L"Description", &var, 0);
      if(FAILED(hr))
        hr = pPropBag->Read(L"FriendlyName", &var, 0);
      if(FAILED(hr) || var.bstrVal == nullptr || wcsnlen(var.bstrVal, 1024) <= 0)
      {
        VariantClear(&var);
        continue;
      }

      auto prettyName = QString::fromWCharArray(var.bstrVal);
      if(prettyName.isEmpty())
        continue;
      enums.push_back(
          {prettyName, new DShowCameraEnumerator{self, pMoniker, pPropBag, prettyName}});

      VariantClear(&var);
    }
  }
};

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator()
{
  return std::make_shared<DShowCameraDeviceEnumerator>();
}
}
