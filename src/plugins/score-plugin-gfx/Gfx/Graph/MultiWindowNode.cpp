#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/NodeProfiler.hpp>

#include <chrono>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Gfx/Graph/WarpImport.hpp>
#include <Gfx/Graph/RhiIndirectCompat.hpp>
#include <Gfx/Graph/PipelineStateHelpers.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/OpenGL.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QFileInfo>
#include <QFloat16>
#include <QGenericMatrix>
#include <QQuaternion>
#include <QGuiApplication>
#include <QScreen>
#include <QThreadPool>

namespace score::gfx
{

static std::array<float, 16>
subRectUBO(const QRectF& rect, const Gfx::CanvasLayout& canvas)
{
  std::array<float, 16> out{
      float(rect.x()), float(rect.y()), float(rect.width()), float(rect.height())};
  const QMatrix3x3 m = QQuaternion::fromEulerAngles(
                           canvas.orientation[0], canvas.orientation[1],
                           canvas.orientation[2])
                           .toRotationMatrix();
  for(int c = 0; c < 3; c++)
    for(int r = 0; r < 3; r++)
      out[4 + 4 * c + r] = m(r, c);
  out[7] = canvas.projection == Gfx::CanvasProjection::Equirectangular ? 1.f : 0.f;
  out[11] = canvas.rotates() ? 1.f : 0.f;
  return out;
}

// Compute 3x3 homography matrix that maps the warped quad corners to the unit square.
// Given 4 source points (warped corners) and 4 destination points (unit square corners),
// this finds the inverse mapping: for screen position p, H*p gives the texture coordinate.
// Uses the standard DLT (Direct Linear Transform) approach.
static std::array<float, 12> computeHomographyUBO(const Gfx::CornerWarp& warp)
{
  // Source corners (the warped quad in output UV space)
  double sx0 = warp.topLeft.x(),     sy0 = warp.topLeft.y();
  double sx1 = warp.topRight.x(),    sy1 = warp.topRight.y();
  double sx2 = warp.bottomRight.x(), sy2 = warp.bottomRight.y();
  double sx3 = warp.bottomLeft.x(),  sy3 = warp.bottomLeft.y();

  // Destination: unit square corners (TL=0,0 TR=1,0 BR=1,1 BL=0,1)
  // We want the mapping FROM warped corners TO unit square,
  // i.e. for a screen pixel at position (u,v), compute where in [0,1]² to sample.
  // This is the inverse of the mapping unit_square -> warped_quad.

  // First compute the forward mapping: unit square -> warped quad
  // Using the standard 4-point perspective transform formulas
  double dx1 = sx1 - sx2, dy1 = sy1 - sy2;
  double dx2 = sx3 - sx2, dy2 = sy3 - sy2;
  double dx3 = sx0 - sx1 + sx2 - sx3, dy3 = sy0 - sy1 + sy2 - sy3;

  double denom = dx1 * dy2 - dx2 * dy1;
  if(std::abs(denom) < 1e-12)
  {
    // Degenerate: return identity
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  }

  double g = (dx3 * dy2 - dx2 * dy3) / denom;
  double h = (dx1 * dy3 - dx3 * dy1) / denom;

  double a = sx1 - sx0 + g * sx1;
  double b = sx3 - sx0 + h * sx3;
  double c = sx0;
  double d = sy1 - sy0 + g * sy1;
  double e = sy3 - sy0 + h * sy3;
  double f = sy0;
  // g, h already computed, i = 1

  // Forward matrix maps (u,v) in [0,1]² to warped position:
  //   [a b c] [u]     [x*w]
  //   [d e f] [v]  =  [y*w]
  //   [g h 1] [1]     [w  ]
  //
  // We need the INVERSE: maps warped position back to [0,1]²

  // Compute adjugate (transpose of cofactors) of 3x3 matrix
  double A = e * 1.0 - f * h;
  double B = -(b * 1.0 - c * h);
  double C = b * f - c * e;
  double D = -(d * 1.0 - f * g);
  double E = a * 1.0 - c * g;
  double F = -(a * f - c * d);
  double G = d * h - e * g;
  double H = -(a * h - b * g);
  double I = a * e - b * d;

  // Normalize so that I = 1 (or close to it)
  if(std::abs(I) > 1e-12)
  {
    double invI = 1.0 / I;
    A *= invI; B *= invI; C *= invI;
    D *= invI; E *= invI; F *= invI;
    G *= invI; H *= invI; I = 1.0;
  }

  // Pack into std140 format: 3 columns of vec4 (xyz used, w padding)
  // Column-major: col0 = first column of matrix
  // Matrix rows: row0 = [A, B, C], row1 = [D, E, F], row2 = [G, H, I]
  // In the shader we do dot(row, p), so we store rows as columns for the dot products
  return {
    (float)A, (float)B, (float)C, 0.0f,  // homographyCol0 (row 0)
    (float)D, (float)E, (float)F, 0.0f,  // homographyCol1 (row 1)
    (float)G, (float)H, (float)I, 0.0f   // homographyCol2 (row 2)
  };
}

namespace
{
void zeroNonFinite(FloatImage& img) noexcept
{
  const int c = img.channels;
  for(std::size_t k = 0; k + c <= img.data.size(); k += c)
  {
    float* p = img.data.data() + k;
    if(!std::all_of(p, p + c, [](float v) { return std::isfinite(v); }))
      std::fill_n(p, c, 0.f);
  }
}

struct MapLoader
{
  int maxSize{};
  ossia::flat_map<QString, std::pair<std::optional<WarpImport>, QString>> archives;

  bool fits(const FloatImage& img, QString& err) const
  {
    if(!img.valid() || img.channels != 4)
    {
      if(err.isEmpty())
        err = QStringLiteral("not an RGBA image");
      return false;
    }
    if(img.width > maxSize || img.height > maxSize)
    {
      err = QStringLiteral("%1x%2 is above the %3 texture size limit")
                .arg(img.width)
                .arg(img.height)
                .arg(maxSize);
      return false;
    }
    return true;
  }

  std::shared_ptr<const FloatImage> finish(FloatImage img, QString& err) const
  {
    if(!fits(img, err))
      return {};
    zeroNonFinite(img);
    return std::make_shared<const FloatImage>(std::move(img));
  }

  std::shared_ptr<const FloatImage>
  map(const QString& spec, WarpMapKind kind, QString& err)
  {
    const QString path = mapArchivePath(spec);
    if(path.size() == spec.size() && !path.endsWith(QStringLiteral(".mpcdi"), Qt::CaseInsensitive))
      return finish(loadWarpMap(spec, kind, &err), err);

    auto it = archives.find(spec);
    if(it == archives.end())
    {
      QString e;
      auto w = loadMPCDIRegion(
          path, path.size() < spec.size() ? spec.mid(path.size() + 1) : QString{}, &e);
      it = archives.emplace(spec, std::make_pair(std::move(w), e)).first;
    }
    auto& [w, e] = it->second;
    if(!w)
    {
      err = e;
      return {};
    }
    if(kind == WarpMapKind::Map && w->threeD)
    {
      err = QStringLiteral("an Advanced 3D region holds world points, not a 2D UV warp");
      return {};
    }
    auto& img = kind == WarpMapKind::Map     ? w->map
                : kind == WarpMapKind::Alpha ? w->alpha
                                             : w->beta;
    return finish(std::move(img), err);
  }

  std::shared_ptr<const FloatImage> response(const QString& path, bool withMixing, QString& err)
  {
    const auto t = loadResponseTable(path, &err);
    if(!t.valid())
      return {};
    const auto& mx = t.mixing;
    const int K = withMixing && mx.valid() ? mx.K : 0;
    FloatImage img;
    img.width = K ? K * K : std::min(t.size, maxResponseSize);
    img.height = 1 + K;
    img.channels = 4;
    img.data.assign(std::size_t(img.width) * img.height * 4, 1.f);
    for(int i = 0; i < img.width; i++)
    {
      const double f = double(i) / (img.width - 1) * (t.size - 1);
      const int i0 = std::min(int(f), t.size - 2);
      const double a = f - i0;
      for(int c = 0; c < 3; c++)
        img.data[i * 4 + c]
            = float(t.inverse[i0 * 3 + c] * (1. - a) + t.inverse[(i0 + 1) * 3 + c] * a);
    }
    if(K)
      img.data[3] = float(mx.D);
    for(int b = 0; b < K; b++)
      for(int g = 0; g < K; g++)
        for(int r = 0; r < K; r++)
        {
          const float* src = mx.l.data() + ((std::size_t(b) * K + g) * K + r) * 3;
          float* dst = img.data.data() + ((std::size_t(1 + g) * img.width) + b * K + r) * 4;
          for(int c = 0; c < 3; c++)
            dst[c] = src[c];
        }
    return finish(std::move(img), err);
  }

  std::array<std::shared_ptr<const FloatImage>, 3>
  compensation(const QString& path, QString& err) const
  {
    auto m = loadCompensation(path, &err);
    if(!m.valid())
      return {};
    for(auto& row : m.rows)
    {
      if(!fits(row, err))
        return {};
      zeroNonFinite(row);
    }
    auto all = std::make_shared<const CompensationMaps>(std::move(m));
    return {
        std::shared_ptr<const FloatImage>(all, &all->rows[0]),
        std::shared_ptr<const FloatImage>(all, &all->rows[1]),
        std::shared_ptr<const FloatImage>(all, &all->rows[2])};
  }

  static std::shared_ptr<const CubeLut> lut(const QString& path, QString& err)
  {
    auto lut = loadCubeLut(path, &err);
    if(!lut.valid())
      return {};
    return std::make_shared<const CubeLut>(std::move(lut));
  }

  bool linear(const QString& spec, WarpMapKind kind) const
  {
    const QString path = mapArchivePath(spec);
    if(path.endsWith(QStringLiteral(".mpcdi"), Qt::CaseInsensitive))
    {
      if(kind == WarpMapKind::Alpha)
        return true;
      auto it = archives.find(spec);
      return it != archives.end() && it->second.first && it->second.first->betaLinear;
    }
    for(const char* ext : {".pfm", ".exr", ".data"})
      if(path.endsWith(QLatin1String(ext), Qt::CaseInsensitive))
        return true;
    return false;
  }

  static constexpr int maxResponseSize = 4096;
};

bool calibrationActive(const MultiWindowNode::WindowOutput& wo) noexcept
{
  return wo.alphaMap || wo.betaMap || wo.lut || wo.responseMap || wo.compensationMaps[0]
         || !wo.color.isIdentity();
}

std::array<const std::shared_ptr<const FloatImage>*, MultiWindowNode::MapSlotCount>
outputMaps(const MultiWindowNode::WindowOutput& wo) noexcept
{
  return {&wo.warpMap,           &wo.alphaMap,          &wo.betaMap,
          &wo.responseMap,       &wo.compensationMaps[0], &wo.compensationMaps[1],
          &wo.compensationMaps[2]};
}

constexpr int mapBindings[MultiWindowNode::MapSlotCount]{7, 8, 9, 13, 14, 15, 16};

std::array<float, 16> packOutputColorUBO(
    const Gfx::OutputColor& c, bool warpMap, bool alphaMap, bool betaMap,
    bool compensation, bool alphaLinear, bool betaLinear) noexcept
{
  return {
      c.gain[0],   c.gain[1],   c.gain[2],   c.gamma,
      c.offset[0], c.offset[1], c.offset[2], c.linearBlend ? 1.f : 0.f,
      warpMap ? 1.f : 0.f, alphaMap ? 1.f : 0.f, betaMap ? 1.f : 0.f,
      compensation ? 1.f : 0.f,
      alphaLinear ? 1.f : 0.f, betaLinear ? 1.f : 0.f, 0.f, 0.f};
}

bool inLinearLight(Gfx::BlendMapSpace space, bool linearMap) noexcept
{
  return space == Gfx::BlendMapSpace::Linear
         || (space == Gfx::BlendMapSpace::Auto && linearMap);
}

int canvasRank(QRhiTexture::Format f) noexcept
{
  switch(f)
  {
    case QRhiTexture::RGBA32F:
      return 2;
    case QRhiTexture::RGBA16F:
      return 1;
    default:
      return 0;
  }
}
}

// --- MultiWindowRenderer ---
// Renders the input texture's sub-regions to multiple swap chains.

class MultiWindowRenderer final : public score::gfx::OutputNodeRenderer
{
public:
  QShader m_vertexS, m_fragmentS, m_bakeFragmentS;
  score::gfx::MeshBuffers m_mesh{};

  struct PerWindowData
  {
    score::gfx::Pipeline pipeline;
    QRhiBuffer* uvRectUBO{};
    QRhiBuffer* blendUBO{};
    QRhiBuffer* warpUBO{};
    QRhiBuffer* colorUBO{};
    QRhiBuffer* qualityUBO{};
    std::array<QRhiTexture*, MultiWindowNode::MapSlotCount> mapTextures{};
    std::array<bool, MultiWindowNode::MapSlotCount> mapOk{};
    QRhiTexture* lutTexture{};
    bool lutOk{};
    std::array<int, 8> uploadedVersions{};
    int readbackServed{};
    std::array<score::gfx::Sampler, 1> samplers{};
    QRectF sourceRect{0, 0, 1, 1};

