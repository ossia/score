#include <Gfx/Graph/DirectVideoNodeRenderer.hpp>
#include <Gfx/Graph/HWAccelSetup.hpp>
#include <Gfx/Graph/VulkanVideoDevice.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoderFactory.hpp>
#include <Gfx/Graph/decoders/HWTransfer.hpp>
#if defined(__linux__)
#include <Gfx/Graph/decoders/HWVAAPI.hpp>
#endif
#include <Gfx/Graph/decoders/HWCUDA.hpp>
#include <Gfx/Graph/decoders/HWD3D11.hpp>
#include <Gfx/Graph/decoders/HWD3D12.hpp>
#include <Gfx/Graph/decoders/HWVideoToolbox.hpp>
#include <Gfx/Graph/decoders/HWVulkanShared.hpp>

#include <Video/GpuFormats.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/libav.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#if __has_include(<libavutil/hwcontext.h>)
#include <libavutil/hwcontext.h>
#endif
#if QT_HAS_VULKAN && __has_include(<libavutil/hwcontext_vulkan.h>)
#include <libavutil/hwcontext_vulkan.h>
#define SCORE_HAS_VULKAN_HWCONTEXT 1
#endif
#if defined(_WIN32) && __has_include(<libavutil/hwcontext_d3d11va.h>)
#include <libavutil/hwcontext_d3d11va.h>
#define SCORE_HAS_D3D11_HWCONTEXT 1
#endif
}

#if defined(__linux__)
#include <dlfcn.h>
#endif
#if defined(_WIN32) && defined(SCORE_HAS_D3D11_HWCONTEXT)
#include <QtGui/private/qrhid3d11_p.h>
#endif

namespace score::gfx
{

DirectVideoNodeRenderer::DirectVideoNodeRenderer(
    const VideoNodeBase& node, const Video::VideoMetadata& metadata) noexcept
    : NodeRenderer{node}
    , m_filePath{metadata.filePath}
    , m_fps{metadata.fps}
    , m_flicks_per_dts{metadata.flicks_per_dts}
    , m_dts_per_flicks{metadata.dts_per_flicks}
    , m_frameFormat{metadata}
{
  m_frameFormat.output_format = node.m_outputFormat;
  m_frameFormat.tonemap = node.m_tonemap;
  m_currentScaleMode = node.m_scaleMode;
}

DirectVideoNodeRenderer::~DirectVideoNodeRenderer()
{
  closeFile();
}

TextureRenderTarget DirectVideoNodeRenderer::renderTargetForInput(const Port& input)
{
  return {};
}

// ============================================================
//  Hardware acceleration selection
// ============================================================

#if LIBAVUTIL_VERSION_MAJOR >= 57

// get_format callback for AVCodecContext.
// When HW decoding is active, ffmpeg calls this to negotiate the output pixel format.
// We select the HW format that matches our desired acceleration.
// Static member function — compatible with C function pointer assignment in practice.
enum AVPixelFormat DirectVideoNodeRenderer::negotiateHWFormat(
    AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
  auto* self = static_cast<DirectVideoNodeRenderer*>(ctx->opaque);
  if(!self)
    return ctx->pix_fmt;

  for(const auto* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p)
  {
    if(*p == self->m_hwPixelFormat)
      return *p;
  }

  // HW format not offered — fall back to default
  qDebug() << "DirectVideoNodeRenderer: HW format not offered by decoder, falling back";
  return ctx->pix_fmt;
}

AVPixelFormat DirectVideoNodeRenderer::selectHardwareAcceleration(
    score::gfx::GraphicsApi api, int codec_id) const
{
  return score::gfx::selectHardwareAcceleration(
      api, static_cast<AVCodecID>(codec_id), m_rhi);
}

bool DirectVideoNodeRenderer::setupHardwareDecoder(
    const void* detected_codec_ptr, AVPixelFormat hwPixFmt)
{
  const auto* detected_codec = static_cast<const AVCodec*>(detected_codec_ptr);
  auto hwInfo = Video::ffmpegHardwareDecodingFormats(hwPixFmt);
  if(hwInfo.device == AV_HWDEVICE_TYPE_NONE)
    return false;

  // Find the appropriate hardware codec
  auto mapped = score::gfx::hwCodecName(detected_codec->name, hwInfo.device);
  if(mapped.empty())
    return false;

  const AVCodec* hw_codec = nullptr;
  if(mapped == detected_codec->name)
  {
    // VideoToolbox, DXVA2, D3D11VA: use generic codec with hw_device_ctx
    hw_codec = detected_codec;
  }
  else
  {
    hw_codec = avcodec_find_decoder_by_name(mapped.c_str());
    if(!hw_codec)
      return false;
  }

  // DRM_PRIME (v4l2m2m) doesn't use a hw device context
  if(hwPixFmt == AV_PIX_FMT_DRM_PRIME)
  {
    // Re-create codec context with the hw codec
    avcodec_free_context(&m_codecContext);
    m_codecContext = avcodec_alloc_context3(hw_codec);
    avcodec_parameters_to_context(m_codecContext, m_avstream->codecpar);
    m_codecContext->pkt_timebase = m_avstream->time_base;
    m_codecContext->thread_count = 1;

    int err = avcodec_open2(m_codecContext, hw_codec, nullptr);
    if(err < 0)
    {
      qDebug() << "DirectVideoNodeRenderer: v4l2m2m open failed:" << err;
      avcodec_free_context(&m_codecContext);
      return false;
    }

    m_hwPixelFormat = hwPixFmt;
    m_codec = hw_codec;
    return true;
  }

  // Ensure CUDA driver is initialized before FFmpeg tries to use it.
  // FFmpeg's hwcontext_cuda calls cuInit() internally, but in some setups
  // it needs to be called earlier in the process lifetime.
  if(hwInfo.device == AV_HWDEVICE_TYPE_CUDA)
  {
    static const bool cuInitOk = []() {
#if defined(__linux__)
      void* lib = dlopen("libcuda.so.1", RTLD_NOW);
      if(!lib)
        return false;
      using FN_cuInit = int (*)(unsigned int);
      auto fn = (FN_cuInit)dlsym(lib, "cuInit");
      if(!fn)
        qDebug("no cuInit!");
      auto res = fn(0);
      qDebug() << res;

      return fn && fn(0) == 0;
      return fn && fn(0) == 0;
      // Intentionally keep library loaded
#elif defined(_WIN32)
      HMODULE lib = LoadLibraryA("nvcuda.dll");
      if(!lib)
        return false;
      using FN_cuInit = int(__stdcall*)(unsigned int);
      auto fn = (FN_cuInit)GetProcAddress(lib, "cuInit");
      if(!fn)
        qDebug("no cuInit!");
      auto res = fn(0);
      qDebug() << res;

      return fn && fn(0) == 0;
#else
      return false;
#endif
    }();
    if(!cuInitOk)
    {
      qDebug() << "DirectVideoNodeRenderer: cuInit(0) failed";
      return false;
    }
    else
    {
      qDebug("CUINIT OK");
    }
  }

  // Create hardware device context
  AVBufferRef* hw_device_ctx = nullptr;

  // For Vulkan: try to create a shared context using QRhi's device.
  // This enables zero-copy AVVkFrame wrapping (no DMA-BUF export needed).
#if QT_HAS_VULKAN && defined(SCORE_HAS_VULKAN_HWCONTEXT) \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if(hwInfo.device == AV_HWDEVICE_TYPE_VULKAN
     && m_rhi && m_rhi->backend() == QRhi::Vulkan)
  {
    auto* nh
        = static_cast<const QRhiVulkanNativeHandles*>(m_rhi->nativeHandles());
    if(nh && nh->dev && nh->physDev && nh->inst)
    {
      hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
      if(hw_device_ctx)
      {
        auto* devCtx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
        auto* vkCtx
            = static_cast<AVVulkanDeviceContext*>(devCtx->hwctx);

        // Fill device handles from QRhi
        vkCtx->inst = nh->inst->vkInstance();
        vkCtx->phys_dev = nh->physDev;
        vkCtx->act_dev = nh->dev;

        // Use the system Vulkan loader's vkGetInstanceProcAddr.
        // Qt's getInstanceProcAddr dispatch doesn't include all extension
        // functions (push_descriptor, drm_format_modifier, etc.).
        // The system loader resolves them correctly for any VkDevice.
#if defined(__linux__)
        {
          static void* libvk = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_NOLOAD);
          if(!libvk)
            libvk = dlopen("libvulkan.so.1", RTLD_NOW);
          if(libvk)
            vkCtx->get_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                dlsym(libvk, "vkGetInstanceProcAddr"));
        }
#elif defined(_WIN32)
        {
          static HMODULE libvk = GetModuleHandleA("vulkan-1.dll");
          if(!libvk)
            libvk = LoadLibraryA("vulkan-1.dll");
          if(libvk)
            vkCtx->get_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                GetProcAddress(libvk, "vkGetInstanceProcAddr"));
        }
#endif
        if(!vkCtx->get_proc_addr)
        {
          qDebug() << "DirectVideoNodeRenderer: could not load vkGetInstanceProcAddr";
          av_buffer_unref(&hw_device_ctx);
          hw_device_ctx = nullptr;
        }

