#include "GStreamerDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Audio/Settings/Model.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>
#include <Gfx/Graph/encoders/I420.hpp>
#include <Gfx/Graph/encoders/NV12.hpp>
#include <Gfx/Graph/encoders/UYVY.hpp>
#include <Gfx/InvertYRenderer.hpp>

#include <score/gfx/OpenGL.hpp>

#include <score/gfx/QRhiGles2.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/name_utils.hpp>

#include <Gfx/GStreamer/GStreamerLoader.hpp>

#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <wobjectimpl.h>

namespace Gfx::GStreamer
{

// Map GStreamer colorimetry string to colorMatrixOut() shader code.
// GStreamer colorimetry encodes range + matrix + transfer + primaries.
// See gst_video_colorimetry_from_string() in GStreamer source.
static QString colorShaderFromColorimetry(
    const QString& colorimetry,
    AVColorTransferCharacteristic input_trc = score::gfx::DefaultInputTrc)
{
  if(colorimetry.isEmpty())
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709, input_trc);

  // Standard GStreamer colorimetry identifiers
  if(colorimetry == "bt709")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_BT709, input_trc);
  if(colorimetry == "sRGB")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_BT709, AVCOL_TRC_IEC61966_2_1, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709, input_trc);
  if(colorimetry == "bt601")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_SMPTE170M, AVCOL_TRC_SMPTE170M, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE170M, input_trc);
  if(colorimetry == "bt2020")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_BT2020_NCL, AVCOL_TRC_BT2020_10, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020, input_trc);
  if(colorimetry == "bt2100-pq")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_BT2020_NCL, AVCOL_TRC_SMPTE2084, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020, input_trc);
  if(colorimetry == "bt2100-hlg")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_BT2020_NCL, AVCOL_TRC_ARIB_STD_B67, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020, input_trc);
  if(colorimetry == "smpte240m")
    return score::gfx::colorMatrixOut(
        AVCOL_SPC_SMPTE240M, AVCOL_TRC_SMPTE240M, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE240M, input_trc);

  // GStreamer also supports "range/matrix/transfer/primaries" format.
  // Fall back to default for unsupported strings.
  return score::gfx::colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709, input_trc);
}

// GStreamer output node: receives video from the GFX pipeline via GPU readback,
// pushes RGBA frames into a GStreamer appsrc element.
struct GStreamerOutputNode : score::gfx::OutputNode
{
  GstElement* m_pipeline{};
  GstElement* m_video_src{};
  GstElement* m_audio_src{};
  GStreamerSettings m_settings;
  bool m_started{};
  std::unique_ptr<score::gfx::GPUVideoEncoder> m_encoder[2];
  int m_encoderIdx{}; // ping-pong index for double-buffered encoder
  QString m_detectedFormat;      // UYVY, NV12, I420, or empty for RGBA
  QString m_detectedColorimetry; // bt709, bt2100-pq, sRGB, etc. or empty

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  QRhiReadbackResult m_readback[2];
  QRhiReadbackResult* m_currentReadback{&m_readback[0]};
  Gfx::InvertYRenderer* m_inv_y_renderer{};