    QRhiTexture* bakedWarp{};
    QRhiTextureRenderTarget* bakeRT{};
    QRhiRenderPassDescriptor* bakeRP{};
    score::gfx::Pipeline bakePipeline;
    bool bakeFailed{};
    int bakedWarpVersion{-1};
    bool bakedBicubic{};
    bool bakedWrap{};
  };
  std::vector<PerWindowData> m_perWindow;

  QRhiSampler* m_mapSampler{};
  QRhiSampler* m_warpSampler{};
  QRhiSampler* m_lutSampler{};
  QRhiSampler* m_noiseSampler{};
  QRhiTexture* m_noiseTexture{};

  const MultiWindowNode& m_multiNode;
  bool m_initialized{false};
  uint32_t m_frame{};

  explicit MultiWindowRenderer(
      const MultiWindowNode& node, const score::gfx::RenderState& state)
      : score::gfx::OutputNodeRenderer{node}
      , m_multiNode{node}
  {
  }

  ~MultiWindowRenderer() override = default;

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    // The upstream graph renders into the node-owned offscreen target.
    // That target is always alive while the rhi is, independently of any
    // specific window.
    return m_multiNode.offscreenTarget();
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // Compile the sub-rect sampling shader
    // sourceRect is in canvas space (Y-down: y=0 top, y=1 bottom).
    // Following the ScaledRenderer pattern: on SPIRV flip v_texcoord.y,
    // on OpenGL flip sourceRect.y to convert from canvas Y-down to texture Y-up.
    static const constexpr auto frag_head = R"_(#version 450
      layout(location = 0) in vec2 v_texcoord;
      layout(location = 0) out vec4 fragColor;

      layout(binding = 3) uniform sampler2D tex;

      layout(std140, binding = 2) uniform SubRect {
          vec4 sourceRect; // x, y, width, height in canvas UV space
          vec4 canvasRot0; // columns of the canvas orientation, canvasRot0.w = equirectangular
          vec4 canvasRot1; // w = the orientation is not the identity
          vec4 canvasRot2;
      };

      layout(std140, binding = 4) uniform BlendParams {
          vec4 blendWidths; // left, right, top, bottom (in output UV 0-1)
          vec4 blendGammas; // left, right, top, bottom
      };

      layout(std140, binding = 5) uniform WarpParams {
          // mat3 stored as 3 x vec4 columns (std140 packing)
          vec4 homographyCol0;
          vec4 homographyCol1;
          vec4 homographyCol2;
          float warpEnabled;
          float rotationDeg;  // 0, 90, 180, 270
          float mirrorX;      // 0.0 or 1.0
          float mirrorY;      // 0.0 or 1.0
      };

      layout(std140, binding = 6) uniform ColorParams {
          vec4 colorGain;   // rgb gain, w = gamma
          vec4 colorOffset; // rgb offset, w = blend in linear light
          vec4 mapFlags;    // x = warp map, y = alpha map, z = beta map, w = compensation
          vec4 mapSpace;    // x = alpha map in linear light, y = beta map in linear light
      };

      layout(binding = 7) uniform sampler2D warpMap;
      layout(binding = 8) uniform sampler2D alphaMap;
      layout(binding = 9) uniform sampler2D betaMap;
      layout(binding = 10) uniform sampler3D lut;
      layout(binding = 11) uniform sampler2D blueNoise;
      layout(binding = 13) uniform sampler2D responseMap;
      layout(binding = 14) uniform sampler2D compMap0;
      layout(binding = 15) uniform sampler2D compMap1;
      layout(binding = 16) uniform sampler2D compMap2;
      layout(binding = 17) uniform sampler2D bakedWarp;

      layout(std140, binding = 12) uniform QualityParams {
          vec4 quality;  // x = sampling (0 linear, 1 trilinear, 2 anisotropic), y = max taps, z = bicubic warp, w = dither levels
          vec4 quality2; // x = transfer (0 gamma 2.2, 1 linear, 2 PQ), y = LUT on, z = frame, w = measured response
          vec4 lutMin;
          vec4 lutMax;
      };

)_";

    static const constexpr auto frag_tail = R"_(
      vec2 wrapCanvas(vec2 uv)
      {
          return canvasRot0.w > 0.5 ? vec2(fract(uv.x), uv.y) : uv;
      }

      vec3 canvasDirection(vec2 c)
      {
          const float pi = 3.14159265358979;
          float lon = (c.x - 0.5) * 2.0 * pi;
          float lat = (0.5 - c.y) * pi;
          return vec3(sin(lon) * cos(lat), sin(lat), -cos(lon) * cos(lat));
      }

      vec2 directionToCanvas(vec3 d)
      {
          const float pi = 3.14159265358979;
          return vec2(
              atan(d.x, -d.z) / (2.0 * pi) + 0.5,
              0.5 - asin(clamp(d.y / max(length(d), 1e-9), -1.0, 1.0)) / pi);
      }

      vec3 sampleSource(vec2 uv, vec2 dx, vec2 dy)
      {
          if(quality.x < 0.5)
              return textureLod(tex, wrapCanvas(uv), 0.0).rgb;
          vec2 sz = vec2(textureSize(tex, 0));
          vec2 px = dx * sz;
          vec2 py = dy * sz;
          float lx = length(px);
          float ly = length(py);
          float limit = 0.25 * max(sz.x, sz.y);
          if(lx > limit) { dx *= limit / lx; lx = limit; }
          if(ly > limit) { dy *= limit / ly; ly = limit; }
          if(quality.x < 1.5)
              return textureLod(tex, wrapCanvas(uv), max(log2(max(max(lx, ly), 1e-6)), 0.0)).rgb;
          float pmax = max(lx, ly);
          float pmin = max(min(lx, ly), 1e-6);
          float n = clamp(ceil(pmax / pmin), 1.0, quality.y);
          float lod = max(log2(max(pmax / n, 1e-6)), 0.0);
          vec2 axis = lx >= ly ? dx : dy;
          int taps = int(n);
          vec3 acc = vec3(0.0);
          for(int i = 0; i < 16; i++)
          {
              if(i >= taps)
                  break;
              acc += textureLod(tex, wrapCanvas(uv + axis * ((float(i) + 0.5) / n - 0.5)), lod).rgb;
          }
          return acc / n;
      }

      vec3 pqToLinear(vec3 e)
      {
          const float m1 = 0.1593017578125, m2 = 78.84375;
          const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
          vec3 p = pow(clamp(e, 0.0, 1.0), vec3(1.0 / m2));
          return pow(max(p - c1, 0.0) / (c2 - c3 * p), vec3(1.0 / m1));
      }

      vec3 linearToPq(vec3 y)
      {
          const float m1 = 0.1593017578125, m2 = 78.84375;
          const float c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
          vec3 p = pow(clamp(y, 0.0, 1.0), vec3(m1));
          return pow((c1 + c2 * p) / (1.0 + c3 * p), vec3(m2));
      }

      vec3 decodeLinear(vec3 c)
      {
          if(quality2.x > 1.5)
              return pqToLinear(c);
          if(quality2.x > 0.5)
              return c;
          return pow(max(c, vec3(0.0)), vec3(2.2));
      }

      vec3 encodeLinear(vec3 c)
      {
          if(quality2.w > 0.5)
          {
              float n = float(textureSize(responseMap, 0).x);
              vec3 t = clamp(c, 0.0, 1.0) * ((n - 1.0) / n) + 0.5 / n;
              float y = 0.5 / float(textureSize(responseMap, 0).y);
              return vec3(
                  textureLod(responseMap, vec2(t.r, y), 0.0).r,
                  textureLod(responseMap, vec2(t.g, y), 0.0).g,
                  textureLod(responseMap, vec2(t.b, y), 0.0).b);
          }
          if(quality2.x > 1.5)
              return linearToPq(c);
          if(quality2.x > 0.5)
              return c;
          return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
      }

      vec3 unmix(vec3 e)
      {
          vec2 sz = vec2(textureSize(responseMap, 0));
          float K = sz.y - 1.0;
          float D = texelFetch(responseMap, ivec2(0, 0), 0).a;
          float top = max(e.r, max(e.g, e.b));
          if(top > D)
              e *= D / top;
          vec3 g = sqrt(clamp(e / D, 0.0, 1.0)) * (K - 1.0);
          float z0 = min(floor(g.b), K - 2.0);
          float tz = g.b - z0;
          vec2 uv = vec2(g.r + 0.5, 1.5 + g.g);
          vec3 a = textureLod(responseMap, vec2(z0 * K + uv.x, uv.y) / sz, 0.0).rgb;
          vec3 b = textureLod(responseMap, vec2((z0 + 1.0) * K + uv.x, uv.y) / sz, 0.0).rgb;
          return mix(a, b, tz);
      }

      bool place(inout vec2 tc)
      {
          vec2 c = tc - 0.5;
          // Clockwise in screen space
          if(rotationDeg > 225.0)
              c = vec2(c.y, -c.x);
          else if(rotationDeg > 135.0)
              c = vec2(-c.x, -c.y);
          else if(rotationDeg > 45.0)
              c = vec2(-c.y, c.x);
          if(mirrorX > 0.5)
              c.x = -c.x;
          if(mirrorY > 0.5)
              c.y = -c.y;
          tc = c + 0.5;

          if(warpEnabled < 0.5)
              return true;
          // The homography is in Y-down space, as in the GUI.
          vec3 p = vec3(tc.x, 1.0 - tc.y, 1.0);
          vec3 warped = vec3(
              dot(vec3(homographyCol0.xyz), p),
              dot(vec3(homographyCol1.xyz), p),
              dot(vec3(homographyCol2.xyz), p));
          tc = warped.xy / warped.z;
          bool inside = tc.x >= 0.0 && tc.x <= 1.0 && tc.y >= 0.0 && tc.y <= 1.0;
          tc.y = 1.0 - tc.y;
          return inside;
      }