        if(hw_device_ctx)
        {
        // Report only the extensions that were actually enabled on
        // the shared device (the curated list from createSharedVulkanDevice).
        // Reporting unavailable extensions causes FFmpeg to resolve null
        // function pointers → crash.
        auto* funcs = nh->inst->functions();
        uint32_t availExtCount = 0;
        funcs->vkEnumerateDeviceExtensionProperties(
            nh->physDev, nullptr, &availExtCount, nullptr);
        std::vector<VkExtensionProperties> availExts(availExtCount);
        funcs->vkEnumerateDeviceExtensionProperties(
            nh->physDev, nullptr, &availExtCount, availExts.data());

        auto devHasExt = [&](const char* name) {
          for(auto& e : availExts)
            if(std::strcmp(e.extensionName, name) == 0)
              return true;
          return false;
        };

        // Same curated list as createSharedVulkanDevice, but exclude
        // DMA-BUF extensions — the shared device doesn't need them and
        // FFmpeg will try (and fail) DMA-BUF exports if it thinks they're enabled.
        // Store pointers to string literals directly — no std::string copy
        // needed (sharedVulkanDeviceExtensions returns compile-time constants).
        // Using std::vector<std::string> + c_str() would cause dangling
        // pointers when the vector reallocates.
        m_vkEnabledExtensions.clear();
        for(auto* ext : score::gfx::sharedVulkanDeviceExtensions())
        {
          if(devHasExt(ext))
            m_vkEnabledExtensions.push_back(ext);
        }
        vkCtx->enabled_dev_extensions = m_vkEnabledExtensions.data();
        vkCtx->nb_enabled_dev_extensions
            = static_cast<int>(m_vkEnabledExtensions.size());

        // Query queue families for FFmpeg
        uint32_t qfCount = 0;
        funcs->vkGetPhysicalDeviceQueueFamilyProperties(
            nh->physDev, &qfCount, nullptr);
        auto qfProps = std::make_unique<VkQueueFamilyProperties[]>(qfCount);
        funcs->vkGetPhysicalDeviceQueueFamilyProperties(
            nh->physDev, &qfCount, qfProps.get());

        int nb_qf = 0;
        for(uint32_t i = 0; i < qfCount && nb_qf < 64; i++)
        {
          vkCtx->qf[nb_qf].idx = static_cast<int>(i);
          vkCtx->qf[nb_qf].num = 1;
          vkCtx->qf[nb_qf].flags
              = static_cast<VkQueueFlagBits>(qfProps[i].queueFlags);
          nb_qf++;
        }
        vkCtx->nb_qf = nb_qf;

        int ret = av_hwdevice_ctx_init(hw_device_ctx);

        // DO NOT clear enabled_dev_extensions — FFmpeg's vulkan_decode.c
        // reads them directly (via ff_vk_extensions_to_mask) during the
        // first frame decode, not during av_hwdevice_ctx_init.
        // The pointers are string literals, valid for program lifetime.

        if(ret < 0)
        {
          qDebug() << "DirectVideoNodeRenderer: shared Vulkan context init failed:"
                   << ret << "- falling back to separate device";
          av_buffer_unref(&hw_device_ctx);
          hw_device_ctx = nullptr;
        }
        else
        {
          qDebug() << "DirectVideoNodeRenderer: using shared Vulkan device for HW decode";
          m_sharedVulkanDevice = true;
        }
        } // if(hw_device_ctx) after get_proc_addr check
      }
    }
  }
