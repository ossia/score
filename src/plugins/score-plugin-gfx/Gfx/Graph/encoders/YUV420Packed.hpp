#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> NV12 / I420 / YV12, as ONE texture holding the framestore.
 *
 * The plane-based encoders (NV12Encoder, YUVPlanarEncoder::p420_8) produce the
 * same bytes in two or three plane textures, which is the right shape for a
 * consumer that wants planes. A consumer that wants the framestore -- NDI's
 * p_data, a capture card's frame buffer -- then has to concatenate them,
 * because a QRhi readback lands in its own allocation per plane. That costs a
 * copy, but it costs something bigger first: two or three separate GPU->CPU
 * round trips, each with its own synchronisation. Measured at 2160p, I420 took
 * 9.97 ms a frame against UYVY's 2.65, and only about 1.2 ms of that gap is the
 * 12.4 MB memcpy. The rest is the extra readbacks.
 *
 * P216PackedEncoder does this for 16-bit 4:2:2, where it is easy: both of its
 * planes have the same row length, so the framestore is just one stacked on the
 * other and every row is the same width. 4:2:0 is not so tidy --
 *
 *   NV12:  Y is w bytes over h rows, interleaved CbCr is also w bytes
 *          (w/2 sites x 2 components) over h/2 rows.       Uniform rows.
 *   I420:  Y is w bytes over h rows, then Cb and Cr are w/2 bytes each over
 *          h/2 rows.                                       NOT uniform.
 *   YV12:  I420 with Cr before Cb.
 *
 * -- so instead of an RGBA8 target whose texels are groups of four bytes, this
 * renders an R8 target of w x (3h/2) whose every texel IS one framestore byte.
 * One readback of it is the framestore, in order, ready to send.
 *
 * R8 rather than RGBA8 buys two things. It needs no width divisible by 4 (an
 * RGBA8 target would be w/4 texels wide, and 722x576 and 1922x1080 are real NDI
 * sizes that are even but not multiples of four), and each fragment writes a
 * single UNORM byte through exactly the same path the plane encoders' own R8
 * targets use -- which is why the output can be byte-identical to theirs rather
 * than merely close.
 *
 * For I420 and YV12 the chroma region's rows do not line up with the target's:
 * a target row is w bytes and a chroma plane row is w/2, so each target row
 * carries two chroma rows, and the second half of the region is the other
 * plane. chroma_byte() below is the whole of that arithmetic.
 *
 * Chroma is one bilinear tap at the centre of the 2x2 source block: a box
 * filter, centre-sited. That is not inertia -- the two plausible improvements
 * were implemented and measured against a real NDI receiver, and both are
 * worse. The alternatives are still in chroma_at(), selectable through
 * SCORE_GFX_CHROMA_SITING, so the experiment can be re-run rather than taken
 * on trust.
 *
 * SITING. MPEG-2, H.264 and HEVC put a 4:2:0 chroma sample ON the even luma
 * column, not between the pair. Encoding to that convention and decoding with
 * the other shifts every chroma edge half a luma pixel. Measured by sending
 * colour bars through the SDK and finding the sub-pixel position of each
 * chroma edge on the way back:
 *
 *     centre siting   mean displacement  -0.001 luma columns
 *     left siting     mean displacement  +0.165 luma columns
 *
 * So NDI's receiver assumes CENTRE siting, and the broadcast convention would
 * be the wrong answer here. That is the same pattern as the colour matrix:
 * NDI follows what software encoders do, not what the standards say.
 *
 * PRE-FILTERING. A box keeps no guard band, so chroma finer than the chroma
 * grid aliases. Replacing it with [1 3 3 1]/8 in each direction band-limits
 * first. Measured on vertical red/blue stripes round-tripped through the SDK,
 * mean |RGB error| against the source, by stripe period in luma columns:
 *
 *     period       4      6      8     12     16     32
 *     box       5.48  33.06   8.46   5.23   3.15   2.67
 *     [1 3 3 1] 49.63  47.04  27.02  15.74  12.95   8.02
 *
 * The box is better everywhere, including at the Nyquist limit where the
 * filter should have won. The receiver upsamples chroma with something
 * box-like of its own and does not reconstruct a band-limited signal, so the
 * blur a pre-filter adds costs more than the aliasing it removes. (The box
 * does alias -- period 6 beats against the chroma grid and is its worst case
 * by far -- but pre-filtering makes even that worse.)
 *
 * Requires width % 2 == 0 and height % 2 == 0, which is what 4:2:0 requires
 * anyway.
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

    // Declared up here because chroma_at() below uses SITING, and GLSL wants
    // a declaration before its use.
    //
    // BPT: bytes carried by one texel of the target -- 4 for an RGBA8 target,
    // 1 for the R8 fallback. Both are constants, so the branches fold away.
    const int BPT = %3;
    const int SITING = %4;

    vec2 flip_y(vec2 tc) {
    // Only OpenGL. The rest of the engine puts its geometry through
    // renderer.clipSpaceCorrMatrix, which negates Y on Vulkan; this pass draws
    // a hardcoded triangle in raw NDC and does not, so the correction it needs
    // is not the same one.
    //
    // Note this flips the SOURCE lookup, never the target row order. The row a
    // fragment writes is taken from v_texcoord.y, which runs from 0 at the
    // first row of the readback on both backends -- on GL because v_texcoord.y
    // = 0 is the bottom of the target and GL reads back bottom-up, on Vulkan
    // because it is the top and Vulkan reads back top-down. That is what lets
    // the luma/chroma split sit anywhere in the target rather than only at the
    // midpoint.
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

    // (Cb, Cr) of chroma site (cx, cy) on the w/2 x h/2 grid.
    //
    // Vertically both sitings are the same: the sample sits on the boundary
    // between source rows 2*cy and 2*cy+1, so one bilinear tap there averages
    // the pair, which is what 4:2:0 asks for.
    //
    // Horizontally they differ, and SITING is the switch:
    //
    //   0 (centre) - one tap on the boundary between columns 2*cx and 2*cx+1.
    //       A 2x2 box. This is what swscale does and therefore what almost
    //       every software encoder emits.
    //
    //   1 (left)   - the sample belongs ON column 2*cx, which is what MPEG-2,
    //       H.264 and HEVC specify for 4:2:0 (chroma_sample_loc_type 0), with
    //       a [1 2 1]/4 filter across columns 2*cx-1, 2*cx, 2*cx+1. Two
    //       bilinear taps land it exactly: one on each of the two boundaries
    //       either side of column 2*cx, averaged. A box filter at the wrong
    //       position shifts every chroma edge half a luma pixel on decode.
    //
    // Which is right is not a matter of taste -- it is whatever the receiver
    // assumes when it upsamples, and that is measurable.
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
        // Centre siting, but a [1 3 3 1]/8 pre-filter in each direction
        // instead of a box -- a 4x4 kernel, separable, in four bilinear taps.
        // A weighted tap between two pixels is (1-t)*a + t*b, so a pair at
        // t = 0.75 gives 1:3 and a pair at t = 0.25 gives 3:1; averaging the
        // two pairs gives 1:3:3:1 over the four columns, centred on the same
        // boundary the box is centred on. The sample does not move; only the
        // amount of aliasing it folds in changes.
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
      float xb = float((cx << 1) + 1) / fs.x;         // right of column 2*cx
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

    // Four framestore bytes per texel where the width allows it, one where it
    // does not.
    //
    // One byte per texel is the simple form and works at any even width, but
    // it runs a fragment -- and therefore a texture fetch and a full colour
    // conversion -- for every byte of the framestore. Measured at 2160p that
    // made this encoder SLOWER than the plane-based one it replaces (12.1 ms
    // against 7.2), which defeats the point of it. Four bytes per texel gives
    // a quarter of the fragments for the same number of fetches.
    //
    // It needs width % 4 == 0, because the target is width/4 texels wide.
    // 720, 1280, 1920 and 3840 all qualify; 722 and 1922 -- even, so 4:2:0 can
    // express them -- do not, and fall back to R8 rather than lose the size.
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

    // Linear, not Nearest: the chroma taps rely on bilinear filtering to
    // average each 2x2 block, which is how the plane encoders downsample. The
    // luma taps sit on exact texel centres, where Linear returns the texel
    // unchanged, so the one sampler serves both.
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