      void main()
      {
          vec2 screenUv = vec2(v_texcoord.x, 1.0 - v_texcoord.y);
          vec2 rampTc = v_texcoord;
          bool inside = place(rampTc);
          vec2 tc = rampTc;

          if(mapFlags.x > 0.5)
          {
              vec3 m = quality.z > 1.5 ? texelFetch(bakedWarp, ivec2(gl_FragCoord.xy), 0).xyz
                                       : sampleWarp(screenUv);
              tc = vec2(m.x, 1.0 - m.y);
              inside = place(tc) && m.z > 0.5;
          }

          vec2 uv = vec2(
              sourceRect.x + tc.x * sourceRect.z, sourceRect.y + (1.0 - tc.y) * sourceRect.w);
          if(canvasRot0.w > 0.5)
          {
              if(canvasRot1.w > 0.5)
              {
                  vec3 d = canvasDirection(uv);
                  d = canvasRot0.xyz * d.x + canvasRot1.xyz * d.y + canvasRot2.xyz * d.z;
                  uv = directionToCanvas(d);
              }
              else
                  uv.x = fract(uv.x);
          }
          vec2 uvDx = dFdx(uv);
          vec2 uvDy = dFdy(uv);
          if(canvasRot0.w > 0.5)
          {
              uvDx.x -= floor(uvDx.x + 0.5);
              uvDy.x -= floor(uvDy.x + 0.5);
          }
#if !defined(QSHADER_SPIRV)
          uv.y = 1.0 - uv.y;
          uvDx.y = -uvDx.y;
          uvDy.y = -uvDy.y;
#endif

          vec3 rgb = vec3(0.0);
          if(inside && uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
              rgb = sampleSource(uv, uvDx, uvDy);

          // Soft-edge blending
          // v_texcoord.y is Y-up on both GL and Vulkan (Qt RHI normalizes this),
          // so tc.y=0 is screen bottom and tc.y=1 is screen top.
          // pow() of a negative base is undefined in GLSL, and the drivers
          // that answer NaN turn it into a fully lit pixel on the way into a
          // UNORM attachment: that is the one-pixel bright fringe that used to
          // reappear along a blended border. The ramp coordinate leaves [0;1]
          // whenever the fragment centre of an edge pixel falls outside the
          // triangle, which multisampling makes routine, so it is clamped.
          float alpha = 1.0;
          if(blendWidths.x > 0.0 && rampTc.x < blendWidths.x)
              alpha *= pow(clamp(rampTc.x / blendWidths.x, 0.0, 1.0), blendGammas.x);
          if(blendWidths.y > 0.0 && rampTc.x > 1.0 - blendWidths.y)
              alpha *= pow(clamp((1.0 - rampTc.x) / blendWidths.y, 0.0, 1.0), blendGammas.y);
          if(blendWidths.z > 0.0 && rampTc.y > 1.0 - blendWidths.z)
              alpha *= pow(clamp((1.0 - rampTc.y) / blendWidths.z, 0.0, 1.0), blendGammas.z);
          if(blendWidths.w > 0.0 && rampTc.y < blendWidths.w)
              alpha *= pow(clamp(rampTc.y / blendWidths.w, 0.0, 1.0), blendGammas.w);

          float ramp = clamp(alpha, 0.0, 1.0);
          vec3 alphaRgb = vec3(1.0);
          if(mapFlags.y > 0.5)
              alphaRgb = clamp(texture(alphaMap, screenUv).rgb, 0.0, 1.0);
          vec3 outAlpha = alphaRgb * ramp;
          alpha = (outAlpha.r + outAlpha.g + outAlpha.b) / 3.0;

          bool alphaLinear = mapFlags.y > 0.5 && mapSpace.x > 0.5;
          bool betaLinear = mapFlags.z > 0.5 && mapSpace.y > 0.5;
          vec3 signalAlpha = vec3(1.0);
          if(mapFlags.y > 0.5 && !alphaLinear)
          {
              signalAlpha = alphaRgb;
              alphaRgb = vec3(1.0);
          }
          bool rampLinear = colorOffset.w > 0.5 || mapFlags.w > 0.5 || quality2.w > 0.5;
          bool linearLight = rampLinear || alphaLinear || betaLinear;
          if(rampLinear)
              alphaRgb *= ramp;
          else
              rgb *= ramp;
          if(linearLight)
              rgb = decodeLinear(rgb);
          if(mapFlags.w > 0.5)
          {
              vec4 m0 = texture(compMap0, screenUv);
              vec4 m1 = texture(compMap1, screenUv);
              vec4 m2 = texture(compMap2, screenUv);
              vec3 p = vec3(dot(m0.rgb, rgb) + m0.a, dot(m1.rgb, rgb) + m1.a, dot(m2.rgb, rgb) + m2.a);
              float lo = min(p.r, min(p.g, p.b));
              float mean = (p.r + p.g + p.b) / 3.0;
              if(lo < 0.0 && mean > 0.0)
                  p = mix(vec3(mean), p, mean / (mean - lo));
              if(quality2.w > 1.5)
                  p = unmix(p);
              float hi = max(p.r, max(p.g, p.b));
              if(hi > 1.0)
                  p /= hi;
              rgb = clamp(p, 0.0, 1.0);
          }
          rgb *= alphaRgb;
          if(betaLinear)
              rgb += texture(betaMap, screenUv).rgb;
          if(linearLight)
              rgb = encodeLinear(rgb);
          rgb *= signalAlpha;
          if(mapFlags.z > 0.5 && !betaLinear)
              rgb += texture(betaMap, screenUv).rgb;

          rgb = rgb * colorGain.rgb + colorOffset.rgb;
          if(colorGain.w != 1.0 && colorGain.w > 0.0)
              rgb = pow(max(rgb, vec3(0.0)), vec3(1.0 / colorGain.w));

          if(quality2.y > 0.5)
          {
              float n = float(textureSize(lut, 0).x);
              vec3 c = clamp((rgb - lutMin.xyz) / (lutMax.xyz - lutMin.xyz), 0.0, 1.0);
              rgb = textureLod(lut, c * ((n - 1.0) / n) + 0.5 / n, 0.0).rgb;
          }

          if(quality.w > 0.5)
          {
              float levels = quality.w;
              ivec2 p = ivec2(gl_FragCoord.xy);
              float r1 = fract(texelFetch(blueNoise, p & 63, 0).r + quality2.z * 0.618034);
              float r2 = fract(texelFetch(blueNoise, (p + ivec2(32, 21)) & 63, 0).r + quality2.z * 0.754878);
              vec3 v = clamp(rgb, 0.0, 1.0);
              vec3 lo = clamp(v * (2.0 * levels), 0.0, 1.0);
              vec3 hi = 1.0 - clamp((v - (levels - 0.5) / levels) * (2.0 * levels), 0.0, 1.0);
              vec3 noise = mix(vec3(r1 - 0.5), vec3(r1 + r2 - 1.0), min(lo, hi));
              rgb = v + noise / levels;
          }

          fragColor = vec4(rgb, alpha);
      }
      )_";

    static const constexpr auto warp_glsl = R"_(
      vec3 warpTexel(ivec2 p, ivec2 size)
      {
          vec4 m = texelFetch(warpMap, clamp(p, ivec2(0), size - 1), 0);
          return vec3(m.xy, m.w > 0.5 ? 1.0 : 0.0);
      }

      // On an equirectangular canvas u wraps: a tap is taken on the turn of ref.
      vec3 unwrapTexel(vec3 s, float ref)
      {
          if(canvasRot0.w > 0.5)
              s.x -= floor(s.x - ref + 0.5);
          return s;
      }

      vec3 wrapWarp(vec2 uv)
      {
          return vec3(canvasRot0.w > 0.5 ? fract(uv.x) : uv.x, uv.y, 1.0);
      }

      // xy = source UV, z = 1 when valid.
      vec3 sampleWarp(vec2 uv)
      {
          ivec2 size = textureSize(warpMap, 0);
          vec2 f = uv * vec2(size) - 0.5;
          vec2 fl = floor(f);
          vec2 t = f - fl;
          ivec2 b = ivec2(fl);
          vec3 s00 = warpTexel(b, size);
          if(mod(quality.z, 2.0) > 0.5)
          {
              vec2 w0 = t * (-0.5 + t * (1.0 - 0.5 * t));
              vec2 w1 = 1.0 + t * t * (-2.5 + 1.5 * t);
              vec2 w2 = t * (0.5 + t * (2.0 - 1.5 * t));
              vec2 w3 = t * t * (-0.5 + 0.5 * t);
              float wx[4] = float[4](w0.x, w1.x, w2.x, w3.x);
              float wy[4] = float[4](w0.y, w1.y, w2.y, w3.y);
              vec2 acc = vec2(0.0);
              float valid = 0.0;
              for(int j = 0; j < 4; j++)
                  for(int i = 0; i < 4; i++)
                  {
                      vec3 s = unwrapTexel(warpTexel(b + ivec2(i - 1, j - 1), size), s00.x);
                      acc += wx[i] * wy[j] * s.xy;
                      valid += s.z;
                  }
              if(valid > 15.5)
                  return wrapWarp(acc);
          }
          vec3 s10 = unwrapTexel(warpTexel(b + ivec2(1, 0), size), s00.x);
          vec3 s01 = unwrapTexel(warpTexel(b + ivec2(0, 1), size), s00.x);
          vec3 s11 = unwrapTexel(warpTexel(b + ivec2(1, 1), size), s00.x);
          float w00 = (1.0 - t.x) * (1.0 - t.y) * s00.z;
          float w10 = t.x * (1.0 - t.y) * s10.z;
          float w01 = (1.0 - t.x) * t.y * s01.z;
          float w11 = t.x * t.y * s11.z;
          float w = w00 + w10 + w01 + w11;
          if(w < 0.5)
              return vec3(0.0);
          return wrapWarp((w00 * s00.xy + w10 * s10.xy + w01 * s01.xy + w11 * s11.xy) / w);
      }
      )_";

    static const constexpr auto bake_head = R"_(#version 450
      layout(location = 0) in vec2 v_texcoord;
      layout(location = 0) out vec4 fragColor;

      layout(binding = 7) uniform sampler2D warpMap;

      layout(std140, binding = 2) uniform SubRect {
          vec4 sourceRect;
          vec4 canvasRot0;
          vec4 canvasRot1;
          vec4 canvasRot2;
      };

      layout(std140, binding = 12) uniform QualityParams {
          vec4 quality;
          vec4 quality2;
          vec4 lutMin;
          vec4 lutMax;
      };
      )_";

    static const constexpr auto bake_main = R"_(
      void main()
      {
          fragColor = vec4(sampleWarp(vec2(v_texcoord.x, 1.0 - v_texcoord.y)), 1.0);
      }
      )_";

    std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(
        renderer.state, mesh.defaultVertexShader(),
        QString::fromUtf8(frag_head) + warp_glsl + frag_tail);
    std::tie(std::ignore, m_bakeFragmentS) = score::gfx::makeShaders(
        renderer.state, mesh.defaultVertexShader(),
        QString::fromUtf8(bake_head) + warp_glsl + bake_main);

    auto& rhi = *renderer.state.rhi;
    m_mapSampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_mapSampler->setName("MultiWindowRenderer::mapSampler");
    m_mapSampler->create();
    m_warpSampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_warpSampler->setName("MultiWindowRenderer::warpSampler");
    m_warpSampler->create();
    m_lutSampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_lutSampler->setName("MultiWindowRenderer::lutSampler");
    m_lutSampler->create();
    m_noiseSampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::Repeat, QRhiSampler::Repeat);
    m_noiseSampler->setName("MultiWindowRenderer::noiseSampler");
    m_noiseSampler->create();
    m_noiseTexture
        = rhi.newTexture(QRhiTexture::R32F, QSize{blueNoiseSize, blueNoiseSize}, 1);
    m_noiseTexture->setName("MultiWindowRenderer::blueNoise");
    m_noiseTexture->create();
    {
      const auto& noise = blueNoise();
      QRhiTextureSubresourceUploadDescription sub{
          noise.data(), quint32(noise.size() * sizeof(float))};
      res.uploadTexture(m_noiseTexture, QRhiTextureUploadDescription{{0, 0, sub}});
    }

    const auto& outputs = m_multiNode.windowOutputs();
    m_perWindow.resize(outputs.size());

    for(int i = 0; i < (int)outputs.size(); ++i)
      buildWindowPipeline(i, renderer, &res);

    m_initialized = true;
  }

  static QSize mapSize(const std::shared_ptr<const FloatImage>& img) noexcept
  {
    return img ? QSize{img->width, img->height} : QSize{1, 1};
  }

  //! Creates `tex` at `sz`, or at 1x1 (and false) when the device refuses it.
  static bool createTexture(QRhiTexture& tex, QSize sz, int depth = 0)
  {
    tex.destroy();
    tex.setPixelSize(sz);
    if(depth > 0)
      tex.setDepth(depth);
    if(tex.create())
      return true;
    qWarning() << "MultiWindowNode: cannot create a" << sz << "texture"
               << tex.name();
    tex.setPixelSize({1, 1});
    if(depth > 0)
      tex.setDepth(1);
    tex.create();
    return false;
  }

  // Build (or rebuild) the per-window GPU resources for a single window.
  // Safe to call multiple times: an existing pipeline is released first.
  // `res` may be null; UBO contents are refreshed every frame inside
  // renderSubRegion() so we don't need to perform an initial upload here.
  void buildWindowPipeline(
      int index, score::gfx::RenderList& renderer, QRhiResourceUpdateBatch* res)
  {
    if(index < 0 || index >= (int)m_perWindow.size())
      return;

    auto& rhi = *renderer.state.rhi;
    const auto& outputs = m_multiNode.windowOutputs();
    if(index >= (int)outputs.size())
      return;

    const auto& wo = outputs[index];
    auto& pw = m_perWindow[index];

    // A window without an RPD has no swap chain yet; postpone build.
    if(!wo.renderPassDescriptor)
      return;
    // The offscreen target must exist — the sampler binds to its texture.
    if(!m_multiNode.offscreenTarget().texture)
      return;

    // Release any existing pipeline/resources for this window before
    // rebuilding so the method is idempotent on re-expose.
    releaseWindowPipeline(index);

    pw.sourceRect = wo.sourceRect;

    // Create UBO for source rect
    pw.uvRectUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16 * sizeof(float));
    pw.uvRectUBO->setName(
        QByteArray("MultiWindowRenderer::uvRectUBO_") + QByteArray::number(index));
    pw.uvRectUBO->create();

    // Create UBO for blend parameters (4 widths + 4 gammas = 8 floats)
    pw.blendUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 8 * sizeof(float));
    pw.blendUBO->setName(
        QByteArray("MultiWindowRenderer::blendUBO_") + QByteArray::number(index));
    pw.blendUBO->create();

    // Create UBO for corner warp (3 vec4 columns + 4 floats trailing)
    pw.warpUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16 * sizeof(float));
    pw.warpUBO->setName(
        QByteArray("MultiWindowRenderer::warpUBO_") + QByteArray::number(index));
    pw.warpUBO->create();

    pw.colorUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16 * sizeof(float));
    pw.colorUBO->setName(
        QByteArray("MultiWindowRenderer::colorUBO_") + QByteArray::number(index));
    pw.colorUBO->create();

    {
      const auto maps = outputMaps(wo);
      for(int m = 0; m < MultiWindowNode::MapSlotCount; m++)
      {
        auto tex = rhi.newTexture(
            m == MultiWindowNode::WarpSlot ? QRhiTexture::RGBA32F : QRhiTexture::RGBA16F,
            QSize{1, 1}, 1);
        tex->setName(
            QByteArray("MultiWindowRenderer::map_") + QByteArray::number(index) + '_'
            + QByteArray::number(m));
        pw.mapOk[m] = createTexture(*tex, mapSize(*maps[m])) && *maps[m];
        pw.mapTextures[m] = tex;
      }
    }

    pw.qualityUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16 * sizeof(float));
    pw.qualityUBO->setName(
        QByteArray("MultiWindowRenderer::qualityUBO_") + QByteArray::number(index));
    pw.qualityUBO->create();

    {
      const int n = wo.lut ? wo.lut->size : 1;
      pw.lutTexture
          = rhi.newTexture(QRhiTexture::RGBA16F, 1, 1, 1, 1, QRhiTexture::ThreeDimensional);
      pw.lutTexture->setName("MultiWindowRenderer::lut");
      pw.lutOk = createTexture(*pw.lutTexture, {n, n}, n) && wo.lut;
    }
    pw.uploadedVersions.fill(-1);

    pw.bakedWarp = rhi.newTexture(QRhiTexture::RGBA32F, QSize{1, 1}, 1, QRhiTexture::RenderTarget);
    pw.bakedWarp->setName(QByteArray("MultiWindowRenderer::bakedWarp_") + QByteArray::number(index));
    pw.bakedWarp->create();
    pw.bakedWarpVersion = -1;

    // Seed initial UBO contents if we were given a batch to piggy-back on.
    // When `res` is null (lazy build from initWindowSwapChain) we skip the
    // initial upload; renderSubRegion() re-uploads every frame anyway.
    if(res)
    {
      const auto rectData = subRectUBO(pw.sourceRect, m_multiNode.canvasLayout());
      res->updateDynamicBuffer(pw.uvRectUBO, 0, sizeof(rectData), rectData.data());

      float blendData[8] = {
          wo.blendLeft.width,  wo.blendRight.width, wo.blendTop.width,
          wo.blendBottom.width, wo.blendLeft.gamma, wo.blendRight.gamma,
          wo.blendTop.gamma,   wo.blendBottom.gamma};
      res->updateDynamicBuffer(pw.blendUBO, 0, sizeof(blendData), blendData);

      float warpEnabled = wo.cornerWarp.isIdentity() ? 0.0f : 1.0f;
      auto hom = computeHomographyUBO(wo.cornerWarp);
      float warpData[16] = {};
      std::copy(hom.begin(), hom.end(), warpData);
      warpData[12] = warpEnabled;
      warpData[13] = (float)wo.rotation;
      warpData[14] = wo.mirrorX ? 1.0f : 0.0f;
      warpData[15] = wo.mirrorY ? 1.0f : 0.0f;
      res->updateDynamicBuffer(pw.warpUBO, 0, sizeof(warpData), warpData);
    }

    // Create sampler reading from the node's offscreen target.
    {
      const bool mips = m_multiNode.offscreenTarget().texture->flags().testFlag(
          QRhiTexture::MipMapped);
      const bool equirect = m_multiNode.canvasLayout().projection
                            == Gfx::CanvasProjection::Equirectangular;
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear,
          mips ? QRhiSampler::Linear : QRhiSampler::None,
          equirect ? QRhiSampler::Repeat : QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->setName("MultiWindowRenderer::sampler");
      sampler->create();

      pw.samplers[0] = {sampler, m_multiNode.offscreenTarget().texture};
    }

    // Build the blit pipeline against this window's render pass descriptor.
    {
      score::gfx::TextureRenderTarget rt;
      rt.renderPass = wo.renderPassDescriptor;

      using B = QRhiShaderResourceBinding;
      constexpr auto F = B::FragmentStage;
      std::vector<B> extraBindings{
          B::uniformBuffer(4, F, pw.blendUBO),
          B::uniformBuffer(5, F, pw.warpUBO),
          B::uniformBuffer(6, F, pw.colorUBO),
          B::sampledTexture(10, F, pw.lutTexture, m_lutSampler),
          B::sampledTexture(11, F, m_noiseTexture, m_noiseSampler),
          B::uniformBuffer(12, F, pw.qualityUBO),
          B::sampledTexture(17, F, pw.bakedWarp, m_warpSampler)};
      for(int m = 0; m < MultiWindowNode::MapSlotCount; m++)
        extraBindings.push_back(B::sampledTexture(
            mapBindings[m], F, pw.mapTextures[m],
            m == MultiWindowNode::WarpSlot ? m_warpSampler : m_mapSampler));

      const auto& mesh = renderer.defaultTriangle();
      pw.pipeline = score::gfx::buildPipeline(
          renderer, mesh, m_vertexS, m_fragmentS, rt, nullptr, pw.uvRectUBO,
          pw.samplers, score::gfx::premultipliedOverBlend(),
          {extraBindings.data(), extraBindings.size()});
    }
  }

  void releaseWindowPipeline(int index)
  {
    if(index < 0 || index >= (int)m_perWindow.size())
      return;

    auto& pw = m_perWindow[index];
    pw.pipeline.release();
    pw.bakePipeline.release();
    delete pw.bakeRT;
    pw.bakeRT = nullptr;
    delete pw.bakeRP;
    pw.bakeRP = nullptr;
    delete pw.bakedWarp;
    pw.bakedWarp = nullptr;
    pw.bakedWarpVersion = -1;
    pw.bakeFailed = false;
    for(auto& s : pw.samplers)
      delete s.sampler;
    pw.samplers = {};
    delete pw.uvRectUBO;
    pw.uvRectUBO = nullptr;
    delete pw.blendUBO;
    pw.blendUBO = nullptr;
    delete pw.warpUBO;
    pw.warpUBO = nullptr;
    delete pw.colorUBO;
    pw.colorUBO = nullptr;
    delete pw.qualityUBO;
    pw.qualityUBO = nullptr;
    delete pw.lutTexture;
    pw.lutTexture = nullptr;
    for(auto& t : pw.mapTextures)
    {
      delete t;
      t = nullptr;
    }
    pw.uploadedVersions.fill(-1);
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    for(int i = 0; i < (int)m_perWindow.size(); ++i)
      releaseWindowPipeline(i);
    m_perWindow.clear();
    for(auto* s : {m_mapSampler, m_warpSampler, m_lutSampler, m_noiseSampler})
      delete s;
    m_mapSampler = m_warpSampler = m_lutSampler = m_noiseSampler = nullptr;
    delete m_noiseTexture;
    m_noiseTexture = nullptr;
    m_initialized = false;
    // m_offscreenTarget is owned by MultiWindowNode; do NOT release it here.
  }

  // Sub-region 0 is no longer special — it's blitted in its own per-window
  // frame just like all other windows. finishFrame is a no-op so that
  // rl->render(cb) can be invoked inside an offscreen frame that is not
  // attached to any swap chain.
  void finishFrame(
      score::gfx::RenderList& /*renderer*/, QRhiCommandBuffer& /*cb*/,
      QRhiResourceUpdateBatch*& /*res*/) override
  {
  }

  // Called by MultiWindowNode::render() for each live window.
  void renderToWindow(
      int windowIndex, score::gfx::RenderList& renderer, QRhiCommandBuffer& cb)
  {
    if(windowIndex < 0 || windowIndex >= (int)m_perWindow.size())
      return;

    // Don't pre-allocate a batch here: renderSubRegion has early-return
    // paths before any consumer (beginPass), and pre-allocating leaks
    // one pool slot per discarded window on every render. The three
    // UBO blocks inside renderSubRegion lazily allocate via
    // `if(!res) res = ...->nextResourceUpdateBatch()`, and beginPass
    // accepts a null batch — so passing nullptr here is safe.
    QRhiResourceUpdateBatch* res = nullptr;
    renderSubRegion(windowIndex, renderer, cb, res);
  }