#endif

  // For D3D11VA: create a shared context using QRhi's device.
  // Without this, FFmpeg creates its own D3D11 device and
  // CopySubresourceRegion silently fails across devices (green screen).
#if defined(_WIN32) && defined(SCORE_HAS_D3D11_HWCONTEXT)
  if(!hw_device_ctx && hwInfo.device == AV_HWDEVICE_TYPE_D3D11VA && m_rhi
     && m_rhi->backend() == QRhi::D3D11)
  {
    auto* nh
        = static_cast<const QRhiD3D11NativeHandles*>(m_rhi->nativeHandles());
    if(nh && nh->dev)
    {
      qDebug() << "DirectVideoNodeRenderer: creating shared D3D11 context, QRhi device:" << nh->dev;
      hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
      if(hw_device_ctx)
      {
        auto* devCtx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
        auto* d3d11Ctx
            = static_cast<AVD3D11VADeviceContext*>(devCtx->hwctx);

        d3d11Ctx->device = static_cast<ID3D11Device*>(nh->dev);
        d3d11Ctx->device->AddRef();

        int ret = av_hwdevice_ctx_init(hw_device_ctx);
        if(ret < 0)
        {
          qDebug() << "DirectVideoNodeRenderer: shared D3D11 context init failed:"
                   << ret << "- falling back to separate device";
          av_buffer_unref(&hw_device_ctx);
          hw_device_ctx = nullptr;
        }
        else
        {
          // Verify FFmpeg is using the same device
          auto* finalCtx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
          auto* finalD3D11 = static_cast<AVD3D11VADeviceContext*>(finalCtx->hwctx);
          qDebug() << "DirectVideoNodeRenderer: shared D3D11 context OK, FFmpeg device:"
                   << finalD3D11->device << "QRhi device:" << nh->dev
                   << "same:" << (finalD3D11->device == static_cast<ID3D11Device*>(nh->dev));
        }
      }
    }
    else
    {
      qDebug() << "DirectVideoNodeRenderer: D3D11VA requested but QRhi has no D3D11 native handles";
    }
  }
  else if(!hw_device_ctx && hwInfo.device == AV_HWDEVICE_TYPE_D3D11VA)
  {
    qDebug() << "DirectVideoNodeRenderer: D3D11VA but no QRhi or wrong backend"
             << "m_rhi:" << m_rhi
             << "backend:" << (m_rhi ? m_rhi->backend() : -1);
  }
#endif

  // Fallback: let FFmpeg create its own device
  if(!hw_device_ctx)
  {
    int ret = av_hwdevice_ctx_create(
        &hw_device_ctx, hwInfo.device, nullptr, nullptr, 0);
    if(ret != 0)
    {
      qDebug() << "DirectVideoNodeRenderer: av_hwdevice_ctx_create failed:" << ret;
      return false;
    }
  }

  // Re-create codec context with HW support
  avcodec_free_context(&m_codecContext);

  // For CUDA/VDPAU: use the hw-specific codec (e.g., h264_cuvid)
  // For DXVA2/D3D11/VideoToolbox: use the generic codec with hw_device_ctx
  const AVCodec* codec_to_open = nullptr;
  if(hwInfo.device == AV_HWDEVICE_TYPE_CUDA
     || hwInfo.device == AV_HWDEVICE_TYPE_VDPAU
     || hwInfo.device == AV_HWDEVICE_TYPE_QSV)
  {
    codec_to_open = hw_codec;
  }
  else
  {
    codec_to_open = detected_codec;
  }

  m_codecContext = avcodec_alloc_context3(codec_to_open);
  avcodec_parameters_to_context(m_codecContext, m_avstream->codecpar);
  m_codecContext->pkt_timebase = m_avstream->time_base;
  m_codecContext->thread_count = 1;
  m_codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
  m_codecContext->opaque = this;
  m_codecContext->get_format = &DirectVideoNodeRenderer::negotiateHWFormat;

  m_hwPixelFormat = hwPixFmt;

  int err = avcodec_open2(m_codecContext, codec_to_open, nullptr);
  if(err < 0)
  {
    qDebug() << "DirectVideoNodeRenderer: HW codec open failed:" << err
             << "for" << codec_to_open->name;
    avcodec_free_context(&m_codecContext);
    av_buffer_unref(&hw_device_ctx);
    m_hwPixelFormat = AV_PIX_FMT_NONE;
    return false;
  }

  m_hwDeviceCtx = hw_device_ctx;
  m_codec = codec_to_open;
  return true;
}

#endif // LIBAVUTIL_VERSION_MAJOR >= 57

