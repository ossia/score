// =============================================================================
// WHICH WAY UP a video output hands its pixels over, per backend.
//
// Every output that produces CPU pixels does it one of two ways, and the two
// are supposed to agree:
//
//   A. Gfx::InvertYRenderer -- a full-screen pass that samples the scene target
//      and reads the result back. Used by the Libav output's RGBA path, the
//      GStreamer output's RGBA path, Shmdata, Sh4lt, NDI and PipeWire.
//
//   B. a score::gfx::GPUVideoEncoder -- a full-screen pass per plane that
//      converts to YUV and reads each plane back, with no InvertYRenderer in
//      front of it because the encoder shader does the flip itself. Used by the
//      Libav and GStreamer outputs whenever the target pixel format is one the
//      GPU encoders cover -- yuv420p, nv12, uyvy422, yuv422p10le, p010 -- which
//      is to say by nearly every real recording or stream: h264 to a file or
//      over UDP lands on yuv420p.
//
// Both are fed the same offscreen colour target, and both must deliver a
// top-down picture: that is what libav, GStreamer, NDI and every consumer read.
//
// The oracle is a vertical ramp painted by the source, row 0 black to row H-1
// white, in a buffer whose row 0 is the top. It is asserted per row against the
// closed-form ramp, not by corner probes: a vertical flip passes "not blank",
// "has both dark and bright pixels" and every other loose check.
//
//   DISPLAY=:0 ctest -R gfx_video_output_orientation
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_video_output_orientation
// =============================================================================

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/TexgenNode.hpp>
#include <Gfx/Graph/encoders/I420.hpp>
#include <Gfx/Graph/encoders/NV12.hpp>
#include <Gfx/Graph/encoders/P010.hpp>
#include <Gfx/Graph/encoders/UYVY.hpp>
#include <Gfx/Graph/encoders/YUV422P10.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>
#include <Gfx/Libav/LibavEncoderNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <QDir>
#include <QTemporaryDir>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <QOffscreenSurface>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;
using Catch::Approx;

namespace
{
constexpr int kW = 64;
constexpr int kH = 64;

//! Row 0 black, row H-1 white, constant along X. Row 0 is the TOP: that is what
//! every consumer of these outputs assumes of the bytes it is handed.
void paintVerticalRamp(unsigned char* rgba, int w, int h, int /*t*/)
{
  for(int y = 0; y < h; y++)
  {
    const auto v = (unsigned char)std::lround(255.0 * double(y) / double(h - 1));
    for(int x = 0; x < w; x++)
    {
      auto* p = rgba + (std::size_t(y) * w + x) * 4;
      p[0] = v;
      p[1] = v;
      p[2] = v;
      p[3] = 255;
    }
  }
}

//! How the sink gets its pixels out.
enum class OutPath
{
  InvertY,  //!< Gfx::InvertYRenderer + RGBA readback
  EncI420,  //!< score::gfx::I420Encoder, Y plane
  EncNV12,  //!< score::gfx::NV12Encoder, Y plane
  EncUYVY,  //!< score::gfx::UYVYEncoder, packed
  EncP010,  //!< score::gfx::P010Encoder, 10-bit Y in the high bits
  Enc422P10, //!< score::gfx::YUV422P10Encoder, 10-bit Y
};

const char* pathName(OutPath p)
{
  switch(p)
  {
    case OutPath::InvertY:
      return "InvertYRenderer";
    case OutPath::EncI420:
      return "I420Encoder";
    case OutPath::EncNV12:
      return "NV12Encoder";
    case OutPath::EncUYVY:
      return "UYVYEncoder";
    case OutPath::EncP010:
      return "P010Encoder";
    case OutPath::Enc422P10:
      return "YUV422P10Encoder";
  }
  return "?";
}

std::unique_ptr<score::gfx::GPUVideoEncoder> makeEncoder(OutPath p)
{
  switch(p)
  {
    case OutPath::EncI420:
      return std::make_unique<score::gfx::I420Encoder>();
    case OutPath::EncNV12:
      return std::make_unique<score::gfx::NV12Encoder>();
    case OutPath::EncUYVY:
      return std::make_unique<score::gfx::UYVYEncoder>();
    case OutPath::EncP010:
      return std::make_unique<score::gfx::P010Encoder>();
    case OutPath::Enc422P10:
      return std::make_unique<score::gfx::YUV422P10Encoder>();
    default:
      return {};
  }
}

/**
 * @brief The shape every readback video output has: an offscreen colour target
 *        the graph renders into, plus one of the two ways of getting the
 *        pixels off the GPU.
 *
 * Modelled on Gfx::LibavEncoderNode, which is also what the GStreamer output
 * does; the Shmdata, Sh4lt, NDI and PipeWire outputs are the InvertY half of it.
 */
class OrientationSink final : public score::gfx::OutputNode
{
public:
  explicit OrientationSink(OutPath p)
      : m_path{p}
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }

  ~OrientationSink() override { }

  bool canRender() const override { return bool(m_renderState); }
  void startRendering() override { }
  void stopRendering() override { }
  void onRendererChange() override { }
  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override
  {
    m_renderer = r;
  }
  score::gfx::RenderList* renderer() const override { return m_renderer.lock().get(); }
  std::shared_ptr<score::gfx::RenderState> renderState() const override
  {
    return m_renderState;
  }
  Configuration configuration() const noexcept override
  {
    return {.manualRenderingRate = 1000. / 30.};
  }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState
        = score::gfx::createRenderState(conf.graphicsApi, QSize(kW, kH), nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;

    auto rhi = m_renderState->rhi;
    m_renderState->renderFormat = QRhiTexture::RGBA8;
    m_texture = rhi->newTexture(
        QRhiTexture::RGBA8, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();

    m_depthStencil = rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil, m_renderState->renderSize, 1);
    m_depthStencil->create();

    QRhiTextureRenderTargetDescription desc{m_texture};
    desc.setDepthStencilBuffer(m_depthStencil);
    m_renderTarget = rhi->newTextureRenderTarget(desc);
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    if(auto enc = makeEncoder(m_path))
    {
      m_encoder = std::move(enc);
      m_encoder->init(*rhi, *m_renderState, m_texture, kW, kH);
    }

    if(conf.onReady)
      conf.onReady();
  }

  void destroyOutput() override
  {
    if(m_encoder)
    {
      m_encoder->release();
      m_encoder.reset();
    }
    if(m_renderState)
    {
      delete m_renderTarget;
      m_renderTarget = nullptr;
      delete m_renderState->renderPassDescriptor;
      m_renderState->renderPassDescriptor = nullptr;
      delete m_depthStencil;
      m_depthStencil = nullptr;
      delete m_texture;
      m_texture = nullptr;
      m_renderState->destroy();
      m_renderState.reset();
    }
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    score::gfx::TextureRenderTarget rt{
        .texture = m_texture,
        .renderPass = m_renderState->renderPassDescriptor,
        .renderTarget = m_renderTarget};

    if(m_encoder)
      return new Gfx::BasicRenderer{rt, *m_renderState, *this};

    return new Gfx::InvertYRenderer{
        *this, rt, const_cast<QRhiReadbackResult&>(m_readback)};
  }

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
    if(m_encoder)
      m_encoder->exec(*rhi, *cb);
    rhi->endOffscreenFrame();
  }

  //! The luma of the delivered picture, row by row, 0..255. Empty when the
  //! path produced nothing.
  std::vector<int> lumaRows() const
  {
    std::vector<int> rows;
    if(m_encoder)
    {
      // Plane 0 is Y (R8) at full resolution for I420/NV12; for UYVY it is an
      // RGBA8 texture of width/2 holding U Y V Y, so Y is every odd byte.
      const auto& rb = m_encoder->readback(0);
      if(rb.data.isEmpty())
        return rows;
      const auto* d = reinterpret_cast<const uint8_t*>(rb.data.constData());
      const int h = rb.pixelSize.height();
      if(h <= 0)
        return rows;
      const int stride = rb.data.size() / h;
      // The 10-bit planes are little-endian uint16 samples. p010 carries its
      // 10 bits in the HIGH end of the word, yuv422p10le in the low end --
      // the same split EncoderTester checks the packing against.
      const bool tenBit
          = (m_path == OutPath::EncP010 || m_path == OutPath::Enc422P10);
      const int shift = (m_path == OutPath::EncP010) ? 8 : 2;
      for(int y = 0; y < h; y++)
      {
        const uint8_t* r = d + std::size_t(y) * stride;
        if(tenBit)
        {
          const auto* u = reinterpret_cast<const uint16_t*>(r);
          rows.push_back((u[kW / 2] >> shift) & 0xFF);
        }
        else
        {
          // Sample away from the edges; the ramp is constant along X.
          const int x = (m_path == OutPath::EncUYVY) ? (kW / 2) | 1 : kW / 2;
          rows.push_back(r[x]);
        }
      }
    }
    else
    {
      if(m_readback.data.isEmpty())
        return rows;
      const auto* d = reinterpret_cast<const uint8_t*>(m_readback.data.constData());
      const int h = m_readback.pixelSize.height();
      if(h <= 0)
        return rows;
      const int stride = m_readback.data.size() / h;
      for(int y = 0; y < h; y++)
        rows.push_back(d[std::size_t(y) * stride + (kW / 2) * 4 + 1]);
    }
    return rows;
  }