private:
  void uploadMaps(
      const MultiWindowNode::WindowOutput& wo, PerWindowData& pw,
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch*& res)
  {
    const auto maps = outputMaps(wo);
    for(int m = 0; m < MultiWindowNode::MapSlotCount; m++)
    {
      if(pw.uploadedVersions[m] == wo.mapVersions[m])
        continue;
      pw.uploadedVersions[m] = wo.mapVersions[m];
      const auto& img = *maps[m];
      auto& tex = *pw.mapTextures[m];
      const QSize sz = mapSize(img);
      bool ok = true;
      if(tex.pixelSize() != sz)
        ok = createTexture(tex, sz);
      pw.mapOk[m] = ok && img;

      const bool half = tex.format() == QRhiTexture::RGBA16F;
      QByteArray bytes;
      if(pw.mapOk[m])
      {
        const auto n = qsizetype(img->data.size());
        if(half)
        {
          bytes.resize(n * qsizetype(sizeof(qfloat16)));
          qFloatToFloat16(
              reinterpret_cast<qfloat16*>(bytes.data()), img->data.data(), n);
        }
        else
        {
          bytes = QByteArray(
              reinterpret_cast<const char*>(img->data.data()),
              n * qsizetype(sizeof(float)));
        }
      }
      else
      {
        bytes = QByteArray(4 * (half ? sizeof(qfloat16) : sizeof(float)), 0);
      }
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();
      res->uploadTexture(
          &tex, QRhiTextureUploadDescription{
                    {0, 0, QRhiTextureSubresourceUploadDescription{bytes}}});
    }

    if(pw.uploadedVersions[MultiWindowNode::LutSlot] == wo.mapVersions[MultiWindowNode::LutSlot])
      return;
    pw.uploadedVersions[MultiWindowNode::LutSlot] = wo.mapVersions[MultiWindowNode::LutSlot];
    const int want = wo.lut ? wo.lut->size : 1;
    bool ok = true;
    if(pw.lutTexture->depth() != want || pw.lutTexture->pixelSize() != QSize{want, want})
      ok = createTexture(*pw.lutTexture, {want, want}, want);
    pw.lutOk = ok && wo.lut;
    const int n = pw.lutTexture->depth();
    const auto N = std::size_t(n) * n * n;
    std::vector<qfloat16> half(N * 4, qfloat16(0.f));
    if(pw.lutOk)
      for(std::size_t i = 0; i < N; i++)
      {
        for(int c = 0; c < 3; c++)
          half[i * 4 + c] = qfloat16(wo.lut->rgb[i * 3 + c]);
        half[i * 4 + 3] = qfloat16(1.f);
      }
    QList<QRhiTextureUploadEntry> entries;
    const quint32 slice = quint32(n) * n * 4 * sizeof(qfloat16);
    for(int z = 0; z < n; z++)
      entries.append(QRhiTextureUploadEntry{
          z, 0,
          QRhiTextureSubresourceUploadDescription{QByteArray(
              reinterpret_cast<const char*>(half.data()) + std::size_t(z) * slice,
              slice)}});
    QRhiTextureUploadDescription desc;
    desc.setEntries(entries.begin(), entries.end());
    if(!res)
      res = renderer.state.rhi->nextResourceUpdateBatch();
    res->uploadTexture(pw.lutTexture, desc);
  }

  static void releaseBakeTarget(PerWindowData& pw)
  {
    pw.bakePipeline.release();
    delete pw.bakeRT;
    pw.bakeRT = nullptr;
    delete pw.bakeRP;
    pw.bakeRP = nullptr;
    pw.bakedWarpVersion = -1;
    if(pw.bakedWarp && pw.bakedWarp->pixelSize() != QSize{1, 1})
    {
      pw.bakedWarp->destroy();
      pw.bakedWarp->setPixelSize({1, 1});
      pw.bakedWarp->create();
    }
  }

  bool ensureBakeTarget(PerWindowData& pw, score::gfx::RenderList& renderer, QSize sz)
  {
    if(pw.bakeFailed)
      return false;
    if(pw.bakeRT && pw.bakedWarp->pixelSize() == sz)
      return true;
    pw.bakedWarpVersion = -1;
    pw.bakedWarp->destroy();
    pw.bakedWarp->setPixelSize(sz);
    bool ok = pw.bakedWarp->create();
    if(ok && !pw.bakeRT)
    {
      auto& rhi = *renderer.state.rhi;
      pw.bakeRT = rhi.newTextureRenderTarget({pw.bakedWarp});
      pw.bakeRT->setName("MultiWindowRenderer::bakeRT");
      pw.bakeRP = pw.bakeRT->newCompatibleRenderPassDescriptor();
      pw.bakeRT->setRenderPassDescriptor(pw.bakeRP);
      ok = pw.bakeRT->create();
      if(ok)
      {
        score::gfx::TextureRenderTarget rt;
        rt.texture = pw.bakedWarp;
        rt.renderTarget = pw.bakeRT;
        rt.renderPass = pw.bakeRP;

        using B = QRhiShaderResourceBinding;
        constexpr auto F = B::FragmentStage;
        std::vector<B> extraBindings{
            B::sampledTexture(
                7, F, pw.mapTextures[MultiWindowNode::WarpSlot], m_warpSampler),
            B::uniformBuffer(12, F, pw.qualityUBO)};
        pw.bakePipeline = score::gfx::buildPipeline(
            renderer, renderer.defaultTriangle(), m_vertexS, m_bakeFragmentS, rt, nullptr,
            pw.uvRectUBO, {}, QRhiGraphicsPipeline::TargetBlend{},
            {extraBindings.data(), extraBindings.size()});
        ok = pw.bakePipeline.pipeline != nullptr;
      }
    }
    else if(ok)
    {
      ok = pw.bakeRT->create();
    }
    if(ok)
      return true;
    qWarning() << "MultiWindowNode: cannot create a" << sz << "warp bake target";
    pw.bakeFailed = true;
    releaseBakeTarget(pw);
    return false;
  }

  void renderSubRegion(
      int index, score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res)
  {
    const auto& outputs = m_multiNode.windowOutputs();
    if(index >= (int)outputs.size())
      return;

    const auto& wo = outputs[index];
    auto& pw = m_perWindow[index];

    if(!wo.swapChain || !wo.hasSwapChain)
      return;

    // Skip windows whose pipeline hasn't been built yet (e.g. window
    // became ready after the initial renderer init and we haven't had a
    // chance to lazily build it).
    if(!pw.pipeline.pipeline || !pw.pipeline.srb)
      return;

    auto rt = wo.swapChain->currentFrameRenderTarget();
    if(!rt)
      return;

    uploadMaps(wo, pw, renderer, res);

    const QSize pixelSize = wo.swapChain->currentPixelSize();
    const bool bicubic = wo.quality.warpInterpolation == Gfx::WarpInterpolation::Bicubic;
    static const bool noBake = qEnvironmentVariableIntValue("SCORE_MW_NO_WARP_BAKE") != 0;
    const bool equirect
        = m_multiNode.canvasLayout().projection == Gfx::CanvasProjection::Equirectangular;
    const bool warpLookup = pw.mapOk[MultiWindowNode::WarpSlot];
    const bool useBaked = warpLookup && !noBake && ensureBakeTarget(pw, renderer, pixelSize);
    if(!warpLookup && pw.bakeRT)
      releaseBakeTarget(pw);
    const bool needBake = useBaked
                          && (pw.bakedWarpVersion != wo.mapVersions[MultiWindowNode::WarpSlot]
                              || pw.bakedBicubic != bicubic || pw.bakedWrap != equirect);
    if(auto* canvasSampler = pw.samplers[0].sampler)
    {
      const auto u = equirect ? QRhiSampler::Repeat : QRhiSampler::ClampToEdge;
      if(canvasSampler->addressU() != u)
      {
        canvasSampler->setAddressU(u);
        canvasSampler->create();
      }
    }

    // Update UBOs from live data (may have changed via device parameters)
    if(pw.uvRectUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();

      const auto rectData = subRectUBO(wo.sourceRect, m_multiNode.canvasLayout());
      res->updateDynamicBuffer(pw.uvRectUBO, 0, sizeof(rectData), rectData.data());
    }

    if(pw.blendUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();

      float blendData[8] = {
          wo.blendLeft.width, wo.blendRight.width, wo.blendTop.width, wo.blendBottom.width,
          wo.blendLeft.gamma, wo.blendRight.gamma, wo.blendTop.gamma, wo.blendBottom.gamma};
      res->updateDynamicBuffer(pw.blendUBO, 0, sizeof(blendData), blendData);
    }

    if(pw.warpUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();

      float warpEnabled = wo.cornerWarp.isIdentity() ? 0.0f : 1.0f;
      auto hom = computeHomographyUBO(wo.cornerWarp);
      float warpData[16] = {};
      std::copy(hom.begin(), hom.end(), warpData);
      warpData[12] = warpEnabled;
      warpData[13] = (float)wo.rotation;
      warpData[14] = wo.mirrorX ? 1.0f : 0.0f;
      warpData[15] = wo.mirrorY ? 1.0f : 0.0f;
      res->updateDynamicBuffer(pw.warpUBO, 0, sizeof(warpData), warpData);
    }

    if(pw.colorUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();
      using S = MultiWindowNode::MapSlot;
      const auto colorData = packOutputColorUBO(
          wo.color, pw.mapOk[S::WarpSlot], pw.mapOk[S::AlphaSlot], pw.mapOk[S::BetaSlot],
          pw.mapOk[S::Comp0Slot] && pw.mapOk[S::Comp1Slot] && pw.mapOk[S::Comp2Slot],
          inLinearLight(wo.blendSpace, wo.alphaLinear),
          inLinearLight(wo.blendSpace, wo.betaLinear));
      res->updateDynamicBuffer(
          pw.colorUBO, 0, sizeof(float) * colorData.size(), colorData.data());
    }

    if(pw.qualityUBO)
    {
      if(!res)
        res = renderer.state.rhi->nextResourceUpdateBatch();
      const auto q = m_multiNode.effectiveQuality(wo);
      const bool canvasMips = m_multiNode.offscreenTarget().texture
                              && m_multiNode.offscreenTarget().texture->flags().testFlag(
                                  QRhiTexture::MipMapped);
      const int sampling = canvasMips ? q.sampling : 0;
      float qualityData[16] = {
          float(sampling),
          float(std::clamp(wo.quality.maxAnisotropy, 1, 16)),
          (bicubic ? 1.f : 0.f) + (useBaked ? 2.f : 0.f),
          float(q.ditherLevels),
          float(q.transfer),
          pw.lutOk ? 1.f : 0.f,
          float(m_frame % 4096u),
          pw.mapOk[MultiWindowNode::ResponseSlot]
              ? (wo.responseMap->height > 1 ? 2.f : 1.f)
              : 0.f,
          wo.lut ? wo.lut->domainMin[0] : 0.f,
          wo.lut ? wo.lut->domainMin[1] : 0.f,
          wo.lut ? wo.lut->domainMin[2] : 0.f,
          0.f,
          wo.lut ? wo.lut->domainMax[0] : 1.f,
          wo.lut ? wo.lut->domainMax[1] : 1.f,
          wo.lut ? wo.lut->domainMax[2] : 1.f,
          0.f};
      res->updateDynamicBuffer(pw.qualityUBO, 0, sizeof(qualityData), qualityData);
      m_frame++;
    }

    if(needBake)
    {
      cb.beginPass(pw.bakeRT, Qt::black, {1.0f, 0}, res);
      res = nullptr;
      cb.setGraphicsPipeline(pw.bakePipeline.pipeline);
      cb.setShaderResources(pw.bakePipeline.srb);
      cb.setViewport(QRhiViewport(0, 0, pixelSize.width(), pixelSize.height()));
      renderer.defaultTriangle().draw(m_mesh, cb);
      cb.endPass();
      pw.bakedWarpVersion = wo.mapVersions[MultiWindowNode::WarpSlot];
      pw.bakedBicubic = bicubic;
      pw.bakedWrap = equirect;
    }

    cb.beginPass(rt, Qt::black, {0.0f, 0}, res);
    res = nullptr;
    {
      auto sz = pixelSize;
      cb.setGraphicsPipeline(pw.pipeline.pipeline);
      cb.setShaderResources(pw.pipeline.srb);
      cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

      const auto& mesh = renderer.defaultTriangle();
      mesh.draw(m_mesh, cb);
    }
    if(pw.readbackServed != wo.readbackRequests)
    {
      pw.readbackServed = wo.readbackRequests;
      auto* rb = renderer.state.rhi->nextResourceUpdateBatch();
      rb->readBackTexture(QRhiReadbackDescription{}, wo.readback.get());
      cb.endPass(rb);
    }
    else
    {
      cb.endPass();
    }
  }
};