// Transfer a hardware-decoded frame to a software frame.
// Used when zero-copy is not available for the current RHI backend.
void DirectVideoNodeRenderer::transferHWFrame()
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
  if(!m_decodedFrame)
    return;

  if(!Video::formatIsHardwareDecoded(static_cast<AVPixelFormat>(m_decodedFrame->format)))
    return;

  if(!m_swTransferFrame)
    m_swTransferFrame = av_frame_alloc();

  av_frame_unref(m_swTransferFrame);
  m_swTransferFrame->format = AV_PIX_FMT_NONE;

  int ret = av_hwframe_transfer_data(m_swTransferFrame, m_decodedFrame, 0);
  if(ret < 0)
  {
    qDebug() << "DirectVideoNodeRenderer: av_hwframe_transfer_data failed:" << ret;
    return;
  }

  m_swTransferFrame->pts = m_decodedFrame->pts;
  m_swTransferFrame->pkt_dts = m_decodedFrame->pkt_dts;

  // Swap: m_decodedFrame now holds the software frame
  av_frame_unref(m_decodedFrame);
  av_frame_move_ref(m_decodedFrame, m_swTransferFrame);
#endif
}

// ============================================================
//  File open / close
// ============================================================

bool DirectVideoNodeRenderer::openFile(score::gfx::GraphicsApi api, QRhi* rhi)
{
  m_rhiApi = api;
  m_rhi = rhi;

  if(m_filePath.empty())
    return false;

  if(avformat_open_input(&m_formatContext, m_filePath.c_str(), nullptr, nullptr) != 0)
    return false;

  if(avformat_find_stream_info(m_formatContext, nullptr) < 0)
  {
    closeFile();
    return false;
  }

  // Find video stream
  int stream = -1;
  for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    if(m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(stream == -1)
      {
        stream = i;
        continue;
      }
    }
    m_formatContext->streams[i]->discard = AVDISCARD_ALL;
  }

  if(stream == -1)
  {
    closeFile();
    return false;
  }

  m_avstream = m_formatContext->streams[stream];
  auto codecPar = m_avstream->codecpar;

  // HAP: no codec needed, raw packet data goes directly to GPU
  if(codecPar->codec_id == AV_CODEC_ID_HAP)
  {
    m_useAVCodec = false;
    memcpy(&m_frameFormat.pixel_format, &codecPar->codec_tag, 4);
    m_frameFormat.width = codecPar->width;
    m_frameFormat.height = codecPar->height;
    return true;
  }

  // DXV: peek first packet to determine sub-format, then GPU-direct for DXT1/DXT5
  if(codecPar->codec_id == AV_CODEC_ID_DXV)
  {
    auto packet = av_packet_alloc();
    bool dxv_ok = false;
    if(av_read_frame(m_formatContext, packet) >= 0 && packet->size >= 4)
    {
      uint32_t tag = packet->data[0] | (packet->data[1] << 8)
                     | (packet->data[2] << 16)
                     | ((uint32_t)packet->data[3] << 24);
      switch(tag)
      {
        case 0x44585431: // MKBETAG('D','X','T','1')
          memcpy(&m_frameFormat.pixel_format, "Dxv1", 4);
          dxv_ok = true;
          break;
        case 0x44585435: // MKBETAG('D','X','T','5')
          memcpy(&m_frameFormat.pixel_format, "Dxv5", 4);
          dxv_ok = true;
          break;
        case 0x59434736: // MKBETAG('Y','C','G','6')
          memcpy(&m_frameFormat.pixel_format, "DxvY", 4);
          dxv_ok = true;
          break;
        case 0x59473130: // MKBETAG('Y','G','1','0')
          memcpy(&m_frameFormat.pixel_format, "DxvA", 4);
          dxv_ok = true;
          break;
        default: {
          // Old format: check type flags in high byte
          uint8_t old_type = tag >> 24;
          if(old_type & 0x40)
          {
            memcpy(&m_frameFormat.pixel_format, "Dxv5", 4);
            dxv_ok = true;
          }
          else if(old_type & 0x20)
          {
            memcpy(&m_frameFormat.pixel_format, "Dxv1", 4);
            dxv_ok = true;
          }
          break;
        }
      }
      av_packet_unref(packet);
    }
    av_packet_free(&packet);
    // Seek back to beginning
    av_seek_frame(m_formatContext, m_avstream->index, 0, AVSEEK_FLAG_BACKWARD);

    if(dxv_ok)
    {
      m_useAVCodec = false;
      m_frameFormat.width = codecPar->width;
      m_frameFormat.height = codecPar->height;
      return true;
    }
    // else: YCG6/YG10 or unknown — fall through to avcodec
  }

  auto codec = avcodec_find_decoder(codecPar->codec_id);
  if(!codec)
  {
    closeFile();
    return false;
  }
  m_codec = codec;

  m_codecContext = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(m_codecContext, codecPar);
  m_codecContext->pkt_timebase = m_avstream->time_base;
  m_codecContext->thread_count = 1;

  // Try hardware-accelerated decoding
  bool hw_ok = false;
