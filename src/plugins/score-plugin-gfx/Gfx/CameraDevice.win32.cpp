#include "CameraDevice.hpp"

#include <Gfx/CameraControls.hpp>
#include <Gfx/CameraDeviceEnumerator.hpp>
#include <Gfx/CameraFormatName.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

// clang-format off
// Order-sensitive — do NOT let clang-format sort these:
//  - <initguid.h> must precede <windows.h>/<dshow.h> so the DirectShow GUIDs get
//    a real definition (not just an extern declaration);
//  - the Windows system headers must come before <dshow.h>/<dvdmedia.h>.
#include <initguid.h>
#include <windows.h>

#include <dshow.h>
#include <dvdmedia.h>
#include <oleauto.h>
#include <wmcodecdsp.h>
// clang-format on

#include <Gfx/Graph/interop/DirectShowPixelFormat.hpp>
#include <Gfx/Graph/interop/DirectShowSubtype.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>

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

// GUID and score::gfx::interop::DirectShowGuid have the same layout; the
// decisions themselves live in DirectShowSubtype.hpp so they can be tested on
// hosts where this file cannot be compiled.
static const score::gfx::interop::DirectShowGuid& asPortableGuid(
    const GUID& subtype) noexcept
{
  static_assert(sizeof(GUID) == sizeof(score::gfx::interop::DirectShowGuid));
  return reinterpret_cast<const score::gfx::interop::DirectShowGuid&>(subtype);
}

// True when the subtype names a compressed stream. Answered from the subtype
// itself, not inferred from guidToPixelFormat returning AV_PIX_FMT_YUVJ420P,
// which a camera genuinely offering that layout also produces.
static bool isCompressedSubtype(const GUID& subtype) noexcept
{
  return score::gfx::interop::directShowSubtypeIsCompressed(
      asPortableGuid(subtype));
}