// --- MultiWindowNode implementation ---

MultiWindowNode::MultiWindowNode(
    Configuration conf, const std::vector<Gfx::OutputMapping>& mappings,
    QSize canvasSize)
    : OutputNode{}
    , m_conf{conf}
    , m_mappings{mappings}
    , m_canvasSize{canvasSize}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

MultiWindowNode::~MultiWindowNode()
{
  // Delegate to destroyOutput() to ensure a single, consistent cleanup path.
  // destroyOutput() is idempotent so calling it here after Graph::~Graph
  // already called it is safe.
  destroyOutput();
}

bool MultiWindowNode::canRender() const
{
  // With offscreen-first rendering we don't need any window to be alive:
  // the upstream graph renders into the offscreen target regardless.
  // Per-window blitting skips dead windows individually.
  return m_renderState && m_renderState->rhi
         && m_offscreenTarget.renderPass != nullptr;
}

void MultiWindowNode::setRenderSize(QSize sz)
{
  if(sz.width() < 1 || sz.height() < 1)
    return;
  m_canvasSize = sz;
  if(!m_renderState)
    return;
  if(sz == m_renderState->renderSize)
    return;

  m_renderState->renderSize = sz;

  // Recreate the offscreen target BEFORE poking the render list: when
  // m_onResize takes the synchronous rebuild path (recreateOutputRenderList,
  // used when the surgical resize fast-path can't apply), the new render
  // list copies the offscreen RT/RPD pointers and compiles pipelines against
  // them — so they must already be the new ones, or the fresh render list
  // holds dangling pointers as soon as we release the old target here.
  // The old target's objects go through QRhi's deferred-release queue
  // (deleteLater), so pipelines from the fast path that still reference
  // them are rebuilt by the rt_changed pass before anything dereferences
  // a destroyed handle.
  recreateOffscreenTarget();

  if(m_onResize)
    m_onResize();
}

void MultiWindowNode::setSourceRect(int windowIndex, QRectF rect)
{
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
    m_mappings[windowIndex].sourceRect = rect;
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
    m_windowOutputs[windowIndex].sourceRect = rect;
}

void MultiWindowNode::setEdgeBlend(int windowIndex, int side, float width, float gamma)
{
  if(side < 0 || side > 3)
    return;
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
  {
    *m_mappings[windowIndex].blendSides()[side] = {width, gamma};
  }
  if(windowIndex < 0 || windowIndex >= (int)m_windowOutputs.size())
    return;

  auto& wo = m_windowOutputs[windowIndex];
  EdgeBlendData* sides[4]{&wo.blendLeft, &wo.blendRight, &wo.blendTop, &wo.blendBottom};
  *sides[side] = {width, gamma};
  updateCanvasSpec();
}

void MultiWindowNode::setCornerWarp(int windowIndex, const Gfx::CornerWarp& warp)
{
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
    m_mappings[windowIndex].cornerWarp = warp;
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
  {
    m_windowOutputs[windowIndex].cornerWarp = warp;
    updateCanvasSpec();
  }
}

void MultiWindowNode::setTransform(int windowIndex, int rotation, bool mirrorX, bool mirrorY)
{
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
  {
    auto& m = m_mappings[windowIndex];
    m.rotation = rotation;
    m.mirrorX = mirrorX;
    m.mirrorY = mirrorY;
  }
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
  {
    auto& wo = m_windowOutputs[windowIndex];
    wo.rotation = rotation;
    wo.mirrorX = mirrorX;
    wo.mirrorY = mirrorY;
  }
}

void MultiWindowNode::requestReadback(int windowIndex)
{
  if(!m_readbackSupport)
  {
    qWarning("MultiWindowNode: readback requested without setReadbackSupport(true)");
    return;
  }
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
  {
    auto& wo = m_windowOutputs[windowIndex];
    wo.readback->data.clear();
    wo.readback->pixelSize = {};
    wo.readbackRequests++;
  }
}

void MultiWindowNode::setColor(int windowIndex, const Gfx::OutputColor& color)
{
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
    m_mappings[windowIndex].color = color;
  if(windowIndex >= 0 && windowIndex < (int)m_windowOutputs.size())
  {
    m_windowOutputs[windowIndex].color = color;
    updateCanvasSpec();
  }
}