#if LIBAVUTIL_VERSION_MAJOR >= 57
  {
    static constexpr const char* apiNames[]
        = {"Null", "OpenGL", "Vulkan", "D3D11", "Metal", "D3D12"};
    const char* apiName = (api >= 0 && api <= 5) ? apiNames[api] : "Unknown";
    uint32_t vendorId = rhi ? rhi->driverInfo().vendorId : 0;

    // Read the user's HW decode setting
    static const Gfx::Settings::HardwareVideoDecoder decoders;
    auto& set = score::AppContext().settings<Gfx::Settings::Model>();
    const auto hwSetting = set.getHardwareDecode();

    if(hwSetting.isEmpty() || hwSetting == decoders.None)
    {
      // Explicitly disabled
      // qDebug() << "DirectVideoNodeRenderer: RHI backend:" << apiName
      //          << "HW decode: disabled by user";
    }
    else if(hwSetting == decoders.Auto)
    {
      // Auto: try each viable HW decoder in priority order until one succeeds
      auto hwFmts = ::Video::selectHardwareAccelerations(
          api, codecPar->codec_id, vendorId);
      for(auto hwFmt : hwFmts)
      {
        hw_ok = setupHardwareDecoder(codec, hwFmt);
        if(hw_ok)
        {
          qDebug() << "DirectVideoNodeRenderer: using HW decoder"
                   << ((const AVCodec*)m_codec)->name
                   << "format:" << av_get_pix_fmt_name(m_hwPixelFormat);
          m_frameFormat.pixel_format = m_hwPixelFormat;
          break;
        }
      }
    }
    else
    {
      // User explicitly selected a decoder — map setting to pixel format
      AVPixelFormat hwFmt = AV_PIX_FMT_NONE;
      if(hwSetting == decoders.CUDA)
        hwFmt = AV_PIX_FMT_CUDA;
      else if(hwSetting == decoders.QSV)
        hwFmt = AV_PIX_FMT_QSV;
      else if(hwSetting == decoders.VDPAU)
        hwFmt = AV_PIX_FMT_VDPAU;
      else if(hwSetting == decoders.VAAPI)
        hwFmt = AV_PIX_FMT_VAAPI;
      else if(hwSetting == decoders.D3D)
        hwFmt = AV_PIX_FMT_D3D11;
#if defined(_WIN32) && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
      else if(hwSetting == decoders.D3D12)
        hwFmt = AV_PIX_FMT_D3D12;
#endif
      else if(hwSetting == decoders.DXVA)
        hwFmt = AV_PIX_FMT_DXVA2_VLD;
      else if(hwSetting == decoders.VideoToolbox)
        hwFmt = AV_PIX_FMT_VIDEOTOOLBOX;
      else if(hwSetting == decoders.V4L2)
        hwFmt = AV_PIX_FMT_DRM_PRIME;
      else if(hwSetting == decoders.VulkanVideo)
        hwFmt = AV_PIX_FMT_VULKAN;

      // Verify the chosen format is available and supports the codec
      if(hwFmt != AV_PIX_FMT_NONE
         && (!::Video::hardwareDecoderIsAvailable(hwFmt)
             || !::Video::codecSupportsHWPixelFormat(codecPar->codec_id, hwFmt, vendorId)))
      {
        hwFmt = AV_PIX_FMT_NONE;
      }

      if(hwFmt != AV_PIX_FMT_NONE)
      {
        hw_ok = setupHardwareDecoder(codec, hwFmt);
        if(hw_ok)
          m_frameFormat.pixel_format = m_hwPixelFormat;
      }
    }
  }
#endif

  // Fallback: software decode
  if(!hw_ok)
  {
    // setupHardwareDecoder may have freed m_codecContext — recreate it
    if(!m_codecContext)
    {
      m_codecContext = avcodec_alloc_context3(codec);
      avcodec_parameters_to_context(m_codecContext, codecPar);
      m_codecContext->pkt_timebase = m_avstream->time_base;
      m_codecContext->thread_count = 1;
    }

    int err = avcodec_open2(m_codecContext, codec, nullptr);
    if(err < 0)
    {
      avcodec_free_context(&m_codecContext);
      closeFile();
      return false;
    }
  }

  // Update timing from codec context
  auto tb = m_codecContext->pkt_timebase;
  m_dts_per_flicks = (tb.den / (tb.num * ossia::flicks_per_second<double>));
  m_flicks_per_dts = (tb.num * ossia::flicks_per_second<double>) / tb.den;

  // Allocate the reusable frame
  m_decodedFrame = av_frame_alloc();

  return true;
}