private:
  OutPath m_path{};
  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiRenderBuffer* m_depthStencil{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::unique_ptr<score::gfx::GPUVideoEncoder> m_encoder{};
  QRhiReadbackResult m_readback{};
};

//! What paints the picture. The two differ in how they get their geometry into
//! clip space, which is the other half of the orientation question.
enum class Source
{
  Texgen, //!< score::gfx::TexgenNode: uploads a CPU buffer, row 0 first
  Isf,    //!< an ISF shader, isf_FragNormCoord.y == 1 at the TOP per the spec
};

const char* sourceName(Source s)
{
  return s == Source::Texgen ? "TexgenNode" : "ISF isf-gradient-y";
}

struct Shot
{
  bool skipped{};
  std::string error;
  std::vector<int> rows;
};

Shot runPath(score::gfx::GraphicsApi api, OutPath path, Source source)
{
  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::unique_ptr<score::gfx::Node> owned_src;
    score::gfx::Node* src{};
    if(source == Source::Texgen)
    {
      auto* t = new score::gfx::TexgenNode;
      t->function = &paintVerticalRamp;
      src = t;
    }
    else
    {
      const QString path
          = QDir{QStringLiteral(GFX_TEST_CORPUS_DIR)}.filePath("isf-gradient-y.fs");
      auto built = make_isf_node(path);
      if(!built.node)
      {
        out.error = built.error;
        return;
      }
      owned_src = std::move(built.node);
      src = owned_src.get();
    }
    auto* sink = new OrientationSink{path};

    auto graph = std::make_unique<score::gfx::Graph>();
    graph->addNode(src);
    graph->addNode(sink);
    graph->addEdge(
        src->output[0], sink->input[0], Process::CableType::ImmediateGlutton);
    graph->createAllRenderLists(api);

    if(!sink->canRender())
    {
      out.skipped = true;
      graph.reset();
      delete sink;
      if(!owned_src)
        delete src;
      return;
    }

    // A few frames: the source uploads its texture from update(), so the first
    // frame can be the clear colour.
    for(int i = 0; i < 4; i++)
      sink->render();

    out.rows = sink->lumaRows();
    if(out.rows.empty())
      out.error = "no pixels read back";

    graph.reset();
    delete sink;
    // owned_src deletes the ISF node when it goes out of scope, after the
    // graph; a Texgen node was new'd by hand and is deleted by hand.
    if(!owned_src)
      delete src;
  });
  return out;
}

//! Largest deviation from the closed-form ramp, and where.
struct RampFit
{
  int worst{}, worstRow{}, got{}, expected{};
};

//! Scale-invariant: the encoders put the ramp through a BT.709 luma with its
//! own range, so what is asserted is the SHAPE -- which end is which -- and not
//! the absolute value.
RampFit fitRamp(const std::vector<int>& rows, bool topIsDark)
{
  RampFit f;
  const int H = rows.size();
  const auto [lo, hi] = std::minmax_element(rows.begin(), rows.end());
  const double span = double(*hi - *lo);
  if(span < 1.)
    return {255, 0, rows.front(), -1};

  for(int y = 0; y < H; y++)
  {
    const double t = double(y) / double(H - 1);
    const double want = topIsDark ? t : (1.0 - t);
    const int expected = int(std::lround(*lo + span * want));
    const int d = std::abs(rows[y] - expected);
    if(d > f.worst)
      f = {d, y, rows[y], expected};
  }
  return f;
}

RampFit fitTopDown(const std::vector<int>& rows)
{
  return fitRamp(rows, true);
}

RampFit fitBottomUp(const std::vector<int>& rows)
{
  return fitRamp(rows, false);
}
}

