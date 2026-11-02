#include "VideoScope.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RowOrder.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/decoders/ColorSpace.hpp>

#include <QGraphicsSceneHoverEvent>
#include <QPainterPath>
#include <QtGui/private/qrhi_p.h>

#include <cstring>

namespace Ui::VideoScope
{
namespace
{
struct Params
{
  float luma[4];
  //! 1 / (2 (1 - kb)), 1 / (2 (1 - kr)), vectorscope zoom
  float chroma[4];
  //! mode, rows reversed, overlay present
  int32_t config[4];
  //! analysed width and height, scope width and height
  int32_t sizes[4];
  //! intensity, persistence
  float view[4];
  //! linear RGB to XYZ, column-major
  float rgb2xyz[16];
  //! layout, slice, layer or face, rows reversed: see shownTextureGlsl
  int32_t shown[4];
};
static_assert(sizeof(Params) == 160);

//! The input is sampled at most at this size, in both directions
constexpr int max_analysis_size = 1024;

const char* const shader_header = R"_(#version 450
layout(std140, binding = 0) uniform Params
{
  vec4 luma;
  vec4 chroma;
  ivec4 config;
  ivec4 sizes;
  vec4 view;
  mat4 rgb2xyz;
  ivec4 shown;
};
// Binding 1, the input, only the accumulation reads: through shownSample
layout(std430, binding = 2) buffer Bins { uint bins[]; };
layout(binding = 3, rgba8) uniform writeonly image2D outImage;
layout(binding = 4) uniform sampler2D overlayTex;
)_";

// The previous frame fades: persistence 0 clears it. One invocation per bin,
// over the scope's two dimensions: a dispatch has at most 65535 groups along
// each.
const char* const decay_main = R"_(
layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  if(p.x >= sizes.z || p.y >= sizes.w)
    return;
  int base = (p.y * sizes.z + p.x) * 4;
  for(int c = 0; c < 4; c++)
    bins[base + c] = uint(float(bins[base + c]) * view.y);
}
)_";

const char* const sdr_eotf = R"_(
vec3 applyEotf(vec3 v) { return pow(max(v, 0.0), vec3(2.4)); }
)_";