void DirectVideoNodeRenderer::closeFile()
{
  if(m_swTransferFrame)
  {
    av_frame_free(&m_swTransferFrame);
    m_swTransferFrame = nullptr;
  }

  if(m_decodedFrame)
  {
    av_frame_free(&m_decodedFrame);
    m_decodedFrame = nullptr;
  }

  if(m_codecContext)
  {
    avcodec_flush_buffers(m_codecContext);
    avcodec_free_context(&m_codecContext);
    m_codecContext = nullptr;
    m_codec = nullptr;
  }

  if(m_hwDeviceCtx)
  {
    av_buffer_unref(&m_hwDeviceCtx);
    m_hwDeviceCtx = nullptr;
  }

  if(m_formatContext)
  {
    avio_flush(m_formatContext->pb);
    avformat_flush(m_formatContext);
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  m_avstream = nullptr;
  m_hwPixelFormat = AV_PIX_FMT_NONE;
}

// ============================================================
//  Packet reading / decoding
// ============================================================

// Check if we can just read the next packet sequentially instead of seeking.
// For forward playback at normal speed, this avoids the expensive
// flush/seek/flush cycle on every frame.
bool DirectVideoNodeRenderer::isSequentialRead(int64_t flicks) const
{
  if(m_lastDecodedDts == INT64_MIN)
    return false; // No previous frame — must seek

  const double fps = m_fps > 0. ? m_fps : 24.;
  const int64_t frameDurationFlicks
      = static_cast<int64_t>(ossia::flicks_per_second<double> / fps);

  const int64_t lastFlicks
      = static_cast<int64_t>(m_lastDecodedDts * m_flicks_per_dts);
  const int64_t delta = flicks - lastFlicks;

  // Sequential if we're moving forward by 0–2 frames
  return delta >= 0 && delta <= frameDurationFlicks * 2;
}

bool DirectVideoNodeRenderer::readNextPacketRaw()
{
  auto packet = av_packet_alloc();
  bool found = false;
  int attempts = 0;

  while(av_read_frame(m_formatContext, packet) >= 0 && attempts < 10)
  {
    attempts++;
    if(packet->stream_index == m_avstream->index)
    {
      auto cp = m_avstream->codecpar;
      if(!m_decodedFrame)
        m_decodedFrame = av_frame_alloc();

      if(m_decodedFrame->buf[0])
        av_buffer_unref(&m_decodedFrame->buf[0]);

      m_decodedFrame->buf[0] = av_buffer_ref(packet->buf);
      m_decodedFrame->width = cp->width;
      m_decodedFrame->height = cp->height;
      m_decodedFrame->format = cp->codec_tag;
      m_decodedFrame->data[0] = packet->data;
      m_decodedFrame->linesize[0] = packet->size;
      m_decodedFrame->pts = packet->pts;
      m_decodedFrame->pkt_dts = packet->dts;

      m_lastDecodedDts = packet->dts;
      found = true;
      av_packet_unref(packet);
      break;
    }
    av_packet_unref(packet);
  }

  av_packet_free(&packet);
  return found;
}

bool DirectVideoNodeRenderer::readNextPacketAVCodec()
{
  av_frame_unref(m_decodedFrame);

  auto packet = av_packet_alloc();
  bool found = false;
  int attempts = 0;

  while(av_read_frame(m_formatContext, packet) >= 0 && attempts < 64)
  {
    if(packet->stream_index != m_avstream->index)
    {
      av_packet_unref(packet);
      continue;
    }

    attempts++;
    int ret = avcodec_send_packet(m_codecContext, packet);
    av_packet_unref(packet);
    if(ret < 0 && ret != AVERROR(EAGAIN))
      break;

    ret = avcodec_receive_frame(m_codecContext, m_decodedFrame);
    if(ret == 0)
    {
      m_lastDecodedDts = m_decodedFrame->pkt_dts;

      // Note: do NOT update m_frameFormat here. The format change detection
      // in update() compares the decoded frame format against m_frameFormat
      // to know when to rebuild the GPU decoder. Updating it here would hide
      // HW→SW fallback transitions (e.g. VideoToolbox rejecting a codec profile).

      found = true;
      break;
    }
    else if(ret != AVERROR(EAGAIN))
    {
      break;
    }
  }

  av_packet_free(&packet);
  return found;
}

bool DirectVideoNodeRenderer::seekAndDecode(int64_t flicks)
{
  if(!m_formatContext || !m_avstream)
    return false;

  if(flicks < 0)
    flicks = 0;

  // For sequential forward playback, skip the expensive seek
  const bool sequential = isSequentialRead(flicks);

  if(!m_useAVCodec)
  {
    // Raw GPU-compressed path (HAP, DXV)
    if(!sequential)
    {
      if(!ossia::seek_to_flick(m_formatContext, nullptr, m_avstream, flicks,
                               AVSEEK_FLAG_BACKWARD))
        return false;
    }
    return readNextPacketRaw();
  }

  // AVCodec path (ProRes, MJPEG, DNxHD, H.264, HEVC, etc.)
  if(!m_codecContext || !m_decodedFrame)
    return false;

  if(!sequential)
  {
    av_frame_unref(m_decodedFrame);
    if(!ossia::seek_to_flick(m_formatContext, m_codecContext, m_avstream, flicks,
                             AVSEEK_FLAG_BACKWARD))
      return false;
  }

  return readNextPacketAVCodec();
}

// ============================================================
//  GPU decoder creation
// ============================================================

PixelFormatInfo DirectVideoNodeRenderer::hwPixelFormatInfo() const
{
  auto codecparFmt = m_avstream
      ? static_cast<AVPixelFormat>(m_avstream->codecpar->format)
      : AV_PIX_FMT_NONE;
  int bitsPerRaw = m_avstream ? m_avstream->codecpar->bits_per_raw_sample : 0;
  return PixelFormatInfo::fromCodecParameters(m_hwSwFormat, codecparFmt, bitsPerRaw);
}

std::unique_ptr<GPUVideoDecoder>
DirectVideoNodeRenderer::tryCreateZeroCopyDecoder(QRhi& rhi)
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
  switch(m_hwPixelFormat)
  {
#if defined(__linux__)
    case AV_PIX_FMT_VAAPI:
    {
#if QT_HAS_VULKAN && defined(SCORE_HAS_DRM_HWCONTEXT) \
    && defined(VK_EXT_image_drm_format_modifier) && defined(VK_KHR_external_memory_fd) \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      if(m_rhiApi == GraphicsApi::Vulkan
         && HWVaapiVulkanDecoder::isAvailable(rhi))
      {
        return std::make_unique<HWVaapiVulkanDecoder>(
            m_frameFormat, rhi, hwPixelFormatInfo());
      }
#endif
      break;
    }
#endif // __linux__
    case AV_PIX_FMT_VULKAN:
    {
      // Shared device: GPU plane copy from multiplane to separate VkImages
#if defined(SCORE_HAS_VULKAN_HWCONTEXT_SHARED) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      if(m_sharedVulkanDevice
         && m_rhiApi == GraphicsApi::Vulkan
         && HWVulkanSharedDecoder::isAvailable(rhi))
      {
        // qDebug() << "DirectVideoNodeRenderer: zero-copy: HWVulkanSharedDecoder"
        //          << "sw_format:" << av_get_pix_fmt_name(m_hwSwFormat);
        return std::make_unique<HWVulkanSharedDecoder>(
            m_frameFormat, rhi, hwPixelFormatInfo());
      }
#endif

      // DMA-BUF bridge fallback disabled — it doesn't work on NVIDIA
      // (can't export Vulkan Video decoded frames as DMA-BUF) and causes
      // green screen when it fails. Fall through to HWTransferDecoder
      // which does GPU decode + CPU transfer (still faster than software).
      break;
    }
    case AV_PIX_FMT_CUDA:
    {
#if defined(SCORE_HAS_CUDA_HWCONTEXT) && QT_HAS_VULKAN \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      if(m_rhiApi == GraphicsApi::Vulkan
         && HWCudaVulkanDecoder::isAvailable(rhi, m_hwDeviceCtx))
      {
        return std::make_unique<HWCudaVulkanDecoder>(
            m_frameFormat, rhi, m_hwDeviceCtx, hwPixelFormatInfo());
      }
#endif
      break;
    }