  explicit GStreamerOutputNode(const GStreamerSettings& set)
      : OutputNode{}
      , m_settings{set}
  {
    input.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  virtual ~GStreamerOutputNode()
  {
    cleanup_pipeline();
  }

  bool init_pipeline()
  {
    auto& gst = libgstreamer::instance();
    if(!gst.available || !gstreamer_init())
      return false;

    GError* err = nullptr;
    m_pipeline = gst.parse_launch(
        m_settings.pipeline.toStdString().c_str(), &err);
    if(err)
    {
      qDebug() << "GStreamer output parse error:" << err->message;
      if(gst.g_error_free)
        gst.g_error_free(err);
      return false;
    }
    if(!m_pipeline)
      return false;

    // Find appsrc elements by name
    m_video_src = gst.bin_get_by_name(m_pipeline, "video");
    m_audio_src = gst.bin_get_by_name(m_pipeline, "audio");

    if(!m_video_src && !m_audio_src)
    {
      qDebug() << "GStreamer output: no appsrc named 'video' or 'audio' found";
      gst.object_unref(m_pipeline);
      m_pipeline = nullptr;
      return false;
    }

    // Configure video appsrc
    if(m_video_src)
    {
      // Set caps with proper framerate fraction via caps_from_string
      if(gst.caps_from_string && gst.app_src_set_caps)
      {
        auto capsStr = QString("video/x-raw,format=RGBA,width=%1,height=%2,framerate=%3/1")
                           .arg(m_settings.width)
                           .arg(m_settings.height)
                           .arg(m_settings.rate);
        GstCaps* caps = gst.caps_from_string(capsStr.toStdString().c_str());
        if(caps)
        {
          gst.app_src_set_caps(m_video_src, caps);
          gst.caps_unref(caps);
        }
      }

      // Configure appsrc for live streaming with automatic timestamps
      if(gst.object_set_property && gst.value_init && gst.value_unset)
      {
        auto setBool = [&](GstElement* elem, const char* prop, bool val) {
          GValue gv{};
          gst.value_init(&gv, G_TYPE_BOOLEAN);
          gst.value_set_boolean(&gv, val);
          gst.object_set_property(elem, prop, &gv);
          gst.value_unset(&gv);
        };
        auto setInt = [&](GstElement* elem, const char* prop, int val) {
          GValue gv{};
          gst.value_init(&gv, G_TYPE_INT);
          gst.value_set_int(&gv, val);
          gst.object_set_property(elem, prop, &gv);
          gst.value_unset(&gv);
        };

        setBool(m_video_src, "is-live", true);
        setBool(m_video_src, "do-timestamp", true);
        setInt(m_video_src, "format", 3); // GST_FORMAT_TIME
      }
    }

    // Configure audio appsrc similarly
    if(m_audio_src && gst.object_set_property && gst.value_init && gst.value_unset)
    {
      auto setBool = [&](GstElement* elem, const char* prop, bool val) {
        GValue gv{};
        gst.value_init(&gv, G_TYPE_BOOLEAN);
        gst.value_set_boolean(&gv, val);
        gst.object_set_property(elem, prop, &gv);
        gst.value_unset(&gv);
      };
      auto setInt = [&](GstElement* elem, const char* prop, int val) {
        GValue gv{};
        gst.value_init(&gv, G_TYPE_INT);
        gst.value_set_int(&gv, val);
        gst.object_set_property(elem, prop, &gv);
        gst.value_unset(&gv);
      };

      setBool(m_audio_src, "is-live", true);
      setBool(m_audio_src, "do-timestamp", true);
      setInt(m_audio_src, "format", 3); // GST_FORMAT_TIME
    }

    // Detect target pixel format by querying pad caps downstream of appsrc.
    // Handles both:
    //   appsrc ! video/x-raw,format=UYVY ! ...  (capsfilter directly after appsrc)
    //   appsrc ! videoconvert ! video/x-raw,format=UYVY ! ...  (capsfilter after vc)
    if(m_video_src && gst.element_get_static_pad && gst.pad_get_allowed_caps
       && gst.pad_get_peer && gst.pad_get_parent_element
       && gst.caps_get_structure && gst.structure_get_string && gst.caps_unref)
    {
      // Check if the allowed caps on a pad contain format and colorimetry strings.
      auto detectCaps = [&](GstPad* pad) -> bool {
        if(GstCaps* allowed = gst.pad_get_allowed_caps(pad))
        {
          if(auto* s = gst.caps_get_structure(allowed, 0))
          {
            bool found = false;
            if(auto* fmt = gst.structure_get_string(s, "format"))
            {
              m_detectedFormat = QString::fromUtf8(fmt);
              found = true;
            }
            if(auto* col = gst.structure_get_string(s, "colorimetry"))
            {
              m_detectedColorimetry = QString::fromUtf8(col);
            }
            gst.caps_unref(allowed);
            return found;
          }
          gst.caps_unref(allowed);
        }
        return false;
      };

      if(GstPad* srcPad = gst.element_get_static_pad(m_video_src, "src"))
      {
        // Try 1: check appsrc src pad's allowed caps (capsfilter right after appsrc)
        detectCaps(srcPad);

        // Try 2: if no specific format (e.g. videoconvert accepts everything),
        // follow the chain through the peer element's src pad.
        if(m_detectedFormat.isEmpty())
        {
          if(GstPad* peerPad = gst.pad_get_peer(srcPad))
          {
            if(GstElement* peerElem = gst.pad_get_parent_element(peerPad))
            {
              if(GstPad* peerSrcPad = gst.element_get_static_pad(peerElem, "src"))
              {
                detectCaps(peerSrcPad);
                gst.object_unref(peerSrcPad);
              }
              gst.object_unref(peerElem);
            }
            gst.object_unref(peerPad);
          }
        }
        gst.object_unref(srcPad);
      }
    }

    return true;
  }

  void start_pipeline()
  {
    if(!m_pipeline || m_started)
      return;

    auto& gst = libgstreamer::instance();
    gst.element_set_state(m_pipeline, GST_STATE_PLAYING);
    m_started = true;
  }

  void stop_pipeline()
  {
    if(!m_pipeline || !m_started)
      return;

    auto& gst = libgstreamer::instance();

    // Send EOS to appsrc elements so downstream can finalize
    if(m_video_src && gst.app_src_end_of_stream)
      gst.app_src_end_of_stream(m_video_src);
    if(m_audio_src && gst.app_src_end_of_stream)
      gst.app_src_end_of_stream(m_audio_src);

    gst.element_set_state(m_pipeline, GST_STATE_NULL);
    m_started = false;
  }

  void cleanup_pipeline()
  {
    stop_pipeline();
    if(m_pipeline)
    {
      auto& gst = libgstreamer::instance();
      if(m_video_src) { gst.object_unref(m_video_src); m_video_src = nullptr; }
      if(m_audio_src) { gst.object_unref(m_audio_src); m_audio_src = nullptr; }
      gst.object_unref(m_pipeline);
      m_pipeline = nullptr;
    }
  }

  // Zero-copy push: takes a shallow copy of the QByteArray.
  // The QByteArray's refcount keeps the data alive until GStreamer is done.
  void push_video_frame_zerocopy(QByteArray data)
  {
    if(!m_video_src || !m_started)
      return;

    auto& gst = libgstreamer::instance();
    if(!gst.buffer_new_wrapped_full)
    {
      push_video_frame_copy(
          (const unsigned char*)data.constData(), data.size());
      return;
    }

    // Move the QByteArray onto the heap so it outlives this scope.
    // GStreamer will call the destroy notify when done with the buffer.
    auto* persistent = new QByteArray(std::move(data));

    GstBuffer* buffer = gst.buffer_new_wrapped_full(
        (1 << 1), // GST_MEMORY_FLAG_READONLY
        const_cast<char*>(persistent->constData()), persistent->size(), 0,
        persistent->size(), persistent,
        [](void* p) { delete static_cast<QByteArray*>(p); });

    if(buffer)
      gst.app_src_push_buffer(m_video_src, buffer);
    else
      delete persistent;
  }

  // Copy push: allocates a GstBuffer and memcpys into it.
  void push_video_frame_copy(const unsigned char* data, int size)
  {
    if(!m_video_src || !m_started)
      return;

    auto& gst = libgstreamer::instance();

    GstBuffer* buffer = gst.buffer_new_allocate(nullptr, size, nullptr);
    if(!buffer)
      return;

    GstMapInfo map{};
    if(gst.buffer_map(buffer, &map, GST_MAP_WRITE))
    {
      memcpy(map.data, data, size);
      gst.buffer_unmap(buffer, &map);
    }

    gst.app_src_push_buffer(m_video_src, buffer);
  }

  void push_audio_frame(const float* interleaved, int num_samples, int channels)
  {
    if(!m_audio_src || !m_started)
      return;

    auto& gst = libgstreamer::instance();

    // Convert float to S16LE for GStreamer
    int size = num_samples * channels * sizeof(int16_t);
    GstBuffer* buffer = gst.buffer_new_allocate(nullptr, size, nullptr);
    if(!buffer)
      return;

    GstMapInfo map{};
    if(gst.buffer_map(buffer, &map, GST_MAP_WRITE))
    {
      auto* dst = reinterpret_cast<int16_t*>(map.data);
      for(int i = 0; i < num_samples * channels; i++)
      {
        float s = interleaved[i];
        if(s > 1.f) s = 1.f;
        if(s < -1.f) s = -1.f;
        dst[i] = (int16_t)(s * 32767.f);
      }
      gst.buffer_unmap(buffer, &map);
    }

    // Timestamps set automatically by appsrc (do-timestamp=true)
    gst.app_src_push_buffer(m_audio_src, buffer);
  }

  // OutputNode interface
  bool canRender() const override { return true; }
  void startRendering() override { start_pipeline(); }
  void onRendererChange() override { }
  void stopRendering() override { stop_pipeline(); }

  void render() override
  {
    auto renderer = m_renderer.lock();
    if(!renderer || !m_renderState)
      return;

    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb);

    if(m_encoder[0])
    {
      // Double-buffered encoder: render current frame into encoder[idx],
      // push PREVIOUS frame from encoder[idx^1] to GStreamer.
      // One frame of latency, zero tearing (same pattern as NDI output).
      auto& currentEnc = *m_encoder[m_encoderIdx];
      auto& prevEnc = *m_encoder[m_encoderIdx ^ 1];

      currentEnc.exec(*rhi, *cb);
      rhi->endOffscreenFrame();

      // Push the PREVIOUS frame's readback (stable — not being written to)
      if(prevEnc.readback(0).data.size() > 0)
      {
        if(prevEnc.planeCount() == 1)
        {
          push_video_frame_zerocopy(prevEnc.readback(0).data);
        }
        else
        {
          int totalSize = 0;
          for(int i = 0; i < prevEnc.planeCount(); i++)
            totalSize += prevEnc.readback(i).data.size();

          if(totalSize > 0)
          {
            QByteArray concat;
            concat.resize(totalSize);
            int offset = 0;
            for(int i = 0; i < prevEnc.planeCount(); i++)
            {
              auto& rb = prevEnc.readback(i);
              memcpy(concat.data() + offset, rb.data.constData(), rb.data.size());
              offset += rb.data.size();
            }
            push_video_frame_zerocopy(std::move(concat));
          }
        }
      }

      // Flip for next frame
      m_encoderIdx ^= 1;
    }
    else
    {
      // Standard RGBA path with double-buffered readback (NDI pattern).
      // Push PREVIOUS frame's readback, then swap buffers.
      rhi->endOffscreenFrame();

      auto& readback = *m_currentReadback;
      int sz = readback.pixelSize.width() * readback.pixelSize.height() * 4;
      int bytes = readback.data.size();
      if(bytes > 0 && bytes >= sz)
        push_video_frame_zerocopy(readback.data);

      // Swap readback buffer for next frame
      m_currentReadback = (m_currentReadback == &m_readback[0])
                              ? &m_readback[1]
                              : &m_readback[0];
      if(m_inv_y_renderer)
        m_inv_y_renderer->updateReadback(*m_currentReadback);
    }
  }

  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / m_settings.rate};
  }

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }
  score::gfx::RenderList* renderer() const override
  {
    return m_renderer.lock().get();
  }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState = std::make_shared<score::gfx::RenderState>();

    m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
    QRhiGles2InitParams params;
    params.fallbackSurface = m_renderState->surface;
    score::GLCapabilities caps;
    caps.setupFormat(params.format);
    m_renderState->rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
    m_renderState->renderSize = QSize(m_settings.width, m_settings.height);
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->api = score::gfx::GraphicsApi::OpenGL;
    m_renderState->version = caps.qShaderVersion;

    auto rhi = m_renderState->rhi;
    m_texture = rhi->newTexture(
        QRhiTexture::RGBA8, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(
        m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    init_pipeline();

    // Create GPU encoder if a YUV target format was detected
    if(!m_detectedFormat.isEmpty() && rhi)
    {
      auto makeEncoder = [&]() -> std::unique_ptr<score::gfx::GPUVideoEncoder> {
        if(m_detectedFormat == "UYVY" || m_detectedFormat == "YUY2")
          return std::make_unique<score::gfx::UYVYEncoder>();
        else if(m_detectedFormat == "NV12")
          return std::make_unique<score::gfx::NV12Encoder>();
        else if(m_detectedFormat == "I420" || m_detectedFormat == "YV12")
          return std::make_unique<score::gfx::I420Encoder>();
        return nullptr;
      };

      // Create two encoder instances for double-buffered readback
      m_encoder[0] = makeEncoder();
      m_encoder[1] = makeEncoder();

      if(m_encoder[0] && m_encoder[1])
      {
        auto input_trc = static_cast<AVColorTransferCharacteristic>(m_settings.input_transfer);
        auto colorShader = colorShaderFromColorimetry(m_detectedColorimetry, input_trc);
        qDebug() << "GStreamer output: GPU encoder"
                 << "format=" << m_detectedFormat
                 << "colorimetry=" << m_detectedColorimetry
                 << "inputTrc=" << m_settings.input_transfer
                 << "shaderLen=" << colorShader.size();
        m_encoder[0]->init(*rhi, *m_renderState, m_texture,
                           m_settings.width, m_settings.height, colorShader);
        m_encoder[1]->init(*rhi, *m_renderState, m_texture,
                           m_settings.width, m_settings.height, colorShader);

        // Update appsrc caps to match the encoder's output format
        if(auto& gst = libgstreamer::instance();
           gst.caps_from_string && gst.app_src_set_caps && m_video_src)
        {
          auto capsStr = QString("video/x-raw,format=%1,width=%2,height=%3,framerate=%4/1")
                             .arg(m_detectedFormat)
                             .arg(m_settings.width)
                             .arg(m_settings.height)
                             .arg(m_settings.rate);
          if(auto* caps = gst.caps_from_string(capsStr.toStdString().c_str()))
          {
            gst.app_src_set_caps(m_video_src, caps);
            gst.caps_unref(caps);
          }
        }
      }
    }

    conf.onReady();
  }

  void destroyOutput() override
  {
    for(auto& enc : m_encoder)
    {
      if(enc)
      {
        enc->release();
        enc.reset();
      }
    }
    cleanup_pipeline();
  }

  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    score::gfx::TextureRenderTarget rt{
        m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor,
        m_renderTarget};

    if(m_encoder[0])
    {
      // When the GPU encoder is active, use a basic renderer that just
      // provides the render target. No Y-flip (the encoder shader does it),
      // no readback (the encoder does its own).
      return new Gfx::BasicRenderer{rt, *m_renderState, *this};
    }

    // Standard RGBA path: InvertYRenderer does Y-flip + double-buffered readback
    // (same pattern as NDI output)
    return const_cast<Gfx::InvertYRenderer*&>(m_inv_y_renderer)
        = new Gfx::InvertYRenderer{
            *this, rt, const_cast<QRhiReadbackResult&>(m_readback[0])};
  }
};

