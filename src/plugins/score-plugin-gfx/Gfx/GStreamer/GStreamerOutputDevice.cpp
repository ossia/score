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
#include <Gfx/Graph/encoders/P010.hpp>
#include <Gfx/Graph/encoders/V210.hpp>
#include <Gfx/Graph/encoders/YUV422P10.hpp>
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
  // m_started: the pipeline is live and still needs finalization (EOS + NULL).
  // m_feeding: it is safe to push frames. Decoupled so a fatal bus ERROR stops
  // feeding without neutralizing stop_pipeline()'s EOS finalization (which is
  // what writes the muxer's moov atom / cluster index). See poll_bus_errors().
  bool m_started{};
  bool m_feeding{};
  uint64_t m_video_max_bytes{}; // appsrc queue cap; 0 = disabled
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
      // gst_parse_launch (non-_full) can return a non-NULL *partial* pipeline
      // with *error set. Such a pipeline is broken (e.g. missing appsrcs) —
      // unref it so we don't leak/retain it and never set it PLAYING.
      if(m_pipeline)
      {
        gst.object_unref(m_pipeline);
        m_pipeline = nullptr;
      }
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
        auto setUInt64 = [&](GstElement* elem, const char* prop, uint64_t val) {
          if(!gst.value_set_uint64)
            return;
          GValue gv{};
          gst.value_init(&gv, G_TYPE_UINT64);
          gst.value_set_uint64(&gv, val);
          gst.object_set_property(elem, prop, &gv);
          gst.value_unset(&gv);
        };

        setBool(m_video_src, "is-live", true);
        setBool(m_video_src, "do-timestamp", true);
        setInt(m_video_src, "format", 3); // GST_FORMAT_TIME

        // Backpressure: the appsrc default max-bytes is 200000, far below a
        // single 1080p RGBA frame (~8 MB). Bound the queue to a few frames so
        // RSS can't grow without limit when downstream stalls. We additionally
        // drop frames ourselves (see push_video_frame_*) by polling
        // current-level-bytes, which gives downstream-leaky behaviour without
        // depending on the leaky-type enum GType (not introspectable here) and
        // without blocking the render thread.
        m_video_max_bytes = (uint64_t)16 * 1024 * 1024; // ~2 frames @1080p RGBA
        setUInt64(m_video_src, "max-bytes", m_video_max_bytes);
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
    m_feeding = true;
  }

  // Non-blocking bus poll: surfaces otherwise-silent encoder/filesink/muxer
  // errors. Called once per rendered frame.
  //
  // Only a genuine GST_MESSAGE_ERROR is fatal. GStreamer routinely posts
  // GST_MESSAGE_WARNING during healthy encoding (late/dropped buffers, missing
  // PTS, encoder rate warnings); treating those as fatal would truncate an
  // otherwise-fine recording. We therefore filter on GST_MESSAGE_ERROR alone —
  // bus_timed_pop_filtered discards the non-matching (warning) messages it
  // encounters, so warnings are drained (no unbounded bus growth) but ignored.
  //
  // On a real error we clear m_feeding (stop pushing frames) but deliberately
  // leave m_started set: stop_pipeline() must still run, emit EOS and drive the
  // pipeline to GST_STATE_NULL so the muxer finalizes the file rather than
  // leaving it truncated/unplayable (or leaking a PLAYING pipeline).
  void poll_bus_errors()
  {
    if(!m_pipeline || !m_started)
      return;

    auto& gst = libgstreamer::instance();
    if(!gst.element_get_bus || !gst.bus_timed_pop_filtered)
      return;

    GstBus* bus = gst.element_get_bus(m_pipeline);
    if(!bus)
      return;

    // Drain WARNINGs first (non-fatal: QoS, v4l2/muxer notices) — log and
    // discard, they must NOT stop the pipeline (would truncate the recording).
    while(GstMessage* msg
          = gst.bus_timed_pop_filtered(bus, 0, GST_MESSAGE_WARNING))
    {
      qWarning() << "GStreamer output: pipeline warning on the bus";
      if(gst.message_unref)
        gst.message_unref(msg);
    }
    // Only an ERROR is fatal: stop feeding frames, but deliberately leave
    // m_started set so stop_pipeline() still runs EOS + drives to NULL.
    if(GstMessage* msg = gst.bus_timed_pop_filtered(bus, 0, GST_MESSAGE_ERROR))
    {
      qWarning() << "GStreamer output: fatal error on the bus; stopping frame "
                    "feed (pipeline will still be finalized)";
      if(gst.message_unref)
        gst.message_unref(msg);
      m_feeding = false;
    }
    gst.object_unref(bus);
  }

  void stop_pipeline()
  {
    if(!m_pipeline)
      return;

    auto& gst = libgstreamer::instance();

    // Only the EOS handshake is gated on m_started: if we were never
    // pushing (or an ERROR already stopped us) there's nothing to
    // flush, but we must still bring the pipeline to NULL below — an
    // early return here would leak a still-PLAYING pipeline.
    if(m_started)
    {
      // Send EOS to appsrc elements so downstream can finalize
      if(m_video_src && gst.app_src_end_of_stream)
        gst.app_src_end_of_stream(m_video_src);
      if(m_audio_src && gst.app_src_end_of_stream)
        gst.app_src_end_of_stream(m_audio_src);

      // appsrc EOS is ASYNC: it travels through the pipeline as a buffer would,
      // and muxers (mp4mux/matroskamux/...) only finalize the file once EOS
      // reaches them. Setting the pipeline to NULL immediately would truncate
      // the moov atom / cluster index, producing unplayable files. Wait for the
      // EOS (or ERROR) message on the bus, with a bounded timeout so we never
      // hang the UI thread on a stuck pipeline.
      if(gst.element_get_bus && gst.bus_timed_pop_filtered)
      {
        if(GstBus* bus = gst.element_get_bus(m_pipeline))
        {
          GstMessage* msg = gst.bus_timed_pop_filtered(
              bus, 5 * GST_SECOND,
              (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
          if(msg)
          {
            if(gst.message_unref)
              gst.message_unref(msg);
          }
          else
          {
            qWarning() << "GStreamer output: timed out waiting for EOS; "
                          "output file may be truncated";
          }
          gst.object_unref(bus);
        }
      }
    }

    gst.element_set_state(m_pipeline, GST_STATE_NULL);
    m_started = false;
    m_feeding = false;
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

  // Downstream-leaky backpressure: if appsrc's queued bytes already exceed the
  // configured budget, drop this frame instead of growing RSS without bound.
  // Reading current-level-bytes (guint64) is cheap and lock-free in appsrc.
  bool video_queue_full() const
  {
    if(m_video_max_bytes == 0 || !m_video_src)
      return false;

    auto& gst = libgstreamer::instance();
    if(!gst.object_get_property || !gst.value_init || !gst.value_unset
       || !gst.value_get_uint64)
      return false;

    GValue gv{};
    gst.value_init(&gv, G_TYPE_UINT64);
    gst.object_get_property(m_video_src, "current-level-bytes", &gv);
    uint64_t level = gst.value_get_uint64(&gv);
    gst.value_unset(&gv);
    return level >= m_video_max_bytes;
  }

  // Zero-copy push: takes a shallow copy of the QByteArray.
  // The QByteArray's refcount keeps the data alive until GStreamer is done.
  void push_video_frame_zerocopy(QByteArray data)
  {
    if(!m_video_src || !m_feeding)
      return;
    if(video_queue_full())
      return; // drop: downstream can't keep up

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
    if(!m_video_src || !m_feeding)
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
    if(!m_audio_src || !m_feeding)
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

    // Surface any silent pipeline errors (encoder/filesink/muxer failures).
    poll_bus_errors();

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
    m_renderState = score::gfx::createRenderState(
        conf.graphicsApi, QSize(m_settings.width, m_settings.height), nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      qWarning() << "GStreamerOutputNode: failed to create QRhi";
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;

    auto rhi = m_renderState->rhi;

    // init_pipeline() negotiates with GStreamer and fills m_detectedFormat, so
    // the scene render-target format (which depends on whether the output is
    // 10-bit) must be chosen AFTER it.
    const bool pipeline_ok = init_pipeline();
    if(!pipeline_ok)
      qWarning() << "GStreamerOutputNode: pipeline init failed; output disabled";

    // 10-bit output (packed v210, planar I422_10LE, or semi-planar P010_10LE)
    // needs a >8-bit scene render target for real precision; else RGBA8.
    const bool is_v210 = (m_detectedFormat == "v210");
    const bool tenBit = is_v210 || (m_detectedFormat == "I422_10LE")
                        || (m_detectedFormat == "P010_10LE");
    m_renderState->renderFormat
        = tenBit ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8;
    m_texture = rhi->newTexture(
        m_renderState->renderFormat, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(
        m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    // Create GPU encoder if a YUV target format was detected
    if(pipeline_ok && !m_detectedFormat.isEmpty() && rhi)
    {
      auto makeEncoder = [&]() -> std::unique_ptr<score::gfx::GPUVideoEncoder> {
        if(m_detectedFormat == "UYVY" || m_detectedFormat == "YUY2")
          return std::make_unique<score::gfx::UYVYEncoder>();
        else if(m_detectedFormat == "NV12")
          return std::make_unique<score::gfx::NV12Encoder>();
        else if(m_detectedFormat == "I420" || m_detectedFormat == "YV12")
          return std::make_unique<score::gfx::I420Encoder>();
        else if(m_detectedFormat == "v210")
          return std::make_unique<score::gfx::V210Encoder>();
        else if(m_detectedFormat == "I422_10LE")
          return std::make_unique<score::gfx::YUV422P10Encoder>();
        else if(m_detectedFormat == "P010_10LE")
          return std::make_unique<score::gfx::P010Encoder>();
        return nullptr;
      };

      // Create two encoder instances for double-buffered readback
      m_encoder[0] = makeEncoder();
      m_encoder[1] = makeEncoder();

      if(m_encoder[0] && m_encoder[1])
      {
        // Stride alignment: QRhi reads textures back with TIGHTLY packed rows,
        // but GStreamer's default GstVideoInfo strides are GST_ROUND_UP_4. For
        // the planar/semi-planar YUV formats the two only agree when each plane
        // row is already a multiple of 4:
        //   I420: Y stride = width, chroma stride = width/2  -> need width%8==0
        //   NV12: Y stride = width, UV   stride = width      -> need width%4==0
        //   UYVY: stride   = width*2 (4:2:2 macropixels)     -> need width%2==0
        // height must be even for 4:2:0 vertical subsampling. We round DOWN so
        // we never sample past the rendered texture, and feed the SAME aligned
        // dimensions to both the encoder and the negotiated caps so the tight
        // readback matches GStreamer's expected (now no-op ROUND_UP_4) strides.
        // v210 packs 6-px groups into a 128-byte-aligned row, so width must be
        // a multiple of 48 for the tight readback to match GStreamer's v210
        // stride (((w+47)/48)*128); it has no vertical subsampling. Other
        // formats: mult-of-8 width (covers 4:2:2 / 4:2:0) and even height.
        // v210 packs 6-px groups into 128-byte rows (width % 48). Other
        // formats need mult-of-8 width. Even height covers 4:2:0 vertical
        // subsampling (P010 / NV12 / I420) and is harmless for 4:2:2.
        const int enc_w = is_v210 ? std::max(48, (m_settings.width / 48) * 48)
                                  : std::max(8, m_settings.width & ~7);
        const int enc_h = std::max(2, m_settings.height & ~1);
        if(enc_w != m_settings.width || enc_h != m_settings.height)
          qDebug() << "GStreamer output: aligning" << m_detectedFormat
                   << "from" << m_settings.width << "x" << m_settings.height
                   << "to" << enc_w << "x" << enc_h << "for packed strides";

        auto input_trc = static_cast<AVColorTransferCharacteristic>(m_settings.input_transfer);
        auto colorShader = colorShaderFromColorimetry(m_detectedColorimetry, input_trc);
        qDebug() << "GStreamer output: GPU encoder"
                 << "format=" << m_detectedFormat
                 << "colorimetry=" << m_detectedColorimetry
                 << "inputTrc=" << m_settings.input_transfer
                 << "shaderLen=" << colorShader.size();
        m_encoder[0]->init(*rhi, *m_renderState, m_texture,
                           enc_w, enc_h, colorShader);
        m_encoder[1]->init(*rhi, *m_renderState, m_texture,
                           enc_w, enc_h, colorShader);

        // Update appsrc caps to match the encoder's output format
        if(auto& gst = libgstreamer::instance();
           gst.caps_from_string && gst.app_src_set_caps && m_video_src)
        {
          auto capsStr = QString("video/x-raw,format=%1,width=%2,height=%3,framerate=%4/1")
                             .arg(m_detectedFormat)
                             .arg(enc_w)
                             .arg(enc_h)
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

    // Reset per-instance frame/encoder state so a subsequent createOutput()
    // (re-create on settings change) starts clean instead of reusing a stale
    // readback, ping-pong index, detected format or dangling renderer pointer.
    m_currentReadback = &m_readback[0];
    m_readback[0] = {};
    m_readback[1] = {};
    m_encoderIdx = 0;
    m_detectedFormat.clear();
    m_detectedColorimetry.clear();
    m_inv_y_renderer = nullptr;
    m_video_max_bytes = 0;

    if(!m_renderState)
      return;

    // Persist-across-rebuild contract: registry survives RL teardown,
    // so we tear down its QRhi resources here BEFORE
    // RenderState::destroy() (called below) frees the device.
    releaseRegistry();

    delete m_renderTarget;
    m_renderTarget = nullptr;

    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;

    delete m_texture;
    m_texture = nullptr;

    m_renderState->destroy();
    m_renderState.reset();
  }

  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    score::gfx::TextureRenderTarget rt{
        .texture = m_texture,
        .renderPass = m_renderState->renderPassDescriptor,
        .renderTarget = m_renderTarget};

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