#if defined(_WIN32)
    case AV_PIX_FMT_D3D11:
    {
#if defined(SCORE_HAS_D3D11_HWCONTEXT)
      if(m_rhiApi == GraphicsApi::D3D11
         && HWD3D11Decoder::isAvailable(rhi))
      {
        return std::make_unique<HWD3D11Decoder>(
            m_frameFormat, rhi, hwPixelFormatInfo());
      }
#endif
      break;
    }
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
    case AV_PIX_FMT_D3D12:
    {
#if defined(SCORE_HAS_D3D12_HWCONTEXT)
      if(m_rhiApi == GraphicsApi::D3D12
         && HWD3D12Decoder::isAvailable(rhi))
      {
        return std::make_unique<HWD3D12Decoder>(
            m_frameFormat, rhi, hwPixelFormatInfo());
      }
#endif
      break;
    }
#endif
#endif
#if defined(__APPLE__)
    case AV_PIX_FMT_VIDEOTOOLBOX:
    {
#if defined(SCORE_HAS_VTB_HWCONTEXT)
      if(m_rhiApi == GraphicsApi::Metal
         && HWVideoToolboxDecoder::isAvailable(rhi))
      {
        auto codecparFmt = m_avstream
            ? static_cast<AVPixelFormat>(m_avstream->codecpar->format)
            : AV_PIX_FMT_NONE;
        int bitsPerRaw = m_avstream
            ? m_avstream->codecpar->bits_per_raw_sample : 0;
        auto fmtInfo = PixelFormatInfo::fromCodecParameters(
            m_hwSwFormat, codecparFmt, bitsPerRaw);
        return std::make_unique<HWVideoToolboxDecoder>(
            m_frameFormat, rhi, fmtInfo);
      }
#endif
      break;
    }
#endif
    default:
      break;
  }
#endif // LIBAVUTIL_VERSION_MAJOR >= 57
  return nullptr;
}

void DirectVideoNodeRenderer::createGpuDecoder(QRhi& rhi)
{
#if LIBAVUTIL_VERSION_MAJOR >= 57
  // If hardware acceleration is active, try zero-copy first,
  // then fall back to HWTransferDecoder (GPU decode + DMA transfer).
  if(m_hwPixelFormat != AV_PIX_FMT_NONE && m_hwDeviceCtx)
  {
    if(!m_zeroCopyFailed)
    {
      auto zc = tryCreateZeroCopyDecoder(rhi);
      if(zc)
      {
        m_gpu = std::move(zc);
        m_recomputeScale = true;
        return;
      }
    }

    // Zero-copy not available — use transfer decoder
    // (GPU decode → DMA transfer to CPU → normal texture upload)
    AVPixelFormat swFmt = m_hwSwFormat != AV_PIX_FMT_NONE
                              ? m_hwSwFormat
                              : AV_PIX_FMT_NV12;
    m_gpu = std::make_unique<HWTransferDecoder>(m_frameFormat, swFmt);
    // qDebug() << "DirectVideoNodeRenderer: using HW transfer decoder, sw_format:"
    //          << av_get_pix_fmt_name(swFmt);
    m_recomputeScale = true;
    return;
  }
#endif

  auto& model = const_cast<VideoNodeBase&>(node());
  auto& filter = model.m_filter;

  m_gpu = createGPUVideoDecoder(m_frameFormat, filter.toStdString());
  if(m_gpu)
  {
    // qDebug() << "DirectVideoNodeRenderer: using SW decoder for"
    //          << av_get_pix_fmt_name(m_frameFormat.pixel_format);
  }
  else
  {
    qDebug() << "DirectVideoNodeRenderer: Unhandled pixel format: '"
             << av_get_pix_fmt_name(m_frameFormat.pixel_format) << "'";
    m_gpu = std::make_unique<EmptyDecoder>();
  }

  m_recomputeScale = true;
}

// ============================================================
//  Pipeline management
// ============================================================

void DirectVideoNodeRenderer::setupGpuDecoder(RenderList& r)
{
  if(m_gpu)
  {
    m_gpu->release(r);
    for(auto& p : m_p)
      p.second.release();
    m_p.clear();
  }

  createGpuDecoder(*r.state.rhi);
  createPipelines(r);
}

void DirectVideoNodeRenderer::createPipelines(RenderList& r)
{
  if(m_gpu)
  {
    auto shaders = m_gpu->init(r);
    SCORE_ASSERT(m_p.empty());
    score::gfx::defaultPassesInit(
        m_p, this->node().output[0]->edges, r, r.defaultQuad(), shaders.first,
        shaders.second, m_processUBO, m_materialUBO, m_gpu->samplers);
  }
}

void DirectVideoNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& rhi = *renderer.state.rhi;

  const auto& mesh = renderer.defaultQuad();
  if(m_meshBuffer.buffers.empty())
  {
    m_meshBuffer = renderer.initMeshBuffer(mesh, res);
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->setName("DirectVideoNodeRenderer::m_processUBO");
  m_processUBO->create();

  m_materialUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Material));
  m_materialUBO->setName("DirectVideoNodeRenderer::m_materialUBO");
  m_materialUBO->create();

  // Open our own decode context, passing the RHI API for HW accel selection
  if(!openFile(renderer.state.api, renderer.state.rhi))
  {
    qDebug() << "DirectVideoNodeRenderer: failed to open" << m_filePath.c_str();
  }

  createGpuDecoder(rhi);
  createPipelines(renderer);
  m_recomputeScale = true;
}