// Each bin holds four counters: a count, and for the colourised modes the
// sum of the 8-bit colours that landed there; RGB overlay counts each
// channel in counters 1 to 3, the histogram uses the first row only.
const char* const accumulate_main = R"_(
layout(local_size_x = 16, local_size_y = 16) in;
void add(int x, int y, int ch, uint v)
{
  if(x < 0 || y < 0 || x >= sizes.z || y >= sizes.w)
    return;
  atomicAdd(bins[(y * sizes.z + x) * 4 + ch], v);
}
int waveRow(float v) { return int((1.0 - (v + 0.07) / 1.16) * float(sizes.w)); }
// A bin past this count shows at full level whatever the intensity, and stops
// counting: with at most 2^20 samples a frame, its colour sums stay below
// 2^21 * 255, within a uint, however long a flat field persists.
const uint saturated = 1u << 20;
void addColored(int x, int y, vec3 rgb)
{
  if(x < 0 || y < 0 || x >= sizes.z || y >= sizes.w)
    return;
  if(bins[(y * sizes.z + x) * 4] >= saturated)
    return;
  uvec3 c8 = uvec3(clamp(rgb, 0.0, 1.0) * 255.0);
  add(x, y, 0, 1u);
  add(x, y, 1, c8.r);
  add(x, y, 2, c8.g);
  add(x, y, 3, c8.b);
}
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  if(p.x >= sizes.x || p.y >= sizes.y)
    return;
  vec2 uv = (vec2(p) + 0.5) / vec2(sizes.xy);
  bool inside;
  vec3 rgb = shownSample(uv, shown.x, shown.y, shown.z != 0, inside).rgb;
  // Between the tiles of an overview
  if(!inside)
    return;
  int W = sizes.z, H = sizes.w;
  int mode = config.x;
  float y = dot(rgb, luma.rgb);
  float cb = (rgb.b - y) * chroma.x;
  float cr = (rgb.r - y) * chroma.y;
  int col = int(uv.x * float(W));
  if(mode == 0)
  {
    add(col, waveRow(y), 0, 1u);
  }
  else if(mode == 1)
  {
    add(col, waveRow(rgb.r), 1, 1u);
    add(col, waveRow(rgb.g), 2, 1u);
    add(col, waveRow(rgb.b), 3, 1u);
  }
  else if(mode == 2 || mode == 3)
  {
    int third = W / 3;
    int c = int(uv.x * float(third));
    vec3 v = mode == 2 ? rgb : vec3(y, cb + 0.5, cr + 0.5);
    add(c, waveRow(v.x), 0, 1u);
    add(third + c, waveRow(v.y), 0, 1u);
    add(2 * third + c, waveRow(v.z), 0, 1u);
  }
  else if(mode == 4)
  {
    addColored(int((0.5 + cb * chroma.z) * float(W)), int((0.5 - cr * chroma.z) * float(H)), rgb);
  }
  else if(mode == 5)
  {
    add(int(clamp(y, 0.0, 1.0) * float(W - 1)), 0, 0, 1u);
    add(int(clamp(rgb.r, 0.0, 1.0) * float(W - 1)), 0, 1, 1u);
    add(int(clamp(rgb.g, 0.0, 1.0) * float(W - 1)), 0, 2, 1u);
    add(int(clamp(rgb.b, 0.0, 1.0) * float(W - 1)), 0, 3, 1u);
  }
  else if(mode == 6)
  {
    vec3 xyz = (rgb2xyz * vec4(applyEotf(rgb), 0.0)).xyz;
    float s = xyz.x + xyz.y + xyz.z;
    if(s > 1e-6)
    {
      vec2 c = xyz.xy / s;
      addColored(int(c.x / 0.8 * float(W)), int((1.0 - c.y / 0.9) * float(H)), rgb);
    }
  }
  else if(mode == 7)
  {
    vec2 u = vec2(0.5 + (rgb.b - rgb.g) * 0.25, 0.5 - (rgb.b + rgb.g) * 0.25);
    vec2 l = vec2(0.5 + (rgb.r - rgb.g) * 0.25, 0.5 + (rgb.r + rgb.g) * 0.25);
    add(int(u.x * float(W)), int(u.y * float(H)), 0, 1u);
    add(int(l.x * float(W)), int(l.y * float(H)), 0, 1u);
  }
}
)_";

const char* const render_main = R"_(
layout(local_size_x = 16, local_size_y = 16) in;
float level(float c) { return clamp(log2(1.0 + c * view.x) / 7.0, 0.0, 1.0); }
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  int W = sizes.z, H = sizes.w;
  if(p.x >= W || p.y >= H)
    return;
  int mode = config.x;
  int base = (p.y * W + p.x) * 4;
  vec3 c = vec3(0.0);
  if(mode == 0)
  {
    c = vec3(0.45, 1.0, 0.55) * level(float(bins[base]));
  }
  else if(mode == 1)
  {
    c = vec3(level(float(bins[base + 1])), level(float(bins[base + 2])), level(float(bins[base + 3])));
  }
  else if(mode == 2 || mode == 3)
  {
    int third = p.x * 3 / W;
    vec3 tint = mode == 2
        ? (third == 0 ? vec3(1.0, 0.35, 0.35) : third == 1 ? vec3(0.35, 1.0, 0.35) : vec3(0.45, 0.55, 1.0))
        : (third == 0 ? vec3(0.9) : third == 1 ? vec3(0.45, 0.6, 1.0) : vec3(1.0, 0.5, 0.45));
    c = tint * level(float(bins[base]));
  }
  else if(mode == 4 || mode == 6)
  {
    float n = float(bins[base]);
    if(n > 0.0)
    {
      vec3 avg = vec3(bins[base + 1], bins[base + 2], bins[base + 3]) / (n * 255.0);
      c = mix(vec3(1.0), clamp(avg * 1.3, 0.0, 1.0), 0.75) * level(n);
    }
  }
  else if(mode == 5)
  {
    float total = float(sizes.x * sizes.y);
    float row = float(H - p.y) / float(H);
    vec4 counts = vec4(bins[p.x * 4], bins[p.x * 4 + 1], bins[p.x * 4 + 2], bins[p.x * 4 + 3]);
    vec4 h = log2(1.0 + counts * view.x) / log2(1.0 + total / 16.0);
    c = vec3(h.y > row ? 0.75 : 0.0, h.z > row ? 0.75 : 0.0, h.w > row ? 0.75 : 0.0)
        + (h.x > row ? vec3(0.25) : vec3(0.0));
  }
  else if(mode == 7)
  {
    c = vec3(level(float(bins[base])));
  }

  int y = config.y != 0 ? H - 1 - p.y : p.y;
  vec3 outc = vec3(0.07) + c;
  if(config.z != 0)
  {
    vec4 o = texelFetch(overlayTex, ivec2(p.x, y), 0);
    outc = mix(outc, o.rgb, o.a);
  }
  imageStore(outImage, ivec2(p.x, y), vec4(min(outc, vec3(1.0)), 1.0));
}
)_";