namespace
{
std::array<float, 4> warpStep(const FloatImage& img)
{
  std::array<float, 4> out{};
  if(!img.valid() || img.channels < 4)
    return out;
  const int W = img.width, H = img.height, C = img.channels;
  constexpr double maxSamples = 1 << 18;
  const int stride = std::max(1, int(std::ceil(std::sqrt(double(W) * H / maxSamples))));
  const std::size_t expected
      = std::size_t((W + stride - 1) / stride) * std::size_t((H + stride - 1) / stride);
  std::array<std::vector<float>, 4> steps;
  for(auto& v : steps)
    v.reserve(expected);
  auto add = [&](const float* a, const float* b, int axis) {
    if(a[3] > 0.5f && b[3] > 0.5f)
      for(int k = 0; k < 2; k++)
        if(const float d = std::abs(a[k] - b[k]); std::isfinite(d))
          steps[axis * 2 + k].push_back(d);
  };
  for(int y = 0; y < H; y += stride)
  {
    const float* r = img.row(y);
    const float* n = y + 1 < H ? img.row(y + 1) : nullptr;
    for(int x = 0; x < W; x += stride)
    {
      if(x + 1 < W)
        add(r + x * C, r + (x + 1) * C, 0);
      if(n)
        add(r + x * C, n + x * C, 1);
    }
  }
  for(int k = 0; k < 4; k++)
  {
    auto& v = steps[k];
    if(v.empty())
      continue;
    auto it = v.begin() + std::ptrdiff_t(0.995 * double(v.size() - 1));
    std::nth_element(v.begin(), it, v.end());
    out[k] = *it;
  }
  return out;
}

template <typename T>
bool loaded(const T& v) noexcept
{
  return bool(v);
}
bool loaded(const std::array<std::shared_ptr<const FloatImage>, 3>& v) noexcept
{
  return bool(v[0]);
}

void warnStretched(const char* name, const QString& path, const FloatImage* img, QSize output)
{
  if(!img || output.isEmpty() || QSize{img->width, img->height} == output)
    return;
  qWarning().noquote() << QStringLiteral(
                              "MultiWindowNode: the %1 %2 is %3x%4 and is stretched to the "
                              "%5x%6 output")
                              .arg(QLatin1String(name), path)
                              .arg(img->width)
                              .arg(img->height)
                              .arg(output.width())
                              .arg(output.height());
}

void loadInto(
    MultiWindowNode::LoadedMaps& wo, const Gfx::OutputMaps& maps, int maxSize, QSize output)
{
  using M = MultiWindowNode;
  MapLoader loader;
  loader.maxSize = maxSize;

  enum Field
  {
    Warp,
    Alpha,
    Beta,
    Lut,
    Response,
    Compensation
  };
  const QString* paths[6]{&maps.warp,     &maps.alpha,    &maps.beta,
                          &maps.lut,      &maps.response, &maps.compensation};
  static const char* const names[6]{"warp map", "alpha map", "beta map",
                                    "LUT",      "response",  "compensation"};
  std::array<QString, 6> stamps;
  for(int k = 0; k < 6; k++)
    stamps[k] = mapFileStamp(*paths[k]);

  auto update = [&](int k, auto& field, auto&& load, bool force = false) -> bool {
    using T = std::decay_t<decltype(field)>;
    if(!force && stamps[k] == wo.mapStamps[k])
      return false;
    const QString& path = *paths[k];
    if(path.isEmpty())
    {
      wo.mapStamps[k].clear();
      const bool had = loaded(field);
      field = T{};
      return had;
    }
    QString err;
    T v = load(path, err);
    if(loaded(v))
    {
      field = std::move(v);
      wo.mapStamps[k] = stamps[k];
      return true;
    }
    qWarning() << "MultiWindowNode: cannot load the" << names[k] << path << err;
    if(wo.mapStamps[k].startsWith(path + QLatin1Char('|')))
      return false;
    wo.mapStamps[k].clear();
    const bool had = loaded(field);
    field = T{};
    return had;
  };

  auto image = [&](WarpMapKind kind) {
    return [&loader, kind](const QString& p, QString& e) { return loader.map(p, kind, e); };
  };
  if(update(Warp, wo.warpMap, image(WarpMapKind::Map)))
  {
    wo.changed[M::WarpSlot] = true;
    wo.warpStep = wo.warpMap ? warpStep(*wo.warpMap) : std::array<float, 4>{};
  }
  if(update(Alpha, wo.alphaMap, image(WarpMapKind::Alpha)))
  {
    wo.changed[M::AlphaSlot] = true;
    wo.alphaLinear = wo.alphaMap && loader.linear(maps.alpha, WarpMapKind::Alpha);
    warnStretched(names[Alpha], maps.alpha, wo.alphaMap.get(), output);
  }
  if(update(Beta, wo.betaMap, image(WarpMapKind::Beta)))
  {
    wo.changed[M::BetaSlot] = true;
    wo.betaLinear = wo.betaMap && loader.linear(maps.beta, WarpMapKind::Beta);
    warnStretched(names[Beta], maps.beta, wo.betaMap.get(), output);
  }
  if(update(Lut, wo.lut, [](const QString& p, QString& e) { return MapLoader::lut(p, e); }))
    wo.changed[M::LutSlot] = true;
  if(update(Compensation, wo.compensationMaps, [&](const QString& p, QString& e) {
       return loader.compensation(p, e);
     }))
  {
    wo.changed[M::Comp0Slot] = wo.changed[M::Comp1Slot] = wo.changed[M::Comp2Slot] = true;
    warnStretched(
        names[Compensation], maps.compensation, wo.compensationMaps[0].get(), output);
  }
  const bool withMixing = bool(wo.compensationMaps[0]);
  const bool mixingChanged = withMixing != wo.responseWithMixing;
  wo.responseWithMixing = withMixing;
  if(update(
         Response, wo.responseMap,
         [&](const QString& p, QString& e) { return loader.response(p, withMixing, e); },
         mixingChanged))
    wo.changed[M::ResponseSlot] = true;
}

MultiWindowNode::LoadedMaps loadedMaps(const MultiWindowNode::WindowOutput& wo)
{
  MultiWindowNode::LoadedMaps m;
  m.warpMap = wo.warpMap;
  m.alphaMap = wo.alphaMap;
  m.betaMap = wo.betaMap;
  m.responseMap = wo.responseMap;
  m.lut = wo.lut;
  m.compensationMaps = wo.compensationMaps;
  m.warpStep = wo.warpStep;
  m.mapStamps = wo.mapStamps;
  m.responseWithMixing = wo.responseWithMixing;
  m.alphaLinear = wo.alphaLinear;
  m.betaLinear = wo.betaLinear;
  return m;
}

void assignMaps(MultiWindowNode::WindowOutput& wo, MultiWindowNode::LoadedMaps&& m)
{
  wo.warpMap = std::move(m.warpMap);
  wo.alphaMap = std::move(m.alphaMap);
  wo.betaMap = std::move(m.betaMap);
  wo.responseMap = std::move(m.responseMap);
  wo.lut = std::move(m.lut);
  wo.compensationMaps = std::move(m.compensationMaps);
  wo.warpStep = m.warpStep;
  wo.mapStamps = std::move(m.mapStamps);
  wo.responseWithMixing = m.responseWithMixing;
  wo.alphaLinear = m.alphaLinear;
  wo.betaLinear = m.betaLinear;
  for(std::size_t k = 0; k < m.changed.size(); k++)
    if(m.changed[k])
      wo.mapVersions[k]++;
}
}

int MultiWindowNode::mapSizeLimit() const noexcept
{
  return m_renderState && m_renderState->rhi
             ? m_renderState->rhi->resourceLimit(QRhi::TextureSizeMax)
             : 16384;
}

QSize MultiWindowNode::outputPixelSize(int windowIndex) const noexcept
{
  const auto& wo = m_windowOutputs[windowIndex];
  if(wo.swapChain && !wo.swapChain->surfacePixelSize().isEmpty())
    return wo.swapChain->surfacePixelSize();
  if(windowIndex < (int)m_mappings.size())
    return m_mappings[windowIndex].windowSize;
  return {};
}

void MultiWindowNode::loadMaps(int windowIndex, const Gfx::OutputMaps& maps)
{
  auto& wo = m_windowOutputs[windowIndex];
  wo.mapRequest = ++m_mapSerial;
  auto m = loadedMaps(wo);
  loadInto(m, maps, mapSizeLimit(), outputPixelSize(windowIndex));
  assignMaps(wo, std::move(m));
}

void MultiWindowNode::requestMaps(int windowIndex, const Gfx::OutputMaps& maps)
{
  auto& wo = m_windowOutputs[windowIndex];
  const auto request = ++m_mapSerial;
  wo.mapRequest = request;
  m_mapsLoading++;
  QThreadPool::globalInstance()->start(
      [alive = std::weak_ptr<int>(m_alive), state = loadedMaps(wo), maps,
       maxSize = mapSizeLimit(), output = outputPixelSize(windowIndex), windowIndex,
       request, this]() mutable {
    loadInto(state, maps, maxSize, output);
    QMetaObject::invokeMethod(
        qApp,
        [alive, state = std::move(state), windowIndex, request, this]() mutable {
      if(!alive.lock())
        return;
      m_mapsLoading--;
      if(windowIndex >= (int)m_windowOutputs.size())
        return;
      auto& wo = m_windowOutputs[windowIndex];
      if(wo.mapRequest != request)
        return;
      assignMaps(wo, std::move(state));
      updateCanvasSpec();
        },
        Qt::QueuedConnection);
  });
}

void MultiWindowNode::setMaps(int windowIndex, const Gfx::OutputMaps& maps)
{
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
    m_mappings[windowIndex].maps = maps;
  if(windowIndex < 0 || windowIndex >= (int)m_windowOutputs.size())
    return;
  m_windowOutputs[windowIndex].blendSpace = maps.blendSpace;
  requestMaps(windowIndex, maps);
}

void MultiWindowNode::setQuality(int windowIndex, const Gfx::OutputQuality& quality)
{
  if(windowIndex >= 0 && windowIndex < (int)m_mappings.size())
    m_mappings[windowIndex].quality = quality;
  if(windowIndex < 0 || windowIndex >= (int)m_windowOutputs.size())
    return;
  m_windowOutputs[windowIndex].quality = quality;
  updateCanvasSpec();
}

bool MultiWindowNode::warpMinifies(const WindowOutput& wo) const noexcept
{
  if(!wo.warpMap)
    return false;
  QSize out{wo.warpMap->width, wo.warpMap->height};
  if(wo.swapChain && !wo.swapChain->surfacePixelSize().isEmpty())
    out = wo.swapChain->surfacePixelSize();
  const double perPixelX = double(wo.warpMap->width) / std::max(1, out.width());
  const double perPixelY = double(wo.warpMap->height) / std::max(1, out.height());
  const double sx = wo.sourceRect.width() * m_canvasSize.width();
  const double sy = wo.sourceRect.height() * m_canvasSize.height();
  const double alongX = std::hypot(wo.warpStep[0] * sx, wo.warpStep[1] * sy) * perPixelX;
  const double alongY = std::hypot(wo.warpStep[2] * sx, wo.warpStep[3] * sy) * perPixelY;
  return std::max(alongX, alongY) > 1.5;
}

MultiWindowNode::EffectiveQuality
MultiWindowNode::effectiveQuality(const WindowOutput& wo) const noexcept
{
  EffectiveQuality q;
  switch(wo.quality.sampling)
  {
    case Gfx::OutputSampling::Auto:
      q.sampling = wo.warpMap && warpMinifies(wo) ? 2 : 0;
      break;
    case Gfx::OutputSampling::Linear:
      q.sampling = 0;
      break;
    case Gfx::OutputSampling::Mipmapped:
      q.sampling = 1;
      break;
    case Gfx::OutputSampling::Anisotropic:
      q.sampling = 2;
      break;
  }

  using F = Gfx::SwapchainFormat;
  const bool srgbFlag = m_swapchainFlag == Gfx::SwapchainFlag::sRGB;
  int levels = 0;
  if(m_swapchainFormat == F::SDR)
    levels = 255;
  else if(m_swapchainFormat == F::HDR10)
    levels = 1023;
  switch(wo.quality.dither)
  {
    case Gfx::OutputDither::Auto:
      q.ditherLevels = (calibrationActive(wo) && !srgbFlag) ? levels : 0;
      break;
    case Gfx::OutputDither::Off:
      q.ditherLevels = 0;
      break;
    case Gfx::OutputDither::On:
      q.ditherLevels = levels;
      break;
  }

  switch(m_swapchainFormat)
  {
    case F::SDR:
      q.transfer = 0;
      break;
    case F::HDR10:
      q.transfer = 2;
      break;
    default:
      q.transfer = 1;
      break;
  }
  return q;
}

std::pair<QRhiTexture::Format, bool> MultiWindowNode::canvasSpec() const noexcept
{
  bool precise = false;
  bool mips = false;
  for(const auto& wo : m_windowOutputs)
  {
    precise = precise || calibrationActive(wo) || wo.warpMap;
    mips = mips || effectiveQuality(wo).sampling > 0;
  }
  const bool hdr = m_swapchainFormat != Gfx::SwapchainFormat::SDR;
  return {hdr || precise ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8, mips};
}

std::pair<QRhiTexture::Format, bool> MultiWindowNode::upgradedCanvas() const noexcept
{
  auto [fmt, mips] = canvasSpec();
  if(m_renderState && canvasRank(m_renderState->renderFormat) >= canvasRank(fmt))
    fmt = m_renderState->renderFormat;
  return {fmt, mips};
}