// ============================================================
//  Render pass
// ============================================================

void DirectVideoNodeRenderer::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  if(!m_gpu || !m_gpu->hasFrame)
    return;
  score::gfx::quadRenderPass(renderer, m_meshBuffer, cb, edge, m_p);
}

void DirectVideoNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node().standardUBO);

  // Compute desired time in flicks
  const double currentTime = this->node().standardUBO.time;
  const int64_t currentFlicks
      = std::max(int64_t{0}, static_cast<int64_t>(currentTime * ossia::flicks_per_second<double>));

  // Only re-decode if we moved to a different time
  if(currentFlicks != m_lastRequestedFlicks)
  {
    // Frame duration in flicks
    const double fps = m_fps > 0. ? m_fps : 24.;
    const int64_t frameDurationFlicks
        = static_cast<int64_t>(ossia::flicks_per_second<double> / fps);

    // Skip decode if we already have this frame (within ±1 frame of the same position)
    const int64_t lastDecodedFlicks
        = m_lastDecodedDts == INT64_MIN
            ? INT64_MIN
            : static_cast<int64_t>(m_lastDecodedDts * m_flicks_per_dts);

    m_lastRequestedFlicks = currentFlicks;

    if(m_lastDecodedDts == INT64_MIN
       || std::abs(currentFlicks - lastDecodedFlicks) >= frameDurationFlicks)
    {
      // HW frames may store surface handles in data[3] (QSV, VAAPI)
      // instead of data[0], so check format instead of data pointer.
      if(seekAndDecode(currentFlicks) && m_decodedFrame
         && (m_decodedFrame->data[0]
             || Video::formatIsHardwareDecoded(
                 static_cast<AVPixelFormat>(m_decodedFrame->format))))
      {
        // Check if format changed (e.g. resolution change)
        if(m_gpu && m_useAVCodec)
        {
          const auto& n = this->node();
          auto fmt = static_cast<AVPixelFormat>(m_decodedFrame->format);

          // For HW decode: the actual sw_format is only known after
          // the first decode when hw_frames_ctx is created. Check if it
          // differs from what we assumed at init time.
#if LIBAVUTIL_VERSION_MAJOR >= 57
          if(Video::formatIsHardwareDecoded(fmt) && m_codecContext
             && m_codecContext->hw_frames_ctx && !m_hwSwFormatChecked)
          {
            m_hwSwFormatChecked = true;
            auto* fc = reinterpret_cast<AVHWFramesContext*>(
                m_codecContext->hw_frames_ctx->data);
            auto realSwFmt = static_cast<AVPixelFormat>(fc->sw_format);
            if(realSwFmt != m_hwSwFormat)
            {
              m_hwSwFormat = realSwFmt;
              setupGpuDecoder(renderer);
            }
          }
#endif

          if(fmt != m_frameFormat.pixel_format
             || m_decodedFrame->width != m_frameFormat.width
             || m_decodedFrame->height != m_frameFormat.height
             || n.m_outputFormat != m_frameFormat.output_format
             || n.m_tonemap != m_frameFormat.tonemap)
          {
            // Detect HW→SW fallback: we expected a HW format but got a SW one.
            // This happens when the HW decoder rejects the codec profile
            // (e.g. VideoToolbox can't handle certain ProRes profiles).
            // Clear HW state so createGpuDecoder() uses the SW decoder path.
            if(m_hwPixelFormat != AV_PIX_FMT_NONE
               && !Video::formatIsHardwareDecoded(fmt))
            {
              m_hwPixelFormat = AV_PIX_FMT_NONE;
            }

            m_frameFormat.pixel_format = fmt;
            m_frameFormat.width = m_decodedFrame->width;
            m_frameFormat.height = m_decodedFrame->height;
            m_frameFormat.output_format = n.m_outputFormat;
            m_frameFormat.tonemap = n.m_tonemap;
            setupGpuDecoder(renderer);
          }
        }

        if(m_gpu)
        {
          m_gpu->exec(renderer, res, *m_decodedFrame);
          m_gpu->hasFrame = true;

          // If the GPU decoder flagged failure (e.g. incompatible VTB
          // pixel format, CVMetalTextureCache error), fall back to
          // HWTransferDecoder (GPU decode + CPU transfer + SW upload).
          if(m_gpu->failed)
          {
            // Prevent tryCreateZeroCopyDecoder from being tried again,
            // but keep m_hwDeviceCtx alive for HWTransferDecoder.
            m_zeroCopyFailed = true;
            setupGpuDecoder(renderer);
          }
        }
      }
    }
  }

  if(m_recomputeScale || m_currentScaleMode != this->node().m_scaleMode)
  {
    m_currentScaleMode = this->node().m_scaleMode;
    auto sz = computeScaleForMeshSizing(
        m_currentScaleMode, renderer.renderSize(edge),
        QSizeF(m_frameFormat.width, m_frameFormat.height));
    Material mat;
    mat.scale_w = sz.width();
    mat.scale_h = sz.height();
    mat.tex_w = m_frameFormat.width;
    mat.tex_h = m_frameFormat.height;

    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    m_recomputeScale = false;
  }
}

void DirectVideoNodeRenderer::release(RenderList& r)
{
  // Destroy GPU decoder BEFORE closeFile() frees m_hwDeviceCtx.
  // HW decoders (CUDA, Vulkan) hold references to the HW device context
  // and must be destroyed while it's still valid.
  if(m_gpu)
  {
    m_gpu->release(r);
    m_gpu.reset();
  }

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_materialUBO;
  m_materialUBO = nullptr;

  for(auto& p : m_p)
    p.second.release();
  m_p.clear();

  m_meshBuffer = {};

  closeFile();
}

}
