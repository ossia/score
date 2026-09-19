#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> NV12 / I420 / YV12, one texture holding the framestore.
 *
 * The plane encoders give two or three plane textures; a consumer that hands a
 * device one pointer then has to concatenate them, and pays a GPU->CPU round
 * trip per plane to get there. This produces the framestore directly, so one
 * readback is the whole thing. At 2160p that is 2.4 ms a frame against 10.3.
 *
 * Plane geometry, which is what chroma_byte() has to navigate:
 *
 *   NV12:  Y is w bytes over h rows; interleaved CbCr is also w bytes
 *          (w/2 sites x 2 components) over h/2 rows.       Uniform rows.
 *   I420:  Y is w bytes over h rows; Cb then Cr are w/2 bytes over h/2 rows.
 *   YV12:  I420 with Cr before Cb.
 *
 * The target is RGBA8 of (w/4) x (3h/2), four framestore bytes per texel, and
 * R8 of w x (3h/2) when the width is not a multiple of four -- 722 and 1922
 * are even, so 4:2:0 can express them, but an RGBA8 target cannot be w/4
 * texels wide. One byte per texel runs a fragment, a fetch and a colour
 * conversion per byte, which measured slower than the plane encoders it
 * replaces, so it is the fallback rather than the rule.
 *
 * Chroma is a 2x2 box, centre-sited. Both plausible improvements were tried
 * and measured worse against a real NDI receiver; they remain in chroma_at()
 * behind SCORE_GFX_CHROMA_SITING so the measurement can be repeated.
 *
 *   Siting. MPEG-2/H.264 put the sample on the even luma column. Sending
 *   colour bars through the SDK and locating each chroma edge on the way back:
 *   centre displaced it by -0.001 luma columns, left by +0.165. The receiver
 *   assumes centre.
 *
 *   Pre-filtering. [1 3 3 1]/8 each way band-limits before decimating. On
 *   red/blue stripes round-tripped through the SDK, mean |RGB error| by stripe
 *   period in luma columns:
 *
 *       period       4      6      8     12     16     32
 *       box       5.48  33.06   8.46   5.23   3.15   2.67
 *       [1 3 3 1] 49.63  47.04  27.02  15.74  12.95   8.02
 *
 *   Worse everywhere, including at Nyquist: the receiver upsamples with
 *   something box-like and does not reconstruct a band-limited signal, so the
 *   blur costs more than the aliasing it removes.
 *
 * Requires even width and height.
 */
struct Yuv420PackedEncoder : GPUVideoEncoder
{
  /// Where a chroma sample sits horizontally relative to its luma pair.
  enum class Siting
  {
    Centre = 0,  ///< between the two columns: a 2x2 box, what swscale emits
    Left = 1,    ///< on the even column, [1 2 1]/4: what MPEG-2/H.264 specify
    CentreWide = 2,  ///< centre, but [1 3 3 1]/8 each way instead of a box
  };

  enum class Layout
  {
    NV12,  ///< Y, then Cb/Cr interleaved
    I420,  ///< Y, then Cb, then Cr
    YV12,  ///< Y, then Cr, then Cb
  };