void MultiWindowNode::updateCanvasSpec(bool allowMipDrop)
{
  if(!m_renderState || m_windowOutputs.empty() || !m_windowOutputs[0].window)
    return;
  m_canvasMipDrop = m_canvasMipDrop || allowMipDrop;
  if(m_canvasUpdateQueued)
    return;
  const auto needed = [this] {
    const auto [fmt, mips] = upgradedCanvas();
    return fmt != m_renderState->renderFormat || (mips && !m_canvasMips)
           || (m_canvasMipDrop && !mips && m_canvasMips);
  };
  if(!needed())
  {
    m_canvasMipDrop = false;
    return;
  }
  m_canvasUpdateQueued = true;
  QMetaObject::invokeMethod(
      m_windowOutputs[0].window.get(),
      [this, needed] {
    m_canvasUpdateQueued = false;
    if(!m_renderState)
      return;
    const bool rebuild = needed();
    m_canvasMipDrop = false;
    if(!rebuild)
      return;
    const auto [fmt, mips] = upgradedCanvas();
    releaseOwnedRenderList();
    m_renderState->renderFormat = fmt;
    m_canvasMips = mips;
    recreateOffscreenTarget();
    if(m_onResize)
      m_onResize();
      },
      Qt::QueuedConnection);
}

void MultiWindowNode::setSwapchainFlag(Gfx::SwapchainFlag flag)
{
  if(m_swapchainFlag == flag)
    return;
  m_swapchainFlag = flag;
  // Live flag change requires per-window swapchain recreation. Mirrors
  // ScreenNode::setSwapchainFlag — destroyOutput tears down all windows;
  // the Graph reconciler rebuilds them on next cycle picking up the new
  // flag at the swapchain create site.
  //
  // The Graph-owned RenderList holds QRhiResources belonging to the QRhi
  // destroyOutput() is about to `delete`; release it first.
  releaseOwnedRenderList();
  destroyOutput();
}

void MultiWindowNode::setSwapchainFormat(Gfx::SwapchainFormat format)
{
  if(m_swapchainFormat == format)
    return;
  m_swapchainFormat = format;
  // Same rebuild rationale — without it the field updates but the live
  // swapchains keep their prior format.
  releaseOwnedRenderList();
  destroyOutput();
}

void MultiWindowNode::startRendering()
{
  if(onFps)
    onFps(0.f);
}

void MultiWindowNode::renderBlack()
{
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto rhi = m_renderState->rhi;

  for(auto& wo : m_windowOutputs)
  {
    if(!wo.window || !wo.swapChain || !wo.hasSwapChain)
      continue;

    // A window that is not exposed has no valid native drawable, and
    // presenting to it is a hard crash rather than a no-op: on macOS
    // QRhiGles2::endFrame -> QCocoaGLContext::swapBuffers -> CGLFlushDrawable
    // dereferences null inside AppleMetalOpenGLRenderer
    // (gldUpdateReadFramebuffer, EXC_BAD_ACCESS at 0xb8). An empty surface is
    // unsafe for the same reason. This is the rule the single-window path
    // follows: ScreenNode and Window both gate on isExposed() throughout.
    if(!wo.window->isExposed() || wo.swapChain->surfacePixelSize().isEmpty())
      continue;

    if(wo.swapChain->currentPixelSize() != wo.swapChain->surfacePixelSize())
      wo.hasSwapChain = wo.swapChain->createOrResize();

    if(!wo.hasSwapChain)
      continue;

    QRhi::FrameOpResult r = rhi->beginFrame(wo.swapChain);
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(!wo.hasSwapChain)
        continue;
      r = rhi->beginFrame(wo.swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
    {
      if(checkDeviceLost(r, *rhi, "MultiWindowNode"))
        return;
      continue;
    }

    auto cb = wo.swapChain->currentFrameCommandBuffer();
    auto batch = rhi->nextResourceUpdateBatch();
    cb->beginPass(wo.swapChain->currentFrameRenderTarget(), Qt::black, {0.0f, 0}, batch);
    cb->endPass();

    if(checkDeviceLost(rhi->endFrame(wo.swapChain), *rhi, "MultiWindowNode"))
      return;
  }
}

void MultiWindowNode::render()
{
  if(!m_renderState || !m_renderState->rhi || m_deviceLost)
    return;

  auto rhi = m_renderState->rhi;

  // Handle swap chain resizes for all live windows. Dead (closed) windows
  // have wo.swapChain == nullptr and are skipped naturally.
  for(auto& wo : m_windowOutputs)
  {
    if(!wo.window || !wo.swapChain)
      continue;
    if(wo.swapChain->currentPixelSize() != wo.swapChain->surfacePixelSize())
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      updateCanvasSpec(true);
    }
  }

  auto rl = m_renderer.lock();
  if(!rl || rl->renderers.size() <= 1)
  {
    // No active render graph — just clear every live window to black.
    renderBlack();
    return;
  }

  const auto windowLive = [this](int i) {
    auto& wo = m_windowOutputs[i];
    return wo.window && wo.swapChain && wo.hasSwapChain && wo.window->isExposed()
           && !wo.swapChain->surfacePixelSize().isEmpty();
  };

#if defined(QRHI_HAS_FRAME_SLOT_GROUPS)
  static const bool noSlotGroup = qEnvironmentVariableIntValue("SCORE_MW_NO_SLOT_GROUP") != 0;
  int lastLive = -1;
  if(!this->renderedNodes.empty() && !noSlotGroup)
    for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
      if(windowLive(i))
        lastLive = i;
  const bool slotGroup = lastLive >= 0;
  const QRhi::BeginFrameFlags windowBeginFlags
      = slotGroup ? QRhi::ContinueFrameSlot : QRhi::BeginFrameFlags{};
  const auto windowEndFlags = [&](int i) {
    return slotGroup && i != lastLive ? QRhi::KeepFrameSlot : QRhi::EndFrameFlags{};
  };
  const QRhi::EndFrameFlags offscreenEndFlags
      = slotGroup ? QRhi::KeepFrameSlot : QRhi::EndFrameFlags{};
  struct SlotCloser
  {
    QRhi& rhi;
    bool open{};
    ~SlotCloser()
    {
      if(!open)
        return;
      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb, QRhi::ContinueFrameSlot) == QRhi::FrameOpSuccess)
        rhi.endOffscreenFrame();
    }
  } slot{*rhi};
  const auto slotEnded = [&](QRhi::EndFrameFlags f) {
    slot.open = f.testFlag(QRhi::KeepFrameSlot);
  };
  const auto slotLost = [&] { slot.open = false; };
#else
  const QRhi::BeginFrameFlags windowBeginFlags{};
  const auto windowEndFlags = [](int) { return QRhi::EndFrameFlags{}; };
  const QRhi::EndFrameFlags offscreenEndFlags{};
  const auto slotEnded = [](QRhi::EndFrameFlags) { };
  const auto slotLost = [] { };
#endif

  // Step 1: render the upstream graph into the offscreen target, in a
  // frame that is not attached to any swap chain. This is what decouples
  // upstream rendering from any specific window's lifetime.
  {
    score::gfx::OffscreenFrame frame{*rhi};
    if(!frame)
    {
      checkDeviceLost(frame, *rhi, "MultiWindowNode");
      return;
    }

    rl->render(frame.commands());

    if(m_canvasMips && m_offscreenTarget.texture)
    {
      auto* mipBatch = rhi->nextResourceUpdateBatch();
      mipBatch->generateMips(m_offscreenTarget.texture);
      frame.commands().resourceUpdate(mipBatch);
    }

    frame.end(offscreenEndFlags);
    if(checkDeviceLost(frame, *rhi, "MultiWindowNode"))
    {
      slotLost();
      return;
    }
    slotEnded(offscreenEndFlags);
  }

  // Step 2: for each live window, blit its sub-region in its own frame.
  // Any window whose swap chain is gone or out-of-date is skipped without
  // affecting the others.
  if(this->renderedNodes.empty())
    return;
  auto outRenderer
      = dynamic_cast<MultiWindowRenderer*>(this->renderedNodes.begin()->second);
  if(!outRenderer)
    return;

  auto* profiler = rl->nodeProfiler();
  if(profiler && !profiler->enabled())
    profiler = nullptr;
  int64_t stageCpuNs = 0;
  double stageGpuS = 0.;
  bool stageGpu = false;

  for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    if(!windowLive(i))
      continue;

    // A window that is not exposed has no valid native drawable, and
    // presenting to it is a hard crash rather than a no-op: on macOS
    // QRhiGles2::endFrame -> QCocoaGLContext::swapBuffers -> CGLFlushDrawable
    // dereferences null inside AppleMetalOpenGLRenderer
    // (gldUpdateReadFramebuffer, EXC_BAD_ACCESS at 0xb8). An empty surface is
    // unsafe for the same reason. This is the rule the single-window path
    // follows: ScreenNode and Window both gate on isExposed() throughout.
    QRhi::FrameOpResult r = rhi->beginFrame(wo.swapChain, windowBeginFlags);
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(!wo.hasSwapChain)
        continue;
      r = rhi->beginFrame(wo.swapChain, windowBeginFlags);
    }
    if(r != QRhi::FrameOpSuccess)
    {
      if(checkDeviceLost(r, *rhi, "MultiWindowNode"))
      {
        slotLost();
        return;
      }
      continue;
    }

    auto cb = wo.swapChain->currentFrameCommandBuffer();
    if(profiler)
    {
      if(const double sec = cb->lastCompletedGpuTime(); sec > 0.)
      {
        stageGpuS += sec;
        stageGpu = true;
      }
    }
    {
      SwapChainFrameGuard guard{*rhi, *wo.swapChain};
      const auto t0 = std::chrono::steady_clock::now();
      outRenderer->renderToWindow(i, *rl, *cb);
      stageCpuNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
      guard.release();
    }

    if(checkDeviceLost(rhi->endFrame(wo.swapChain, windowEndFlags(i)), *rhi, "MultiWindowNode"))
    {
      slotLost();
      return;
    }
    slotEnded(windowEndFlags(i));
  }

  if(profiler)
  {
    score::gfx::FrameTiming f;
    f.list = this;
    f.nodesOnly = true;
    score::gfx::NodeTiming t;
    t.node = this->nodeId;
    t.type = "Output stage";
    t.cpu_ns = stageCpuNs;
    t.gpu_ns = stageGpu ? int64_t(stageGpuS * 1e9) : -1;
    f.nodes.push_back(t);
    profiler->submit(std::move(f));
  }
}

void MultiWindowNode::onRendererChange()
{
}

void MultiWindowNode::stopRendering()
{
  if(onFps)
    onFps(0.f);
}

void MultiWindowNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_renderer = r;
}

RenderList* MultiWindowNode::renderer() const
{
  return m_renderer.lock().get();
}

void MultiWindowNode::recreateOffscreenTarget()
{
  if(!m_renderState || !m_renderState->rhi)
    return;

  // Tear down any previous offscreen target, including its RPD. Any
  // pipelines built against this RPD must have been released already;
  // the callers of this method are responsible for that (createOutput,
  // destroyOutput, and the renderSize-change path which triggers a full
  // render-list rebuild).
  m_offscreenTarget.release();

  QSize sz = m_renderState->renderSize;
  if(sz.width() <= 0 || sz.height() <= 0)
    sz = QSize{1280, 720};

  m_offscreenTarget = score::gfx::createRenderTarget(
      *m_renderState, m_renderState->renderFormat, sz, m_renderState->samples,
      /*depth*/ true, false,
      m_canvasMips ? (QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips)
                   : QRhiTexture::Flags{});

  // Publish the RPD so that any code path that reads
  // state.renderPassDescriptor (ScaledRenderer-style renderers) sees a
  // stable, window-independent value. Not strictly required by
  // MultiWindowRenderer (which uses per-window RPDs for its blit
  // pipelines) but it keeps the contract of RenderState consistent with
  // other output nodes.
  m_renderState->renderPassDescriptor = m_offscreenTarget.renderPass;
}