static int guidToPixelFormat(const GUID& subtype)
{
  // The YUV layouts come from the shared, host-testable fourcc table, so they
  // cannot drift from the DMA capture path's.
  if(const auto av = score::gfx::interop::directShowSubtypePixelFormat(
         asPortableGuid(subtype));
     av != AV_PIX_FMT_NONE)
    return av;

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
    // FFmpeg has no 5:5:5 format carrying alpha, so the single alpha bit is
    // dropped. The layout is otherwise identical to RGB555.
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
  else if(subtype == MEDIASUBTYPE_IF09)
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
  if(width <= 0 || height <= 0
     || (source.pixelformat == AV_PIX_FMT_NONE && !isCompressedSubtype(subtype)))
  {
    // OLECHAR* guidString;
    // StringFromCLSID(subtype, &guidString);
    // qDebug() << "Unsupported PixelFormat GUID: " << guidString;
    // CoTaskMemFree(guidString);
    return;
  }

  if(isCompressedSubtype(subtype))
  {
    source.codec
        = score::gfx::interop::directShowSubtypeCodec(asPortableGuid(subtype));
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

    bool added = false;
    for(auto fps : common_fps)
    {
      if(fps >= minFps && fps <= maxFps)
      {
        source.fps = fps;
        f();
        added = true;
      }
    }

    if(!added)
    {
      if(maxFps - minFps < 1.)
      {
        source.fps = maxFps;
        f();
      }
      else
      {
        source.fps = minFps;
        f();
        source.fps = maxFps;
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

    std::vector<std::pair<CameraSettings, QString>> modes;
    enumerateCameraFormats(moniker, settings, [&]() {
      const std::string str
          = cameraFormatName(settings.codec, settings.pixelformat);

      QString desc = QString("%1: %2x%3@%4")
                         .arg(str.c_str())
                         .arg(settings.size.width())
                         .arg(settings.size.height())
                         .arg(std::round(settings.fps));

      modes.emplace_back(settings, std::move(desc));
    });

    keepHighestFramerates(modes);

    for(auto& [mode, desc] : modes)
    {
      s.deviceSpecificSettings = QVariant::fromValue(mode);
      func(desc, s);
    }
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
  auto root = std::make_shared<DShowCameraDeviceEnumerator>();
  root->registerAllEnumerators(enums);
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

namespace
{
struct DShowControlSpec
{
  const char* name;
  const char* auto_name;
  bool procamp;
  long property;
  bool exposure;
};

static constexpr DShowControlSpec dshow_controls[] = {
    {"exposure", "auto_exposure", false, CameraControl_Exposure, true},
    {"focus", "auto_focus", false, CameraControl_Focus, false},
    {"gain", nullptr, true, VideoProcAmp_Gain, false},
    {"white_balance", "auto_white_balance", true, VideoProcAmp_WhiteBalance, false},
    {"backlight_compensation", nullptr, true, VideoProcAmp_BacklightCompensation,
     false},
};

// CameraControl_Flags_* and VideoProcAmp_Flags_* have the same values
static constexpr long dshow_flag_auto = 0x1;
static constexpr long dshow_flag_manual = 0x2;

static QString monikerName(IMoniker* moniker)
{
  IPropertyBag* bag{};
  if(FAILED(moniker->BindToStorage(0, 0, IID_PPV_ARGS(&bag))))
    return {};

  VARIANT var;
  VariantInit(&var);
  HRESULT hr = bag->Read(L"Description", &var, 0);
  if(FAILED(hr))
    hr = bag->Read(L"FriendlyName", &var, 0);

  QString name;
  if(SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal)
    name = QString::fromWCharArray(var.bstrVal);
  VariantClear(&var);
  bag->Release();
  return name;
}

class DShowCameraControls final : public CameraControls
{
public:
  explicit DShowCameraControls(QString name)
      : m_name{std::move(name)}
  {
    std::promise<void> ready;
    auto opened = ready.get_future();
    m_thread = std::thread{[this, &ready] { run(ready); }};
    opened.wait();
  }

  ~DShowCameraControls() override { stop(); }

  bool empty() const noexcept { return m_controls.empty(); }

  void setup(ossia::net::node_base& root) override
  {
    for(auto& c : m_controls)
    {
      const std::string base = "/controls/color/";
      auto& node = ossia::net::create_node(root, base + c.spec->name);
      c.param = node.create_parameter(
          c.spec->exposure ? ossia::val_type::FLOAT : ossia::val_type::INT);
      if(c.param)
      {
        c.param->set_domain(ossia::make_domain(toValue(c, c.min), toValue(c, c.max)));
        c.param->set_value(toValue(c, c.value));
      }

      if(c.has_auto)
      {
        auto& auto_node = ossia::net::create_node(root, base + c.spec->auto_name);
        c.auto_param = auto_node.create_parameter(ossia::val_type::BOOL);
        if(c.auto_param)
          c.auto_param->set_value(bool(c.flags & dshow_flag_auto));
      }
    }
    m_published = true;
  }

  bool push(const ossia::net::parameter_base& param, const ossia::value& v) override
  {
    for(auto& c : m_controls)
    {
      if(&param == c.param)
      {
        long raw{};
        if(c.spec->exposure)
        {
          const double us = ossia::convert<double>(v);
          if(!(us > 0.))
            return false;
          raw = std::lround(std::log2(us * 1e-6));
        }
        else
        {
          raw = ossia::convert<int>(v);
        }

        const long clamped = std::clamp(raw, c.min, c.max);
        if(clamped != raw)
          qDebug() << "Camera" << m_name << c.spec->name << ":"
                   << ossia::convert<double>(v) << "clamped to"
                   << ossia::convert<double>(toValue(c, clamped));
        return post([this, &c, clamped] { request(c, clamped, dshow_flag_manual); });
      }
      else if(&param == c.auto_param)
      {
        const bool automatic = ossia::convert<bool>(v);
        return post([this, &c, automatic] {
          read(c);
          request(c, c.value, automatic ? dshow_flag_auto : dshow_flag_manual);
        });
      }
    }
    return false;
  }

  void streamStarted() override
  {
    post([this] {
      m_enforce_until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    });
  }

  void stop() override
  {
    {
      std::lock_guard lock{m_mutex};
      m_running = false;
    }
    m_cv.notify_all();
    if(m_thread.joinable())
      m_thread.join();
  }

private:
  struct Control
  {
    const DShowControlSpec* spec{};
    long min{}, max{}, value{}, flags{};
    bool has_auto{};
    bool written{};
    long wanted_value{}, wanted_flags{}, applied_value{};
    ossia::net::parameter_base* param{};
    ossia::net::parameter_base* auto_param{};
  };

  static ossia::value toValue(const Control& c, long raw)
  {
    if(c.spec->exposure)
      return float(std::exp2(double(raw)) * 1e6);
    return int(raw);
  }

  bool post(std::function<void()> f)
  {
    {
      std::lock_guard lock{m_mutex};
      if(!m_running)
        return false;
      m_queue.push_back(std::move(f));
    }
    m_cv.notify_one();
    return true;
  }

  void run(std::promise<void>& ready)
  {
    const bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    open();
    ready.set_value();

    for(;;)
    {
      std::vector<std::function<void()>> ops;
      {
        std::unique_lock lock{m_mutex};
        m_cv.wait_for(lock, std::chrono::milliseconds(enforcing() ? 100 : 500), [this] {
          return !m_running || !m_queue.empty();
        });
        if(!m_running)
          break;
        ops.swap(m_queue);
      }
      for(auto& op : ops)
        op();
      if(ops.empty() && m_published)
        refresh(enforcing());
    }

    if(m_camera)
      m_camera->Release();
    if(m_procamp)
      m_procamp->Release();
    m_camera = nullptr;
    m_procamp = nullptr;
    if(com)
      CoUninitialize();
  }

  void open()
  {
    ICreateDevEnum* devices{};
    if(FAILED(CoCreateInstance(
           CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
           IID_PPV_ARGS(&devices))))
      return;

    IEnumMoniker* monikers{};
    const HRESULT hr
        = devices->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &monikers, 0);
    devices->Release();
    if(hr != S_OK || !monikers)
      return;

    IBaseFilter* filter{};
    IMoniker* moniker{};
    while(!filter && monikers->Next(1, &moniker, nullptr) == S_OK)
    {
      if(monikerName(moniker) == m_name)
        moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&filter);
      moniker->Release();
    }
    monikers->Release();
    if(!filter)
      return;

    filter->QueryInterface(IID_IAMCameraControl, (void**)&m_camera);
    filter->QueryInterface(IID_IAMVideoProcAmp, (void**)&m_procamp);
    filter->Release();

    for(auto& spec : dshow_controls)
    {
      Control c;
      c.spec = &spec;
      long step{}, def{}, caps{};
      HRESULT ok = E_FAIL;
      if(spec.procamp && m_procamp)
        ok = m_procamp->GetRange(spec.property, &c.min, &c.max, &step, &def, &caps);
      else if(!spec.procamp && m_camera)
        ok = m_camera->GetRange(spec.property, &c.min, &c.max, &step, &def, &caps);
      if(FAILED(ok) || c.min > c.max || !read(c))
        continue;

      c.has_auto = spec.auto_name && (caps & dshow_flag_auto);
      m_controls.push_back(c);
    }
  }

  bool read(Control& c)
  {
    long value{}, flags{};
    const HRESULT hr = c.spec->procamp
                           ? m_procamp->Get(c.spec->property, &value, &flags)
                           : m_camera->Get(c.spec->property, &value, &flags);
    if(FAILED(hr))
      return false;
    c.value = value;
    c.flags = flags;
    return true;
  }

  void apply(Control& c, long value, long flags)
  {
    const HRESULT hr = c.spec->procamp
                           ? m_procamp->Set(c.spec->property, value, flags)
                           : m_camera->Set(c.spec->property, value, flags);
    if(FAILED(hr))
      qDebug() << "Camera" << m_name << ": could not set" << c.spec->name
               << Qt::hex << (unsigned long)hr;

    if(read(c))
      publish(c);
  }

  void publish(Control& c)
  {
    if(c.param)
      c.param->set_value(toValue(c, c.value));
    if(c.auto_param)
      c.auto_param->set_value(bool(c.flags & dshow_flag_auto));
  }

  void request(Control& c, long value, long flags)
  {
    apply(c, value, flags);
    c.written = true;
    c.wanted_value = value;
    c.wanted_flags = flags;
    c.applied_value = c.value;
  }

  bool enforcing() const noexcept
  {
    return std::chrono::steady_clock::now() < m_enforce_until;
  }

  void refresh(bool enforce)
  {
    for(auto& c : m_controls)
    {
      const long value = c.value, flags = c.flags;
      if(!read(c))
        continue;

      const bool manual = c.wanted_flags == dshow_flag_manual;
      if(enforce && c.written
         && (bool(c.flags & dshow_flag_manual) != manual
             || (manual && c.value != c.applied_value)))
      {
        qDebug() << "Camera" << m_name << c.spec->name << ": reset to" << c.value
                 << "by the device, set again";
        apply(c, c.wanted_value, c.wanted_flags);
      }
      else if(c.value != value || c.flags != flags)
        publish(c);
    }
  }

  QString m_name;
  IAMCameraControl* m_camera{};
  IAMVideoProcAmp* m_procamp{};
  std::vector<Control> m_controls;

  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::vector<std::function<void()>> m_queue;
  bool m_running{true};
  std::atomic_bool m_published{};
  std::chrono::steady_clock::time_point m_enforce_until{};
  std::thread m_thread;
};
}

std::shared_ptr<CameraControls> make_camera_controls(const CameraSettings& set)
{
  static const QString prefix = QStringLiteral("video=");
  if(set.input != "dshow" || !set.device.startsWith(prefix))
    return {};

  auto controls = std::make_shared<DShowCameraControls>(set.device.mid(prefix.size()));
  if(controls->empty())
    return {};
  return controls;
}
}