QString shaderFor(
    const char* main, Ui::Monitor::Color::Transfer t,
    std::optional<score::gfx::TextureKind> input = {})
{
  QString s = shader_header;
  if(input)
    s += score::gfx::shownTextureGlsl(*input, 1, "inTex");
  switch(t)
  {
    case Ui::Monitor::Color::Transfer::PQ:
      s += score::gfx::BT2020_PQ_EOTF;
      break;
    case Ui::Monitor::Color::Transfer::HLG:
      s += score::gfx::BT2020_HLG_EOTF;
      break;
    default:
      s += sdr_eotf;
      break;
  }
  s += main;
  return s;
}

//! The signal value of a luminance in cd/m2 under a transfer function.
double signalOfNits(double nits, Ui::Monitor::Color::Transfer t)
{
  using T = Ui::Monitor::Color::Transfer;
  switch(t)
  {
    case T::PQ:
      return Ui::Monitor::Color::pqOfNits(nits);
    case T::HLG: {
      // Scene light at a 1000 cd/m2 display, then the HLG OETF
      const double e = std::pow(std::max(nits, 0.) / 1000., 1. / 1.2);
      constexpr double a = 0.17883277, b = 0.28466892, c = 0.55991073;
      return e <= 1. / 12. ? std::sqrt(3. * e) : a * std::log(12. * e - b) + c;
    }
    default:
      return std::pow(std::max(nits, 0.) / 100., 1. / 2.4);
  }
}
}