TEST_CASE(
    "a video output hands over a top-down picture",
    "[gfx][video][output][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto source = GENERATE(Source::Texgen, Source::Isf);
  const auto path = GENERATE(
      OutPath::InvertY, OutPath::EncI420, OutPath::EncNV12, OutPath::EncUYVY,
      OutPath::EncP010, OutPath::Enc422P10);

  const auto shot = runPath(api, path, source);

  INFO(
      "backend " << backend_name(api) << ", source " << sourceName(source)
                 << ", path " << pathName(path));
  if(shot.skipped)
    SKIP("backend unavailable");
  REQUIRE(shot.error.empty());
  REQUIRE(shot.rows.size() >= 8);

  // Not a flat picture: the ramp is really there.
  const auto [lo, hi]
      = std::minmax_element(shot.rows.begin(), shot.rows.end());
  REQUIRE((*hi - *lo) > 100);

  // TexgenNode paints row 0 black; the ISF reference is green == 1 at the TOP,
  // so its row 0 is white. Either way row 0 is the top of the picture.
  const bool topIsDark = (source == Source::Texgen);
  const auto fit = topIsDark ? fitTopDown(shot.rows) : fitBottomUp(shot.rows);
  INFO(
      "row " << fit.worstRow << ": got " << fit.got << ", expected "
             << fit.expected << " (first " << shot.rows.front() << ", last "
             << shot.rows.back() << ")");
  CHECK(fit.worst <= 24);
}

// Same picture, two ways off the GPU: whatever they deliver, they must deliver
// the same thing. This is the assertion that catches one path being corrected
// for a backend the other is not.
TEST_CASE(
    "the readback and the GPU encoder agree on which way up",
    "[gfx][video][output][orientation]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto source = GENERATE(Source::Texgen, Source::Isf);

  const auto ref = runPath(api, OutPath::InvertY, source);
  if(ref.skipped)
    SKIP("backend unavailable");
  REQUIRE(ref.error.empty());
  REQUIRE(ref.rows.size() >= 8);

  for(auto path :
      {OutPath::EncI420, OutPath::EncNV12, OutPath::EncUYVY, OutPath::EncP010,
       OutPath::Enc422P10})
  {
    const auto shot = runPath(api, path, source);
    INFO(
        "backend " << backend_name(api) << ", source " << sourceName(source)
                   << ", path " << pathName(path));
    if(shot.skipped)
      continue;
    REQUIRE(shot.error.empty());
    REQUIRE(shot.rows.size() == ref.rows.size());

    // Compare the two ends rather than every row: the encoders go through a
    // full-range BT.709 luma, so the values are not the ramp itself.
    const bool refTopDark = ref.rows.front() < ref.rows.back();
    const bool gotTopDark = shot.rows.front() < shot.rows.back();
    INFO(
        "reference " << ref.rows.front() << ".." << ref.rows.back() << ", got "
                     << shot.rows.front() << ".." << shot.rows.back());
    CHECK(refTopDark == gotTopDark);
  }
}


