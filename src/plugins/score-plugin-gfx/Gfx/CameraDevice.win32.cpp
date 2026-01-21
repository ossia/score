#include "CameraDevice.hpp"

#include <Gfx/CameraDeviceEnumerator.hpp>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

// !
#include <initguid.h>
// ! Needs to be present before, to ensure uuids get enumerated

#include <dshow.h>
#include <dvdmedia.h>
#include <wmcodecdsp.h>
#include <oleauto.h>

namespace Gfx
{
static constexpr const GUID MEDIASUBTYPE_Y800             = { 0x30303859, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_MICROSOFT_IR8    = { 0x00000032, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_Y8               = { 0x20203859, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_Z16              = { 0x2036315A, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_Y160             = { 0x30363159, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_YV16             = { 0x32315659, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_Y422             = { 0x32323459, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_GREY             = { 0x59455247, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_NV24             = { 0x3432564E, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_V216             = { 0x36313256, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static constexpr const GUID MEDIASUBTYPE_P208             = { '802P', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_P210             = { '012P', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_P216             = { '612P', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_P010             = { '010P', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_P016             = { '610P', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_Y210             = { '012Y', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_Y216             = { '612Y', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static constexpr const GUID MEDIASUBTYPE_P408             = { '804P', 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};

static int guidToPixelFormat(const GUID& subtype)
{
  if(subtype == MEDIASUBTYPE_RGB24)
    return AV_PIX_FMT_BGR24;
  else if(subtype == MEDIASUBTYPE_RGB32)
    return AV_PIX_FMT_BGR0;
  else if(subtype == MEDIASUBTYPE_ARGB32)
    return AV_PIX_FMT_BGRA;
  else if(subtype == MEDIASUBTYPE_RGB565)
    return AV_PIX_FMT_BGR565LE;
  else if(subtype == MEDIASUBTYPE_RGB555)
    return AV_PIX_FMT_BGR555LE;
  else if(subtype == MEDIASUBTYPE_ARGB1555)
    return AV_PIX_FMT_BGR555LE;
  else if(subtype == MEDIASUBTYPE_RGB8)
    return AV_PIX_FMT_PAL8;
  else if(subtype == MEDIASUBTYPE_RGB4)
    return AV_PIX_FMT_RGB4_BYTE;

  else if (subtype == MEDIASUBTYPE_A2R10G10B10)
    return AV_PIX_FMT_X2RGB10LE;
  else if (subtype == MEDIASUBTYPE_A2B10G10R10)
    return AV_PIX_FMT_X2BGR10LE;

  else if(subtype == MEDIASUBTYPE_AYUV)
    return AV_PIX_FMT_AYUV;
  else if(subtype == MEDIASUBTYPE_NV24)
    return AV_PIX_FMT_NV24;
//   else if(subtype == MEDIASUBTYPE_NV42)
//     return AV_PIX_FMT_NV42;
//   else if(subtype == MEDIASUBTYPE_Y410)
//     return AV_PIX_FMT_XV30LE;
//   else if(subtype == MEDIASUBTYPE_Y416)
//     return AV_PIX_FMT_AYUV64LE;

  else if(subtype == MEDIASUBTYPE_YUY2)
    return AV_PIX_FMT_YUYV422;
  else if(subtype == MEDIASUBTYPE_YUYV)
    return AV_PIX_FMT_YUYV422;
  else if(subtype == MEDIASUBTYPE_UYVY)
    return AV_PIX_FMT_UYVY422;
  else if(subtype == MEDIASUBTYPE_YVYU)
    return AV_PIX_FMT_YVYU422;

  else if(subtype == MEDIASUBTYPE_IYUV)
    return AV_PIX_FMT_YUV420P;
  else if(subtype == MEDIASUBTYPE_I420)
    return AV_PIX_FMT_YUV420P;
  else if(subtype == MEDIASUBTYPE_YV12)
    return AV_PIX_FMT_YUV420P;
  else if(subtype == MEDIASUBTYPE_NV12)
    return AV_PIX_FMT_NV12;
//  else if(subtype == MEDIASUBTYPE_NV21)
//    return AV_PIX_FMT_NV21;
//  else if(subtype == MEDIASUBTYPE_YV12)
//    return AV_PIX_FMT_YUV420P;
  else if (subtype == MEDIASUBTYPE_IMC1 || subtype == MEDIASUBTYPE_IMC2 ||
          subtype == MEDIASUBTYPE_IMC3 || subtype == MEDIASUBTYPE_IMC4)
    return AV_PIX_FMT_YUV420P;

  else if(subtype == MEDIASUBTYPE_P010)
    return AV_PIX_FMT_P010LE;
  else if(subtype == MEDIASUBTYPE_P016)
    return AV_PIX_FMT_P016LE;

//  else if(subtype == MEDIASUBTYPE_NV16)
//    return AV_PIX_FMT_NV16;
  else if(subtype == MEDIASUBTYPE_P210)
    return AV_PIX_FMT_P210LE;
  else if(subtype == MEDIASUBTYPE_P216)
    return AV_PIX_FMT_P216LE;

  else if(subtype == MEDIASUBTYPE_Y210)
    return AV_PIX_FMT_Y210LE;
  else if(subtype == MEDIASUBTYPE_Y216)
    return AV_PIX_FMT_Y216LE;
  else if(subtype == MEDIASUBTYPE_V216) // not sure
    return AV_PIX_FMT_Y216LE;

/////   Not supported in dshow as of ffmpeg 8, causes hangs in avformat_find_stream_info
  else if(subtype == MEDIASUBTYPE_Y8) {
     return AV_PIX_FMT_GRAY8;
  }
  else if(subtype == MEDIASUBTYPE_Z16) {
     return AV_PIX_FMT_GRAY16;
  }
  else if(subtype == MEDIASUBTYPE_Y800) {
    return AV_PIX_FMT_GRAY8;
  }
  else if(subtype == MEDIASUBTYPE_MICROSOFT_IR8) {
    return AV_PIX_FMT_GRAY8;
  }
  else if (subtype == MEDIASUBTYPE_YVU9 || subtype == MEDIASUBTYPE_IF09)
    return AV_PIX_FMT_YUV410P;

  else if (subtype == MEDIASUBTYPE_Y411 || subtype == MEDIASUBTYPE_Y41P)
    return AV_PIX_FMT_UYYVYY411;

  else if(subtype == MEDIASUBTYPE_MJPG)
    return AV_PIX_FMT_YUVJ420P;
  else if(subtype == MEDIASUBTYPE_TVMJ)
    return AV_PIX_FMT_YUVJ420P;
  else if(subtype == MEDIASUBTYPE_WAKE)
    return AV_PIX_FMT_YUVJ420P;
  else if(subtype == MEDIASUBTYPE_Plum)
    return AV_PIX_FMT_YUVJ420P;
  else if(subtype == MEDIASUBTYPE_YVU9)
    return AV_PIX_FMT_YUV410P;
  else
    return AV_PIX_FMT_NONE;
}

static void enumerateCameraFormat(
    const GUID& subtype,
    int width,
    int height,
    REFERENCE_TIME avgTimePerFrame,
    const VIDEO_STREAM_CONFIG_CAPS& caps,
    CameraSettings& source,
    std::function<void()> f)
{
  source.codec = AV_CODEC_ID_RAWVIDEO;
  source.pixelformat = guidToPixelFormat(subtype);

  // Basic validation
  if(width <= 0 || height <= 0 || source.pixelformat == AV_PIX_FMT_NONE)
  {
    // OLECHAR* guidString;
    // StringFromCLSID(subtype, &guidString);
    // qDebug() << "Unsupported PixelFormat GUID: " << guidString;
    // CoTaskMemFree(guidString);
    return;
  }

  // MJPEG special case
  if(source.pixelformat == AV_PIX_FMT_YUVJ420P)
  {
    source.codec = AV_CODEC_ID_MJPEG;
    source.pixelformat = AV_PIX_FMT_NONE;
  }

  source.size = {width, height};

  // FPS Enumeration Logic
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
  else if(avgTimePerFrame > 0)
  {
    source.fps = 10000000.0 / avgTimePerFrame;
    f();
  }
  else
  {
    // Fallback if no timing info is available
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
              if(pmt->majortype == MEDIATYPE_Video)
              {
                if(pmt->formattype == FORMAT_VideoInfo)
                {
                  if(VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)pmt->pbFormat)
                  {
                    enumerateCameraFormat(
                        pmt->subtype,
                        pVih->bmiHeader.biWidth,
                        pVih->bmiHeader.biHeight,
                        pVih->AvgTimePerFrame,
                        caps, source, f);
                  }
                }
                else if(pmt->formattype == FORMAT_VideoInfo2)
                {
                  if(VIDEOINFOHEADER2* pVih = (VIDEOINFOHEADER2*)pmt->pbFormat)
                  {
                    enumerateCameraFormat(
                        pmt->subtype,
                        pVih->bmiHeader.biWidth,
                        pVih->bmiHeader.biHeight,
                        pVih->AvgTimePerFrame,
                        caps, source, f);
                  }
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

void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  Device::DeviceEnumerators enums;
  DShowCameraDeviceEnumerator root;
  root.registerAllEnumerators(enums);
  for(auto& [name, dev] : enums)
  {
    dev->enumerate([func](const QString& name, const Device::DeviceSettings& s) {
      func(s.deviceSpecificSettings.value<CameraSettings>(), name);
    });
    delete dev;
  }
}

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator()
{
  return std::make_shared<DShowCameraDeviceEnumerator>();
}
}