void drawGraticule(
    QPainter& p, const QRectF& r, Mode m, Units u, bool legal,
    Ui::Monitor::Color::Matrix matrix, Ui::Monitor::Color::Transfer transfer,
    bool targets75, double zoom)
{
  namespace C = Ui::Monitor::Color;
  const QColor line(255, 255, 255, 70), strong(255, 255, 255, 120), text(215, 215, 215, 230);
  QFont f = p.font();
  f.setPixelSize(std::max(9, int(r.height() / 40)));
  p.setFont(f);
  p.setRenderHint(QPainter::Antialiasing, true);
  auto at = [&](QPointF n) { return QPointF{r.left() + n.x() * r.width(), r.top() + n.y() * r.height()}; };
  auto label = [&](QPointF pt, const QString& s, int align = Qt::AlignLeft | Qt::AlignVCenter) {
    p.setPen(text);
    QRectF box{pt.x() + 2, pt.y() - 8, 60, 16};
    if(align & Qt::AlignRight)
      box.moveRight(pt.x() - 2);
    p.drawText(box, align, s);
  };

  switch(m)
  {
    case Mode::Waveform:
    case Mode::RgbOverlay:
    case Mode::RgbParade:
    case Mode::YCbCrParade: {
      // Signal value of a percentage of the nominal range
      const double lo = legal ? 16. / 255. : 0., hi = legal ? 235. / 255. : 1.;
      auto pct = [&](double v) { return lo + (hi - lo) * v / 100.; };
      std::vector<std::pair<double, QString>> ticks;
      switch(u)
      {
        case Units::Percent:
          for(int v = 0; v <= 100; v += 10)
            ticks.emplace_back(pct(v), QString::number(v));
          break;
        case Units::Millivolts:
          for(int v = 0; v <= 700; v += 100)
            ticks.emplace_back(pct(v / 7.), QString::number(v));
          break;
        case Units::Code8:
          for(int v : {0, 16, 64, 128, 192, 235, 255})
            ticks.emplace_back(v / 255., QString::number(v));
          break;
        case Units::Code10:
          for(int v : {0, 64, 256, 512, 768, 940, 1023})
            ticks.emplace_back(v / 1023., QString::number(v));
          break;
        case Units::Nits:
          for(double v : {0.1, 1., 10., 100., 203., 1000., 4000., 10000.})
          {
            const double s = signalOfNits(v, transfer);
            if(s <= 1.0001)
              ticks.emplace_back(s, QString::number(v));
          }
          break;
      }
      for(auto& [v, s] : ticks)
      {
        const double y = geometry::waveY(v);
        p.setPen(QPen(line, 1));
        p.drawLine(at({0, y}), at({1, y}));
        label(at({0, y}), s);
      }
      // Legal limits, dashed
      p.setPen(QPen(QColor(255, 200, 60, 120), 1, Qt::DashLine));
      for(double v : {16. / 255., 235. / 255.})
        p.drawLine(at({0, geometry::waveY(v)}), at({1, geometry::waveY(v)}));
      if(m == Mode::RgbParade || m == Mode::YCbCrParade)
      {
        const QString names[2][3]{{"R", "G", "B"}, {"Y", "Cb", "Cr"}};
        for(int i = 0; i < 3; i++)
        {
          if(i > 0)
          {
            p.setPen(QPen(strong, 1));
            p.drawLine(at({i / 3., 0}), at({i / 3., 1}));
          }
          label(at({i / 3. + 1. / 6. - 0.02, 0.97}), names[m == Mode::YCbCrParade][i]);
        }
      }
      break;
    }
    case Mode::Vectorscope: {
      const QPointF c = at({0.5, 0.5});
      p.setPen(QPen(line, 1));
      p.drawLine(at({0.5, 0}), at({0.5, 1}));
      p.drawLine(at({0, 0.5}), at({1, 0.5}));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(c, r.width() * 0.5 * zoom * 0.5, r.height() * 0.5 * zoom * 0.5);
      p.drawEllipse(c, r.width() * 0.5 * zoom, r.height() * 0.5 * zoom);
      // Skin tone line along the I axis
      const double a = qDegreesToRadians(C::skin_tone_degrees);
      p.setPen(QPen(QColor(255, 190, 140, 160), 1));
      p.drawLine(c, at(geometry::vector(0.5 * std::cos(a), 0.5 * std::sin(a), zoom)));
      const auto targets = C::barTargets(matrix, targets75 ? 0.75 : 1.);
      const double box = r.width() * 0.02;
      for(int i = 0; i < 6; i++)
      {
        const QPointF t = at(geometry::vector(targets[i].cb, targets[i].cr, zoom));
        p.setPen(QPen(strong, 1));
        p.drawRect(QRectF(t.x() - box / 2, t.y() - box / 2, box, box));
        label(t + QPointF{box / 2, 0}, C::bar_names[i]);
      }
      label(at({0.01, 0.03}), targets75 ? "75%" : "100%");
      break;
    }
    case Mode::Histogram: {
      for(int v = 0; v <= 100; v += 25)
      {
        const double x = v / 100.;
        p.setPen(QPen(line, 1));
        p.drawLine(at({x, 0}), at({x, 1}));
        label(at({x, 0.97}), QString::number(v), Qt::AlignLeft | Qt::AlignVCenter);
      }
      break;
    }
    case Mode::CIE: {
      p.setPen(QPen(line, 1));
      for(int i = 1; i <= 8; i++)
      {
        p.drawLine(at(geometry::cie(i / 10., 0)), at(geometry::cie(i / 10., 0.9)));
        if(i <= 9)
          p.drawLine(at(geometry::cie(0, i / 10.)), at(geometry::cie(0.8, i / 10.)));
        label(at(geometry::cie(i / 10., 0.)) - QPointF{0, 8}, QString::number(i / 10.));
      }
      QPainterPath locus;
      for(std::size_t i = 0; i < C::spectral_locus.size(); i++)
      {
        const auto pt = at(geometry::cie(C::spectral_locus[i][0], C::spectral_locus[i][1]));
        i == 0 ? locus.moveTo(pt) : locus.lineTo(pt);
      }
      locus.closeSubpath();
      p.setPen(QPen(strong, 1.5));
      p.drawPath(locus);
      auto triangle = [&](const C::Primaries& pr, QColor col, const QString& name) {
        QPolygonF tri;
        tri << at(geometry::cie(pr.rx, pr.ry)) << at(geometry::cie(pr.gx, pr.gy))
            << at(geometry::cie(pr.bx, pr.by));
        p.setPen(QPen(col, 1.2));
        p.drawPolygon(tri);
        p.setPen(col);
        p.drawText(tri[0] + QPointF{4, 4}, name);
      };
      triangle(C::rec709, QColor(240, 240, 240, 200), "709");
      triangle(C::dciP3, QColor(90, 210, 240, 200), "P3");
      triangle(C::rec2020, QColor(240, 160, 60, 200), "2020");
      QPainterPath planck;
      for(double t = 1667.; t <= 25000.; t *= 1.05)
      {
        const auto xy = C::planckian(t);
        const auto pt = at(geometry::cie(xy[0], xy[1]));
        t == 1667. ? planck.moveTo(pt) : planck.lineTo(pt);
      }
      p.setPen(QPen(QColor(255, 220, 120, 150), 1));
      p.drawPath(planck);
      for(double t : {2000., 3000., 6500., 10000.})
      {
        const auto xy = C::planckian(t);
        const auto pt = at(geometry::cie(xy[0], xy[1]));
        p.setBrush(QColor(255, 220, 120));
        p.drawEllipse(pt, 2, 2);
        label(pt + QPointF{0, 10}, QString::number(int(t)) + "K");
      }
      const auto d65 = at(geometry::cie(0.3127, 0.3290));
      p.setBrush(Qt::white);
      p.drawEllipse(d65, 2.5, 2.5);
      label(d65 + QPointF{2, -8}, "D65");
      break;
    }
    case Mode::Diamond: {
      auto diamond = [&](auto fn) {
        QPolygonF d;
        d << at(fn(0., 0.)) << at(fn(1., 0.)) << at(fn(1., 1.)) << at(fn(0., 1.));
        p.setPen(QPen(strong, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(d);
      };
      diamond([](double a, double g) { return geometry::diamondUpper(a, g); });
      diamond([](double a, double g) { return geometry::diamondLower(a, g); });
      label(at(geometry::diamondUpper(1, 0)), "B");
      label(at(geometry::diamondUpper(0, 1)), "G", Qt::AlignRight | Qt::AlignVCenter);
      label(at(geometry::diamondLower(1, 0)), "R");
      label(at(geometry::diamondLower(0, 1)), "G", Qt::AlignRight | Qt::AlignVCenter);
      break;
    }
  }
}

Node::Node() = default;
Node::~Node() = default;

bool Node::createPipelines(score::gfx::RenderList& r)
{
  auto& rhi = *r.state.rhi;
  const auto t = inputs.transfer.value;
  // Both or none: a node left with one of them would dispatch it alone
  QRhiComputePipeline* made[2]{};
  const char* const mains[2]{decay_main, render_main};
  bool ok = true;
  for(int i = 0; i < 2 && ok; i++)
  {
    try
    {
      made[i] = rhi.newComputePipeline();
      made[i]->setShaderStage(
          {QRhiShaderStage::Compute, score::gfx::makeCompute(r.state, shaderFor(mains[i], t))});
      made[i]->setShaderResourceBindings(m_srb);
      ok = made[i]->create();
    }
    catch(...)
    {
      ok = false;
    }
  }
  if(!ok)
  {
    // Never recorded into a frame
    for(auto* pl : made)
      delete pl;
    return false;
  }
  m_decay = made[0];
  m_render = made[1];
  return true;
}

QRhiComputePipeline* Node::accumulate(score::gfx::RenderList& r, score::gfx::TextureKind kind)
{
  // The variants differ only in the type of binding 1: all of them work with
  // the bindings, whatever texture these point at
  const int k = int(kind);
  if(m_accumulate[k] || m_accumulateFailed[k] || !m_srb)
    return m_accumulate[k];
  QRhiComputePipeline* pl{};
  try
  {
    pl = r.state.rhi->newComputePipeline();
    pl->setShaderStage(
        {QRhiShaderStage::Compute,
         score::gfx::makeCompute(
             r.state, shaderFor(accumulate_main, m_builtTransfer, kind))});
    pl->setShaderResourceBindings(m_srb);
    if(!pl->create())
    {
      delete pl;
      pl = nullptr;
    }
  }
  catch(...)
  {
    delete pl;
    pl = nullptr;
  }
  m_accumulate[k] = pl;
  m_accumulateFailed[k] = !pl;
  return pl;
}

void Node::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  m_rhi = r.state.rhi;
  if(!m_rhi || !m_rhi->isFeatureSupported(QRhi::Compute))
    return;
  auto& rhi = *m_rhi;

  m_params = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Params));
  m_params->create();
  m_sampler = rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
  m_sampler->create();
  allocate(512);
}