// =============================================================================
// The whole FFmpeg output, end to end: the same ISF ramp, encoded to a file by
// the real Gfx::LibavEncoderNode at the pixel format an h264 stream actually
// uses, and decoded back. This is the user's report -- "ffmpeg udp in ->
// passthrough shader -> ffmpeg udp out, Y is upside down" -- minus the network.
// =============================================================================
namespace
{
//! Luma of the middle column of `frame`, row by row.
std::vector<int> lumaRowsOf(const AVFrame& f)
{
  std::vector<int> rows;
  if(f.height <= 0 || f.width <= 0)
    return rows;
  // Y plane of any planar YUV; for anything else, convert.
  if(f.format == AV_PIX_FMT_YUV420P || f.format == AV_PIX_FMT_YUVJ420P)
  {
    for(int y = 0; y < f.height; y++)
      rows.push_back(f.data[0][std::size_t(y) * f.linesize[0] + f.width / 2]);
    return rows;
  }

  SwsContext* sws = sws_getContext(
      f.width, f.height, AVPixelFormat(f.format), f.width, f.height,
      AV_PIX_FMT_GRAY8, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if(!sws)
    return rows;
  std::vector<uint8_t> gray(std::size_t(f.width) * f.height);
  uint8_t* dst[4]{gray.data(), nullptr, nullptr, nullptr};
  int stride[4]{f.width, 0, 0, 0};
  sws_scale(sws, f.data, f.linesize, 0, f.height, dst, stride);
  sws_freeContext(sws);
  for(int y = 0; y < f.height; y++)
    rows.push_back(gray[std::size_t(y) * f.width + f.width / 2]);
  return rows;
}

//! First decodable frame of `path`, as its luma rows.
std::vector<int> decodeFirstFrameRows(const QString& path)
{
  std::vector<int> rows;
  AVFormatContext* fmt{};
  if(avformat_open_input(&fmt, path.toUtf8().constData(), nullptr, nullptr) != 0)
    return rows;
  if(avformat_find_stream_info(fmt, nullptr) < 0)
  {
    avformat_close_input(&fmt);
    return rows;
  }
  int vs = -1;
  for(unsigned i = 0; i < fmt->nb_streams; i++)
    if(fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      vs = int(i);
      break;
    }
  if(vs < 0)
  {
    avformat_close_input(&fmt);
    return rows;
  }
  const AVCodec* dec = avcodec_find_decoder(fmt->streams[vs]->codecpar->codec_id);
  AVCodecContext* cc = dec ? avcodec_alloc_context3(dec) : nullptr;
  if(!cc || avcodec_parameters_to_context(cc, fmt->streams[vs]->codecpar) < 0
     || avcodec_open2(cc, dec, nullptr) < 0)
  {
    if(cc)
      avcodec_free_context(&cc);
    avformat_close_input(&fmt);
    return rows;
  }
  AVPacket* pkt = av_packet_alloc();
  AVFrame* frame = av_frame_alloc();
  while(rows.empty() && av_read_frame(fmt, pkt) >= 0)
  {
    if(pkt->stream_index == vs && avcodec_send_packet(cc, pkt) == 0)
      while(rows.empty() && avcodec_receive_frame(cc, frame) == 0)
        rows = lumaRowsOf(*frame);
    av_packet_unref(pkt);
  }
  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&cc);
  avformat_close_input(&fmt);
  return rows;
}

Gfx::LibavOutputSettings mp4Settings(const QString& file, int w, int h)
{
  Gfx::LibavOutputSettings set;
  set.path = file;
  set.width = w;
  set.height = h;
  set.rate = 30.;
  set.muxer = "mp4";
  set.video_encoder_short = "libx264";
  // What an h264 stream is: this is the branch that engages the GPU I420
  // encoder rather than the RGBA readback.
  set.video_converted_pixfmt = "yuv420p";
  set.video_render_pixfmt = "rgba";
  set.threads = 1;
  set.options["preset"] = "ultrafast";
  return set;
}
}

TEST_CASE(
    "the FFmpeg output records a top-down picture",
    "[gfx][video][output][orientation][libav]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString file = dir.path() + "/orientation.mp4";

  bool skipped = false, started = false;
  std::string error;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    const QString shader
        = QDir{QStringLiteral(GFX_TEST_CORPUS_DIR)}.filePath("isf-gradient-y.fs");
    auto built = make_isf_node(shader);
    if(!built.node)
    {
      error = built.error;
      return;
    }

    auto set = mp4Settings(file, kW, kH);
    Gfx::LibavEncoder enc{set};
    if(enc.start() != 0 || !enc.available())
    {
      skipped = true; // no libx264 in this build
      return;
    }
    started = true;

    auto* sink = new Gfx::LibavEncoderNode{set, enc, 0};
    auto graph = std::make_unique<score::gfx::Graph>();
    graph->addNode(built.node.get());
    graph->addNode(sink);
    graph->addEdge(
        built.node->output[0], sink->input[0],
        Process::CableType::ImmediateGlutton);
    graph->createAllRenderLists(api);

    if(!sink->canRender())
    {
      skipped = true;
      graph.reset();
      delete sink;
      return;
    }

    for(int i = 0; i < 12; i++)
      sink->render();

    graph.reset();
    delete sink;
    enc.stop();
  });

  INFO("backend " << backend_name(api));
  if(skipped)
    SKIP("libx264 or the backend is unavailable here");
  REQUIRE(error.empty());
  REQUIRE(started);

  const auto rows = decodeFirstFrameRows(file);
  REQUIRE(rows.size() >= 8);

  const auto [lo, hi] = std::minmax_element(rows.begin(), rows.end());
  INFO("first " << rows.front() << ", last " << rows.back());
  REQUIRE((*hi - *lo) > 60);

  // isf-gradient-y is bright at the TOP: row 0 of the recorded frame is the
  // bright end, or the recording is upside down.
  const auto fit = fitBottomUp(rows);
  INFO(
      "row " << fit.worstRow << ": got " << fit.got << ", expected "
             << fit.expected);
  CHECK(fit.worst <= 40);
}