// Audio parameter that receives audio from the engine and pushes to GStreamer
class gstreamer_output_audio_parameter final : public ossia::audio_parameter
{
public:
  gstreamer_output_audio_parameter(
      GStreamerOutputNode& node, int num_channels, int bs,
      ossia::net::node_base& n)
      : audio_parameter{n}
      , m_node{node}
      , m_audio_data(num_channels)
      , m_interleaved(bs * num_channels)
  {
    audio.resize(num_channels);
    for(int i = 0; i < num_channels; i++)
    {
      m_audio_data[i].resize(bs, 0.f);
      audio[i] = m_audio_data[i];
    }
  }

  virtual ~gstreamer_output_audio_parameter() { }

  void push_value(const ossia::audio_port& mixed) noexcept override
  {
    auto min_chan = std::min(mixed.channels(), (std::size_t)audio.size());
    if(min_chan == 0)
      return;

    int num_samples = mixed.channel(0).size();
    if(num_samples == 0)
      return;

    // Interleave channels for GStreamer
    m_interleaved.resize(num_samples * min_chan);
    for(int s = 0; s < num_samples; s++)
    {
      for(std::size_t ch = 0; ch < min_chan; ch++)
      {
        m_interleaved[s * min_chan + ch]
            = float(mixed.channel(ch)[s] * m_gain);
      }
    }

    m_node.push_audio_frame(m_interleaved.data(), num_samples, min_chan);
  }

private:
  GStreamerOutputNode& m_node;
  std::vector<ossia::float_vector> m_audio_data;
  std::vector<float> m_interleaved;
};