void Node::allocate(int sz)
{
  auto& rhi = *m_rhi;
  for(QRhiResource* r : std::initializer_list<QRhiResource*>{m_bins, m_out, m_overlay})
    if(r)
      r->deleteLater();
  size = sz;
  m_bins = rhi.newBuffer(
      QRhiBuffer::Static, QRhiBuffer::StorageBuffer, size * size * 4 * sizeof(uint32_t));
  m_bins->create();
  // The bins start at zero: the first decay pass clears them
  m_clearBins = true;

  m_out = rhi.newTexture(
      QRhiTexture::RGBA8, QSize{size, size}, 1, QRhiTexture::UsedWithLoadStore);
  m_out->create();
  m_overlay = rhi.newTexture(QRhiTexture::RGBA8, QSize{size, size}, 1);
  m_overlay->create();

  outputs.scope.texture.handle = m_out;
  outputs.scope.texture.width = size;
  outputs.scope.texture.height = size;
  outputs.scope.texture.format = halp::gpu_texture::RGBA8;
  m_boundInput = nullptr;
  m_overlayKey = -1;
}

bool Node::rebuildBindings(QRhiTexture* input)
{
  using B = QRhiShaderResourceBinding;
  constexpr auto stage = B::ComputeStage;
  const bool created = m_srb != nullptr;
  if(!m_srb)
    m_srb = m_rhi->newShaderResourceBindings();
  m_srb->setBindings(
      {B::uniformBuffer(0, stage, m_params), B::sampledTexture(1, stage, input, m_sampler),
       B::bufferLoadStore(2, stage, m_bins), B::imageStore(3, stage, m_out, 0),
       B::sampledTexture(4, stage, m_overlay, m_sampler)});
  // Same layout, other resources: the pipelines built against it still apply
  if(created)
  {
    m_srb->updateResources();
    return true;
  }
  if(!m_srb->create())
  {
    delete m_srb;
    m_srb = nullptr;
    return false;
  }
  return true;
}