  /// One byte of the chroma region, for a semi-planar layout: site b/2 of row
  /// cr, Cb on even bytes and Cr on odd ones.
  static constexpr const char* chroma_nv12 = R"_(
    float chroma_byte(int b, int cr, ivec2 sz) {
      vec2 uv = chroma_at(b >> 1, cr, sz);
      return ((b & 1) == 0) ? uv.x : uv.y;
    }
  )_";

  /// And for the fully planar ones. %1 / %2 are the components of the first and
  /// second plane: (x, y) is Cb-then-Cr (I420), (y, x) is Cr-then-Cb (YV12).
  static constexpr const char* chroma_planar = R"_(
    float chroma_byte(int b, int cr, ivec2 sz) {
      int cw = sz.x >> 1;      // bytes in one chroma plane row
      int ch = sz.y >> 1;      // rows in one chroma plane
      // A target row is w bytes and a chroma plane row is w/2, so a target row
      // holds EXACTLY two chroma rows. That makes the mapping shifts and
      // compares -- no integer division, which is worth avoiding here because
      // it would run once per chroma byte of every frame.
      int sel = (b < cw) ? 0 : 1;   // "half" is a GLSL reserved word
      int col = b - sel * cw;
      int gr = (cr << 1) + sel;   // chroma row counted across both planes
      bool first = gr < ch;
      vec2 uv = chroma_at(col, first ? gr : gr - ch, sz);
      return first ? uv.%1 : uv.%2;
    }
  )_";

  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  // %2 = the chroma_byte() for this layout
  static constexpr const char* frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(

    // Constants, so the branches on them fold away. Declared before
    // chroma_at(), which uses SITING.
    const int BPT = %3;
    const int SITING = %4;

    vec2 flip_y(vec2 tc) {
    // Only OpenGL: this pass draws a hardcoded triangle in raw NDC rather than
    // going through renderer.clipSpaceCorrMatrix.
    //
    // It flips the SOURCE lookup, never the target row order -- v_texcoord.y
    // runs from 0 at the first row of the readback on both backends, which is
    // what lets the luma/chroma split sit anywhere rather than at the midpoint.
    #if defined(QSHADER_SPIRV) || defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }

    // Luma of source pixel (x, y). At an exact texel centre a Linear sampler
    // returns that texel, so this is the plane encoder's full-size pass.
    float luma_at(int x, int y, ivec2 sz) {
      vec2 tc = (vec2(float(x), float(y)) + 0.5) / vec2(sz);
      return convert_from_rgb(texture(src_tex, flip_y(tc)).rgb).x;
    }

    // (Cb, Cr) of chroma site (cx, cy). Vertically always the boundary
    // between source rows 2*cy and 2*cy+1, so one tap averages the pair.
    vec2 chroma_at(int cx, int cy, ivec2 sz) {
      vec2 fs = vec2(sz);
      float y = float((cy << 1) + 1) / fs.y;          // boundary of the row pair
      if(SITING == 0)
      {
        float x = float((cx << 1) + 1) / fs.x;        // boundary of the column pair
        return convert_from_rgb(texture(src_tex, flip_y(vec2(x, y))).rgb).yz;
      }
      if(SITING == 2)
      {
        // [1 3 3 1]/8 each way as four bilinear taps: a tap at t=0.75 weights
        // its pair 1:3 and one at t=0.25 weights it 3:1, so averaging the two
        // gives 1:3:3:1 centred on the same boundary the box uses.
        float xa = (float(cx << 1) + 0.25) / fs.x;
        float xb = (float(cx << 1) + 1.75) / fs.x;
        float ya = (float(cy << 1) + 0.25) / fs.y;
        float yb = (float(cy << 1) + 1.75) / fs.y;
        vec3 p0 = convert_from_rgb(texture(src_tex, flip_y(vec2(xa, ya))).rgb);
        vec3 p1 = convert_from_rgb(texture(src_tex, flip_y(vec2(xb, ya))).rgb);
        vec3 p2 = convert_from_rgb(texture(src_tex, flip_y(vec2(xa, yb))).rgb);
        vec3 p3 = convert_from_rgb(texture(src_tex, flip_y(vec2(xb, yb))).rgb);
        return (p0.yz + p1.yz + p2.yz + p3.yz) * 0.25;
      }
      float xa = float(cx << 1) / fs.x;               // left of column 2*cx
      float xb = float((cx << 1) + 1) / fs.x;         // right of it
      vec3 a = convert_from_rgb(texture(src_tex, flip_y(vec2(xa, y))).rgb);
      vec3 b = convert_from_rgb(texture(src_tex, flip_y(vec2(xb, y))).rgb);
      return (a.yz + b.yz) * 0.5;
    }
    )_" "%2" R"_(


    float byte_at(int b, int outRow, ivec2 sz) {
      return (outRow < sz.y) ? luma_at(b, outRow, sz)
                             : chroma_byte(b, outRow - sz.y, sz);
    }

    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      int outRows = sz.y + (sz.y >> 1);

      int outRow = int(floor(v_texcoord.y * float(outRows)));
      int t      = int(floor(v_texcoord.x * float(sz.x / BPT)));
      int b0     = t * BPT;

      if(BPT == 1)
        fragColor = vec4(byte_at(b0, outRow, sz), 0.0, 0.0, 1.0);
      else
        fragColor = vec4(
            byte_at(b0,     outRow, sz), byte_at(b0 + 1, outRow, sz),
            byte_at(b0 + 2, outRow, sz), byte_at(b0 + 3, outRow, sz));
    }
  )_";

  explicit Yuv420PackedEncoder(Layout layout, Siting siting = Siting::Centre) noexcept
      : m_layout{layout}
      , m_siting{siting}
  {
  }

  static std::unique_ptr<Yuv420PackedEncoder> nv12()
  {
    return std::make_unique<Yuv420PackedEncoder>(Layout::NV12);
  }
  static std::unique_ptr<Yuv420PackedEncoder> i420()
  {
    return std::make_unique<Yuv420PackedEncoder>(Layout::I420);
  }
  static std::unique_ptr<Yuv420PackedEncoder> yv12()
  {
    return std::make_unique<Yuv420PackedEncoder>(Layout::YV12);
  }

  Layout m_layout{Layout::NV12};
  Siting m_siting{Siting::Centre};
  QRhiTexture* m_outTexture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  QRhiRenderPassDescriptor* m_rpDesc{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiGraphicsPipeline* m_pipeline{};
  QRhiReadbackResult m_readback{};
  int m_width{};
  int m_height{};
  int m_bytesPerTexel{1};
  bool m_readbackEnabled{true};

  /// Rows in the whole framestore: the picture, plus half of it again.
  int framestoreRows() const noexcept { return m_height + m_height / 2; }

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& colorConversion) override
  {
    m_width = width;
    m_height = height;

    // Overridable so the two sitings can be compared against a real receiver
    // rather than argued about. See the chroma_at() comment.
    if(const auto env = qgetenv("SCORE_GFX_CHROMA_SITING"); !env.isEmpty())
    {
      const auto e = env.toLower();
      m_siting = (e == "left")   ? Siting::Left
                 : (e == "wide") ? Siting::CentreWide
                                 : Siting::Centre;
    }

    m_bytesPerTexel = (width % 4 == 0) ? 4 : 1;
    m_outTexture = rhi.newTexture(
        m_bytesPerTexel == 4 ? QRhiTexture::RGBA8 : QRhiTexture::R8,
        QSize{width / m_bytesPerTexel, framestoreRows()}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_outTexture->create();

    m_renderTarget = rhi.newTextureRenderTarget({m_outTexture});
    m_rpDesc = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_rpDesc);
    m_renderTarget->create();

    // Linear: the chroma taps need bilinear filtering to average each block.
    // Luma taps sit on exact texel centres, where Linear is a no-op.
    m_sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    m_srb->create();

    QString chroma;
    switch(m_layout)
    {
      case Layout::NV12:
        chroma = QString::fromLatin1(chroma_nv12);
        break;
      case Layout::I420:  // Cb first, then Cr
        chroma = QString::fromLatin1(chroma_planar)
                     .arg(QStringLiteral("x"), QStringLiteral("y"));
        break;
      case Layout::YV12:  // Cr first, then Cb
        chroma = QString::fromLatin1(chroma_planar)
                     .arg(QStringLiteral("y"), QStringLiteral("x"));
        break;
    }

    // One pass, so a %2 inside colorConversion cannot be eaten as a placeholder.
    auto [vertS, fragS] = makeShaders(
        state, QString::fromLatin1(vertex_shader),
        QString::fromLatin1(frag).arg(
            colorConversion, chroma, QString::number(m_bytesPerTexel),
            QString::number(int(m_siting))));

    m_pipeline = rhi.newGraphicsPipeline();
    m_pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertS},
        {QRhiShaderStage::Fragment, fragS},
    });
    m_pipeline->setVertexInputLayout({});
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setRenderPassDescriptor(m_rpDesc);
    m_pipeline->create();
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    cb.beginPass(m_renderTarget, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.setViewport(
        QRhiViewport(0, 0, m_width / m_bytesPerTexel, framestoreRows()));
    cb.draw(3);

    if(m_readbackEnabled)
    {
      auto* readbackBatch = rhi.nextResourceUpdateBatch();
      QRhiReadbackDescription rb(m_outTexture);
      readbackBatch->readBackTexture(rb, &m_readback);
      cb.endPass(readbackBatch);
    }
    else
    {
      cb.endPass();
    }
  }

  int planeCount() const override { return 1; }

  const QRhiReadbackResult& readback(int) const override { return m_readback; }
  QRhiTexture* outputTexture() const noexcept override { return m_outTexture; }
  void setReadbackEnabled(bool e) noexcept override { m_readbackEnabled = e; }

  void release() override
  {
    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_srb;
    m_srb = nullptr;
    delete m_sampler;
    m_sampler = nullptr;
    delete m_rpDesc;
    m_rpDesc = nullptr;
    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_outTexture;
    m_outTexture = nullptr;
  }
};

} // namespace score::gfx