// Protocol for GStreamer output
class gstreamer_output_protocol : public Gfx::gfx_protocol_base
{
public:
  GStreamerOutputNode* output_node{};

  explicit gstreamer_output_protocol(
      const GStreamerSettings& set, GfxExecutionAction& ctx)
      : gfx_protocol_base{ctx}
  {
    output_node = new GStreamerOutputNode{set};
  }

  ~gstreamer_output_protocol() { }

  void start_execution() override { }
  void stop_execution() override { }
};

// Device tree with video output node and audio parameter
class gstreamer_output_device : public ossia::net::device_base
{
  ossia::net::generic_node root;

public:
  gstreamer_output_device(
      const GStreamerSettings& set, GStreamerOutputNode& node,
      std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{name, *this}
  {
    auto& p = *static_cast<gfx_protocol_base*>(m_protocol.get());

    // Video output node
    root.add_child(
        std::make_unique<Gfx::gfx_node_base>(*this, p, &node, "Video"));

    // Audio input node
    if(set.audio_channels > 0)
    {
      auto& audio_stgs
          = score::AppContext().settings<Audio::Settings::Model>();
      auto audio_node = std::make_unique<ossia::net::generic_node>(
          "Audio", *this, root);
      audio_node->set_parameter(
          std::make_unique<gstreamer_output_audio_parameter>(
              node, set.audio_channels, audio_stgs.getBufferSize(),
              *audio_node));
      root.add_child(std::move(audio_node));
    }
  }

  const ossia::net::generic_node& get_root_node() const override
  {
    return root;
  }
  ossia::net::generic_node& get_root_node() override { return root; }
};

// Score-facing output device
class OutputDevice final : public Gfx::GfxOutputDevice
{
  W_OBJECT(OutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~OutputDevice();

private:
  void disconnect() override;
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  mutable std::unique_ptr<gstreamer_output_device> m_dev;
};

W_OBJECT_IMPL(Gfx::GStreamer::OutputDevice)

OutputDevice::~OutputDevice() { }

void OutputDevice::disconnect()
{
  GfxOutputDevice::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

bool OutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings()
                   .deviceSpecificSettings.value<GStreamerSettings>();
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      auto proto
          = new gstreamer_output_protocol{set, plug->exec};
      m_dev = std::make_unique<gstreamer_output_device>(
          set, *proto->output_node,
          std::unique_ptr<gstreamer_output_protocol>(proto),
          this->settings().name.toStdString());
      deviceChanged(nullptr, m_dev.get());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "GStreamer output: Could not connect:" << e.what();
  }
  catch(...)
  {
  }

  return connected();
}

// Factory function called from GStreamerDevice.cpp's ProtocolFactory::makeDevice()
Device::DeviceInterface* makeOutputDevice(
    const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
{
  return new OutputDevice(settings, ctx);
}

}