void Node::runInitialPasses(
    score::gfx::RenderList& r, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
    score::gfx::Edge& e)
{
  using namespace score::gfx;
  auto* input = static_cast<QRhiTexture*>(inputs.image.texture.handle);
  const TextureShape shape = input ? TextureShape::of(*input) : TextureShape{};
  if(monitor)
    Ui::Monitor::publish(*monitor, shape);

  if(!m_out || !m_rhi)
    return;
  auto& rhi = *m_rhi;
  namespace C = Ui::Monitor::Color;

  // As large as what shows it, in steps of 16 for the dispatches
  {
    // The largest of all that show it in this list: the node's view and the
    // inspector's preview of the same outlet both read this one texture
    QSize shown = r.renderSize(&e);
    if(e.source)
      for(const auto* edge : e.source->edges)
        if(edge)
          shown = shown.expandedTo(r.renderSize(edge));
    const int want = std::clamp(std::min(shown.width(), shown.height()) / 16 * 16, min_size, max_size);
    if(want != size)
      allocate(want);
  }

  QRhiTexture* const empty = &r.emptyTexture();
  if(!input)
    input = empty;
  const ShownTexture shown = Ui::Monitor::shownTexture(
      shape, inputs.layout.value, inputs.slice.value, inputs.layer.value);

  // Bindings follow the input texture; pipelines follow the transfer
  // function, which is compiled in
  if(input != m_boundInput || !m_srb)
  {
    if(!rebuildBindings(input))
      return;
    m_boundInput = input;
  }
  if(!m_decay || m_builtTransfer != inputs.transfer.value)
  {
    // A transfer whose shaders did not build is not tried again every frame
    if(!m_decay && m_pipelinesFailed && m_builtTransfer == inputs.transfer.value)
      return;
    for(auto* pl : {m_decay, m_render})
      if(pl)
        pl->deleteLater();
    for(auto*& pl : m_accumulate)
    {
      if(pl)
        pl->deleteLater();
      pl = nullptr;
    }
    m_accumulateFailed = {};
    m_decay = m_render = nullptr;
    m_builtTransfer = inputs.transfer.value;
    m_pipelinesFailed = !createPipelines(r);
    if(m_pipelinesFailed)
      return;
  }

  const Mode mode = inputs.mode.value;
  const auto matrix = inputs.matrix.value;
  const bool targets75 = inputs.targets75.value;
  const double zoom = inputs.zoom.value;

  // The graticule, redrawn when what it shows changes
  const int key = int(mode) | int(inputs.units.value) << 4 | int(inputs.legal.value) << 8
                  | int(matrix) << 9 | int(inputs.transfer.value) << 12
                  | int(targets75) << 15 | int(zoom * 100) << 16;
  if(key != m_overlayKey)
  {
    QImage img(size, size, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);
    {
      QPainter p(&img);
      drawGraticule(
          p, QRectF(0, 0, size, size), mode, inputs.units.value, inputs.legal.value, matrix,
          inputs.transfer.value, targets75, zoom);
    }
    img = img.convertToFormat(QImage::Format_RGBA8888);
    res->uploadTexture(m_overlay, score::gfx::imageInPublishedRowOrder(rhi, std::move(img)));
    // Another mode leaves nothing of the previous one's bins
    if((key & 0xF) != (m_overlayKey & 0xF))
      m_clearBins = true;
    m_overlayKey = key;
  }

  Params params{};
  const auto k = C::coefficients(matrix);
  params.luma[0] = float(k.kr);
  params.luma[1] = float(k.kg);
  params.luma[2] = float(k.kb);
  params.chroma[0] = float(1. / (2. * (1. - k.kb)));
  params.chroma[1] = float(1. / (2. * (1. - k.kr)));
  params.chroma[2] = float(zoom);
  params.config[0] = int(mode);
  params.config[1] = score::gfx::uploadReversesRows(rhi) ? 1 : 0;
  params.config[2] = 1;
  // What the layout shows of a texture of several images
  const QSize shown_size = shown.size(max_analysis_size * 4);
  const int iw = shown.layered() ? shown_size.width() : std::max(1, inputs.image.texture.width);
  const int ih = shown.layered() ? shown_size.height() : std::max(1, inputs.image.texture.height);
  QSize analysed{iw, ih};
  if(iw > max_analysis_size || ih > max_analysis_size)
    analysed = analysed.scaled(max_analysis_size, max_analysis_size, Qt::KeepAspectRatio);
  const int aw = std::max(1, analysed.width());
  const int ah = std::max(1, analysed.height());
  params.sizes[0] = aw;
  params.sizes[1] = ah;
  params.sizes[2] = size;
  params.sizes[3] = size;
  params.view[0] = inputs.intensity.value;
  if(monitor)
    if(const uint32_t c = monitor->clears.load(std::memory_order_relaxed); c != m_clears)
    {
      m_clears = c;
      m_clearBins = true;
    }
  params.view[1] = m_clearBins ? 0.f : inputs.persistence.value;
  m_clearBins = false;
  const auto xyz = C::rgbToXyz(matrix == C::Matrix::Rec2020 ? C::rec2020 : C::rec709);
  for(int col = 0; col < 3; col++)
    for(int row = 0; row < 3; row++)
      params.rgb2xyz[col * 4 + row] = float(xyz[row][col]);
  params.rgb2xyz[15] = 1.f;
  params.shown[0] = shown.overview() ? 1 : 0;
  params.shown[1] = shown.index;
  params.shown[2] = rhi.isYUpInNDC() ? 1 : 0;
  res->updateDynamicBuffer(m_params, 0, sizeof(Params), &params);

  QRhiComputePipeline* const acc = accumulate(r, shape.kind);
  const bool has_input = input != empty && acc;
  auto pass = [&](QRhiComputePipeline* pl, int x, int y) {
    cb.beginComputePass(res);
    res = rhi.nextResourceUpdateBatch();
    cb.setComputePipeline(pl);
    cb.setShaderResources(m_srb);
    cb.dispatch(x, y, 1);
    cb.endComputePass();
  };
  pass(m_decay, size / 16, size / 16);
  if(has_input)
    pass(acc, (aw + 15) / 16, (ah + 15) / 16);
  pass(m_render, size / 16, size / 16);

}