void MultiWindowNode::initWindowSwapChain(int index)
{
  if(index < 0 || index >= (int)m_windowOutputs.size())
    return;
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto& wo = m_windowOutputs[index];
  if(!wo.window)
    return;

  // Release any existing swap chain for this window first. This makes
  // initWindowSwapChain idempotent so it can be called again when a
  // closed window is re-exposed.
  releaseWindowSwapChain(index);

  auto rhi = m_renderState->rhi;

  wo.swapChain = rhi->newSwapChain();
  wo.swapChain->setWindow(wo.window.get());
#if QT_VERSION > QT_VERSION_CHECK(6, 4, 0)
  wo.swapChain->setFormat((QRhiSwapChain::Format)m_swapchainFormat);
#endif

  wo.depthStencil = rhi->newRenderBuffer(
      QRhiRenderBuffer::DepthStencil, QSize(), m_renderState->samples,
      QRhiRenderBuffer::UsedWithSwapChainOnly | depthStencilRenderBufferFlags(*rhi));
  wo.swapChain->setDepthStencil(wo.depthStencil);
  wo.swapChain->setSampleCount(m_renderState->samples);

  QRhiSwapChain::Flags flags = QRhiSwapChain::MinimalBufferCount;
  if(m_readbackSupport)
    flags |= QRhiSwapChain::UsedAsTransferSource;
  // With multi-window we drive rendering from the node's timer and don't
  // rely on swap-chain vsync. Keep NoVSync on every window so no single
  // swap chain blocks the others.
  flags |= QRhiSwapChain::NoVSync;
  if(m_swapchainFlag == Gfx::SwapchainFlag::sRGB)
    flags |= QRhiSwapChain::sRGB;
  wo.swapChain->setFlags(flags);

  wo.renderPassDescriptor = wo.swapChain->newCompatibleRenderPassDescriptor();
  if(!wo.renderPassDescriptor)
  {
    // QVkSwapChain returns null when the surface cannot present (e.g. RADV
    // on Xvfb, without DRI3), and createOrResize() dereferences it. This
    // window gets no swap chain; the others keep running.
    qCritical(
        "MultiWindowNode: window %d cannot present on this display (no "
        "swap chain): nothing will be rendered to it",
        index);
    releaseWindowSwapChain(index);
    return;
  }
  wo.swapChain->setRenderPassDescriptor(wo.renderPassDescriptor);


  wo.hasSwapChain = wo.swapChain->createOrResize();
  if(wo.hasSwapChain)
  {
    if(m_renderState->outputSize.isEmpty())
      m_renderState->outputSize = wo.swapChain->currentPixelSize();
    updateCanvasSpec(true);
  }

  // If a renderer already exists, lazily build the per-window pipeline
  // for this newly-ready window so the next frame can blit to it.
  if(auto rl = m_renderer.lock())
  {
    if(auto it = this->renderedNodes.find(rl.get());
       it != this->renderedNodes.end())
    {
      if(auto* mwr = dynamic_cast<MultiWindowRenderer*>(it->second))
      {
        mwr->buildWindowPipeline(index, *rl, nullptr);
      }
    }
  }
}

void MultiWindowNode::releaseWindowSwapChain(int index)
{
  if(index < 0 || index >= (int)m_windowOutputs.size())
    return;
  if(!m_renderState || !m_renderState->rhi)
    return;

  auto& wo = m_windowOutputs[index];
  if(!wo.swapChain && !wo.depthStencil && !wo.renderPassDescriptor)
    return;

  // Release the renderer's per-window GPU state first, so its pipeline
  // (built against wo.renderPassDescriptor) is gone before we delete the
  // RPD itself.
  if(auto rl = m_renderer.lock())
  {
    if(auto it = this->renderedNodes.find(rl.get());
       it != this->renderedNodes.end())
    {
      if(auto* mwr = dynamic_cast<MultiWindowRenderer*>(it->second))
      {
        mwr->releaseWindowPipeline(index);
      }
    }
  }

  // Order matters: clear hasSwapChain BEFORE releasing wo.swapChain so a
  // queued expose / resize event landing in the middle of teardown can
  // never observe (hasSwapChain == true && swapChain dangling).
  wo.hasSwapChain = false;

  // Use deleteLater() instead of a synchronous rhi->finish() + delete.
  // rhi->finish() issues vkQueueWaitIdle which drains ALL in-flight work on
  // the graphics queue — stalling every other window. deleteLater() defers
  // native-object destruction to the next endFrame() when the relevant frame
  // slot is known safe, with no cross-window stall.
  auto* sc = wo.swapChain;
  wo.swapChain = nullptr;
  auto* ds = wo.depthStencil;
  wo.depthStencil = nullptr;
  auto* rpd = wo.renderPassDescriptor;
  wo.renderPassDescriptor = nullptr;

  if(sc)
    sc->deleteLater();
  if(ds)
    ds->deleteLater();
  if(rpd)
    rpd->deleteLater();
}

void MultiWindowNode::createOutput(score::gfx::OutputConfiguration conf)
{
  resetDeviceLost();
  m_onReleaseRenderList = conf.onReleaseRenderList;

  if(m_mappings.empty())
    return;

  // Create shared QRhi without a specific window. The rhi will be used
  // by every window's swap chain as well as by the offscreen target.
  m_renderState
      = score::gfx::createRenderState(conf.graphicsApi, m_canvasSize, nullptr);
  if(!m_renderState || !m_renderState->rhi)
    return;

  m_renderState->outputSize = m_renderState->renderSize;

  // Stash onResize so per-window events (re-expose, close+reopen) can
  // request a full render-list rebuild when they need one.
  m_onResize = conf.onResize;

  // 1. Create the offscreen target that is the stable render target
  //    for the upstream graph. This must exist BEFORE the render list is
  //    built because the renderer queries offscreenTarget() in its init.
  m_windowOutputs.resize(m_mappings.size());
  for(int i = 0; i < (int)m_mappings.size(); ++i)
  {
    // Parameter callbacks mutate these afterwards and write through to
    // m_mappings; initWindowSwapChain() never touches them.
    auto& wo = m_windowOutputs[i];
    const auto& mapping = m_mappings[i];
    wo.sourceRect = mapping.sourceRect;
    wo.blendLeft = {mapping.blendLeft.width, mapping.blendLeft.gamma};
    wo.blendRight = {mapping.blendRight.width, mapping.blendRight.gamma};
    wo.blendTop = {mapping.blendTop.width, mapping.blendTop.gamma};
    wo.blendBottom = {mapping.blendBottom.width, mapping.blendBottom.gamma};
    wo.cornerWarp = mapping.cornerWarp;
    wo.rotation = mapping.rotation;
    wo.mirrorX = mapping.mirrorX;
    wo.mirrorY = mapping.mirrorY;
    wo.color = mapping.color;
    wo.quality = mapping.quality;
    wo.blendSpace = mapping.maps.blendSpace;
    loadMaps(i, mapping.maps);
  }
  m_canvasUpdateQueued = false;
  m_canvasMipDrop = false;
  std::tie(m_renderState->renderFormat, m_canvasMips) = canvasSpec();
  recreateOffscreenTarget();

  // 2. Create every window. Their swap chains will be created lazily
  //    per-window in onWindowReady as each platform surface becomes
  //    available — we no longer wait on window 0 to drive all of them.
  for(int i = 0; i < (int)m_mappings.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    auto& mapping = m_mappings[i];

    wo.window = std::make_shared<Window>(conf.graphicsApi);
    wo.window->setTitle(QString("Output %1").arg(i));

    // Each window gets its own onWindowReady callback. When the native
    // surface becomes ready (first expose, or re-expose after a close),
    // we create / recreate just this window's swap chain and lazily
    // build its blit pipeline.
    wo.window->onWindowReady = [this, i] { initWindowSwapChain(i); };

    // When the user closes the window (or the platform destroys the
    // surface), tear down that window's swap chain so the other windows
    // keep running. The QWindow itself stays alive so the user can
    // later re-show it via the ossia parameters.
    wo.window->onClose = [this, i] { releaseWindowSwapChain(i); };

    // A per-window resize event only needs to poke its own swap chain.
    // The upstream graph's size is governed by the offscreen target and
    // by setRenderSize(), independently of any window.
    wo.window->onResize = [this, i] {
      if(i >= (int)m_windowOutputs.size())
        return;
      auto& w = m_windowOutputs[i];
      // No render pass descriptor: see initWindowSwapChain.
      if(w.swapChain && w.renderPassDescriptor)
      {
        w.hasSwapChain = w.swapChain->createOrResize();
        updateCanvasSpec(true);
      }
    };

    // Determine target screen
    QScreen* targetScreen = nullptr;
    if(mapping.screenIndex >= 0)
    {
      const auto& screens = qApp->screens();
      if(mapping.screenIndex < screens.size())
        targetScreen = screens[mapping.screenIndex];
    }

    if(mapping.fullscreen)
    {
      if(targetScreen)
      {
        wo.window->setScreen(targetScreen);
        wo.window->setGeometry(targetScreen->geometry());
      }
      wo.window->showFullScreen();
    }
    else
    {
      if(targetScreen)
        wo.window->setScreen(targetScreen);
      const QRect geom(mapping.windowPosition, mapping.windowSize);
      wo.window->setGeometry(geom);
      wo.window->show();
      // Ask again now that the platform window exists. A tiling window
      // manager is free to ignore a geometry set before the window is mapped,
      // and on i3 the first output window stayed invisible until it was moved
      // by one pixel; asking after show() is what makes it configure it.
      if(wo.window->geometry() != geom)
        wo.window->setGeometry(geom);
    }
  }

  // Notify the device that the QWindow objects exist (it uses this to
  // connect Qt signals like mouseMove, key, etc).
  if(onWindowsCreated)
    onWindowsCreated();

  // 3. The graph can build the render list right now — it only needs
  //    rhi + offscreen target, both of which are ready synchronously.
  //    This means the upstream graph renders from the very first frame,
  //    regardless of whether any window has been exposed yet.
  if(conf.onReady)
    conf.onReady();
}

void MultiWindowNode::destroyOutput()
{
  // Nothing to clean up if the rhi was never successfully created.
  m_canvasUpdateQueued = false;
  if(!m_renderState || !m_renderState->rhi)
  {
    // Still release any windows that may have been created before init failed.
    m_windowOutputs.clear();
    m_offscreenTarget = {};
    if(m_renderState)
      m_renderState.reset();
    return;
  }

  // Ensure the GPU has no outstanding work on any of our swap chains before
  // we start tearing resources down. Without this, the validation layer may
  // report leaked VkBuffer / VkCommandBuffer / VkInstance at shutdown because
  // there are still frames in flight when resources are destroyed.
  m_renderState->rhi->finish();

  // Persist-across-rebuild contract: registry survives RL teardown,
  // so its QRhi resources have to be torn down here (BEFORE
  // RenderState::destroy below) while the device is still alive.
  releaseRegistry();

  // Detach Window callbacks so a close that races with destruction can't
  // reach back into us while we're tearing things down.
  for(auto& wo : m_windowOutputs)
  {
    if(wo.window)
    {
      wo.window->onWindowReady = {};
      wo.window->onClose = {};
      wo.window->onResize = {};
    }
  }

  // 1. Destroy per-window GPU resources (swap chain, depth, RPD) first.
  //    Keep the QWindow shared_ptrs alive — their VkSurfaceKHR must
  //    outlive the rhi's teardown of per-window state.
  for(auto& wo : m_windowOutputs)
  {
    // Order matters: clear hasSwapChain BEFORE deleting wo.swapChain so a
    // queued event cannot observe (hasSwapChain == true && swapChain
    // dangling).
    wo.hasSwapChain = false;

    delete wo.swapChain;
    wo.swapChain = nullptr;

    delete wo.depthStencil;
    wo.depthStencil = nullptr;

    delete wo.renderPassDescriptor;
    wo.renderPassDescriptor = nullptr;
  }

  // 2. Release the offscreen target (texture + depth + RT + RPD). This
  //    must happen before `destroy()` on the rhi. It's also where
  //    m_renderState->renderPassDescriptor is cleared (we aliased the
  //    pointer in recreateOffscreenTarget()).
  m_offscreenTarget.release();
  m_renderState->renderPassDescriptor = nullptr;

  // 3. Tear down the rhi. Must happen while the windows (and thus
  //    their VkSurfaceKHR) are still alive.
  m_renderState->destroy();

  // 4. Now that the rhi is gone, releasing the windows / their surfaces
  //    is safe. Clearing the vector destroys each WindowOutput which in
  //    turn releases its shared_ptr<Window>.
  m_windowOutputs.clear();

  // 5. Release our reference to the now-empty RenderState so we don't
  //    touch it again.
  m_renderState.reset();
}

void MultiWindowNode::updateGraphicsAPI(GraphicsApi api)
{
  if(!m_renderState)
    return;

  if(m_renderState->api != api)
  {
    destroyOutput();
    return;
  }

  // Same API, but the requested sample count may have changed via the
  // settings panel. Mirror ScreenNode's clamp-and-compare path: rebuild
  // if the resolved sample count no longer matches what the rhi was
  // created with.
  auto* rhi = m_renderState->rhi;
  if(!rhi)
    return;

  int samples_request
      = Gfx::Settings::samplesForCurrentApplication(api);
  const auto supported = rhi->supportedSampleCounts();
  if(supported.isEmpty())
  {
    samples_request = 1;
  }
  else
  {
    int chosen = supported.first();
    for(int v : supported)
    {
      if(v == samples_request)
      {
        chosen = v;
        break;
      }
      if(v < samples_request)
        chosen = v;
    }
    samples_request = chosen;
  }

  if(m_renderState->samples != samples_request)
    destroyOutput();
}

void MultiWindowNode::setVSyncCallback(std::function<void()> f)
{
  m_vsyncCallback = std::move(f);
}

std::shared_ptr<RenderState> MultiWindowNode::renderState() const
{
  return m_renderState;
}

OutputNodeRenderer* MultiWindowNode::createRenderer(RenderList& r) const noexcept
{
  return new MultiWindowRenderer{*this, r.state};
}

OutputNode::Configuration MultiWindowNode::configuration() const noexcept
{
  return m_conf;
}

}