void Node::release(score::gfx::RenderList& r)
{
  for(auto*& pl : m_accumulate)
  {
    if(pl)
      pl->deleteLater();
    pl = nullptr;
  }
  m_accumulateFailed = {};
  for(QRhiResource* res : std::initializer_list<QRhiResource*>{
          m_decay, m_render, m_srb, m_params, m_bins, m_out, m_overlay, m_sampler})
    if(res)
      res->deleteLater();
  m_decay = m_render = nullptr;
  m_srb = nullptr;
  m_params = m_bins = nullptr;
  m_out = m_overlay = nullptr;
  m_sampler = nullptr;
  m_boundInput = nullptr;
  outputs.scope.texture.handle = nullptr;
  outputs.scope.texture.width = 0;
  outputs.scope.texture.height = 0;
}

Layer::Layer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent)
    : Gfx::OutletPreviewLayer{process, doc, parent}
    , m_scope{process}
{
  // For the double click; a press goes on to the node
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  Ui::Monitor::showInlet(process, 0, doc, this, this);
  Ui::Monitor::ModeSwitch::make(process, doc, this, this);
  aspect = [] { return std::optional<double>{1.}; };
  placeholder = QStringLiteral("Video scope\nno image");
}

void Layer::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if(!m_doubleClick(*ev))
  {
    ev->ignore();
    return;
  }
  ev->accept();
  if(auto m = std::dynamic_pointer_cast<ScopeMonitor>(m_scope.monitor()))
    m->clears.fetch_add(1, std::memory_order_relaxed);
}

void Layer::hoverMoveEvent(QGraphicsSceneHoverEvent*)
{
  update();
}

void Layer::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
  update();
}

void Layer::paintOverlay(QPainter* p, const QRectF& r) const
{
  if(!isUnderMouse())
    return;
  auto* m = dynamic_cast<const ScopeMonitor*>(m_scope.monitor().get());
  if(!m)
    return;
  const auto shown = Ui::Monitor::shownTexture(m_scope, Ui::Monitor::latestShape(*m));
  QStringList info = Ui::Monitor::describeShown(shown);
  if(shown.overview())
  {
    using K = score::gfx::TextureKind;
    const auto k = shown.shape.kind;
    info << QStringLiteral("overview, %1 %2")
                .arg(shown.tiles())
                .arg(k == K::Cube ? "faces" : k == K::Array ? "layers" : "slices");
  }
  // Under the mode switch, right of the inlet
  Ui::Monitor::drawReadout(
      *p, QPointF{std::max(r.left() + 2, 14.), std::max(r.top() + 2, 18.)}, info);
}
}
