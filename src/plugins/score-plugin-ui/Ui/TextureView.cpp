#include "TextureView.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QPainter>
#include <QtGui/private/qrhi_p.h>

#include <Ui/Monitor/Paint.hpp>

#include <cmath>

namespace Ui::TextureView
{
namespace
{
struct Params
{
  float luma[4];
  //! mode, show NaN, background, reference present
  int32_t config[4];
  //! exposure gain, zebra high, zebra low, peaking threshold
  float view[4];
  //! blend, wipe, frame counter
  float view2[4];
  //! width, height
  int32_t sizes[4];
};
static_assert(sizeof(Params) == 80);

constexpr int max_size = 8192;

const char* const shader = R"_(#version 450
layout(local_size_x = 16, local_size_y = 16) in;
layout(std140, binding = 0) uniform Params
{
  vec4 luma;
  ivec4 config;
  vec4 view;
  vec4 view2;
  ivec4 sizes;
};
layout(binding = 1) uniform sampler2D inTex;
layout(binding = 2) uniform sampler2D refTex;
layout(binding = 3, rgba8) uniform writeonly image2D outImage;

// False-colour palette after the ARRI conventions, as in the Video Monitor
// shader of the library
vec3 false_color(float L)
{
  if(L < 0.005) return vec3(0.0, 0.0, 0.4);
  if(L < 0.02)  return vec3(0.0, 0.0, 0.8);
  if(L < 0.06)  return vec3(0.0, 0.4, 0.8);
  if(L < 0.12)  return vec3(0.0, 0.6, 0.3);
  if(L < 0.18)  return vec3(0.2, 0.2, 0.2);
  if(L < 0.25)  return vec3(0.4, 0.4, 0.4);
  if(L < 0.40)  return vec3(0.3, 0.6, 0.1);
  if(L < 0.55)  return vec3(0.5, 0.8, 0.2);
  if(L < 0.70)  return vec3(0.8, 0.8, 0.0);
  if(L < 0.85)  return vec3(1.0, 0.5, 0.0);
  if(L < 0.95)  return vec3(1.0, 0.2, 0.2);
  if(L < 1.005) return vec3(1.0, 0.0, 0.5);
  return vec3(1.0, 0.0, 1.0);
}

float lumaAt(ivec2 p)
{
  p = clamp(p, ivec2(0), sizes.xy - 1);
  return dot(texelFetch(inTex, p, 0).rgb * view.x, luma.rgb);
}

void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  if(p.x >= sizes.x || p.y >= sizes.y)
    return;
  vec4 src = texelFetch(inTex, p, 0);
  vec3 c = src.rgb * view.x;
  float a = src.a;
  float L = dot(c, luma.rgb);
  int mode = config.x;
  float blend = view2.x;
  vec3 outc = c;

  if(mode == 0)
  {
    // Under the image: checkerboard, black, grey, white, magenta, or none
    // to show the colour whatever its alpha
    vec3 under;
    switch(config.z)
    {
      case 0: under = ((p.x / 8 + p.y / 8) % 2 == 0) ? vec3(0.42) : vec3(0.58); break;
      case 1: under = vec3(0.0); break;
      case 2: under = vec3(0.18); break;
      case 3: under = vec3(1.0); break;
      default: under = vec3(1.0, 0.0, 1.0); break;
    }
    if(config.z != 5)
      outc = mix(under, clamp(c, 0.0, 1.0), clamp(a, 0.0, 1.0));
  }
  else if(mode == 1) outc = vec3(c.r);
  else if(mode == 2) outc = vec3(c.g);
  else if(mode == 3) outc = vec3(c.b);
  else if(mode == 4) outc = vec3(a);
  else if(mode == 5) outc = vec3(L);
  else if(mode == 6)
  {
    outc = mix(clamp(c, 0.0, 1.0), false_color(L), blend);
  }
  else if(mode == 7)
  {
    float maxC = max(c.r, max(c.g, c.b));
    float stripe = step(0.5, fract(float(p.x + p.y) / 8.0));
    outc = clamp(c, 0.0, 1.0);
    if(maxC > view.y)
      outc = mix(outc, vec3(1.0, 0.0, 0.0), stripe * blend);
    if(maxC < view.z)
      outc = mix(outc, vec3(0.0, 0.0, 1.0), stripe * blend);
  }
  else if(mode == 8)
  {
    // Sobel on luma: sharp edges are in focus
    float tl = lumaAt(p + ivec2(-1, -1)), t = lumaAt(p + ivec2(0, -1)), tr = lumaAt(p + ivec2(1, -1));
    float l = lumaAt(p + ivec2(-1, 0)), r = lumaAt(p + ivec2(1, 0));
    float bl = lumaAt(p + ivec2(-1, 1)), b = lumaAt(p + ivec2(0, 1)), br = lumaAt(p + ivec2(1, 1));
    float gx = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
    float gy = (bl + 2.0 * b + br) - (tl + 2.0 * t + tr);
    float edge = length(vec2(gx, gy));
    outc = clamp(c, 0.0, 1.0) * 0.6;
    if(edge > view.w)
      outc = mix(outc, vec3(1.0, 0.15, 0.15), blend + (1.0 - blend) * 0.5);
  }
  else if(mode == 9)
  {
    float stripe = step(0.5, fract(float(p.x + p.y) / 6.0));
    outc = clamp(c, 0.0, 1.0);
    if(any(lessThan(c, vec3(0.0))))
      outc = mix(outc, vec3(0.0, 1.0, 1.0), stripe * blend);
    else if(any(greaterThan(c, vec3(1.0))))
      outc = mix(outc, vec3(1.0, 0.0, 1.0), stripe * blend);
  }
  else if(mode == 10 || mode == 11)
  {
    vec2 uv = (vec2(p) + 0.5) / vec2(sizes.xy);
    vec3 ref = config.w != 0 ? textureLod(refTex, uv, 0.0).rgb * view.x : vec3(0.0);
    if(mode == 10)
    {
      int split = int(view2.y * float(sizes.x));
      outc = p.x < split ? c : ref;
      if(abs(p.x - split) < 1)
        outc = vec3(1.0);
    }
    else
    {
      outc = abs(c - ref);
    }
  }

  if(config.y != 0 && (any(isnan(src)) || any(isinf(src))))
  {
    // Blinks, so that it cannot be mistaken for content
    outc = (int(view2.z) / 15) % 2 == 0 ? vec3(1.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
  }
  imageStore(outImage, p, vec4(clamp(outc, 0.0, 1.0), 1.0));
}
)_";
}

Node::Node() = default;
Node::~Node() = default;

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
  resizeOutput(QSize{16, 16});

  // The pipeline is built once, against bindings of the same layout as the
  // ones each frame then points at its textures
  auto* empty = &r.emptyTexture();
  m_srb = rhi.newShaderResourceBindings();
  setBindings(empty, empty);
  m_boundInput = m_boundReference = empty;
  if(!m_srb->create())
  {
    delete m_srb;
    m_srb = nullptr;
    return;
  }
  try
  {
    m_pipeline = rhi.newComputePipeline();
    m_pipeline->setShaderStage(
        {QRhiShaderStage::Compute, score::gfx::makeCompute(r.state, shader)});
    m_pipeline->setShaderResourceBindings(m_srb);
    if(!m_pipeline->create())
    {
      delete m_pipeline;
      m_pipeline = nullptr;
    }
  }
  catch(...)
  {
    delete m_pipeline;
    m_pipeline = nullptr;
  }
}

bool Node::resizeOutput(QSize sz)
{
  if(m_out && m_out->pixelSize() == sz)
    return false;
  if(m_out)
    m_out->deleteLater();
  m_out = m_rhi->newTexture(QRhiTexture::RGBA8, sz, 1, QRhiTexture::UsedWithLoadStore);
  m_out->create();
  outputs.texture.texture.handle = m_out;
  outputs.texture.texture.width = sz.width();
  outputs.texture.texture.height = sz.height();
  outputs.texture.texture.format = halp::gpu_texture::RGBA8;
  return true;
}

void Node::setBindings(QRhiTexture* input, QRhiTexture* reference)
{
  using B = QRhiShaderResourceBinding;
  constexpr auto stage = B::ComputeStage;
  m_srb->setBindings(
      {B::uniformBuffer(0, stage, m_params), B::sampledTexture(1, stage, input, m_sampler),
       B::sampledTexture(2, stage, reference, m_sampler), B::imageStore(3, stage, m_out, 0)});
}

void Node::runInitialPasses(
    score::gfx::RenderList& r, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
    score::gfx::Edge& e)
{
  if(!m_rhi || !m_pipeline || !m_srb || !m_out)
    return;
  auto& rhi = *m_rhi;

  auto* input = static_cast<QRhiTexture*>(inputs.image.texture.handle);
  auto* reference = static_cast<QRhiTexture*>(inputs.reference.texture.handle);
  const QSize in_size = input ? input->pixelSize() : QSize{16, 16};
  const QSize out_size{
      std::clamp(in_size.width(), 1, max_size), std::clamp(in_size.height(), 1, max_size)};

  const bool resized = resizeOutput(out_size);
  QRhiTexture* const empty = &r.emptyTexture();
  QRhiTexture* in = input ? input : empty;
  QRhiTexture* ref = reference ? reference : empty;
  if(resized || in != m_boundInput || ref != m_boundReference)
  {
    // Same layout: the pipeline keeps working with them
    setBindings(in, ref);
    m_srb->updateResources();
    m_boundInput = in;
    m_boundReference = ref;
  }

  const bool rec2020 = inputs.luma.value == LumaStandard::Rec2020;
  Params params{
      .luma = {rec2020 ? 0.2627f : 0.2126f, rec2020 ? 0.6780f : 0.7152f, rec2020 ? 0.0593f : 0.0722f, 0.f},
      .config = {int(inputs.mode.value), inputs.nan.value ? 1 : 0, int(inputs.background.value), reference ? 1 : 0},
      .view = {std::exp2(inputs.exposure.value), inputs.zebra_high.value, inputs.zebra_low.value, inputs.peaking.value},
      .view2 = {inputs.blend.value, inputs.wipe.value, float(m_frame++), 0.f},
      .sizes = {out_size.width(), out_size.height(), 0, 0}};
  res->updateDynamicBuffer(m_params, 0, sizeof(Params), &params);

  cb.beginComputePass(res);
  res = rhi.nextResourceUpdateBatch();
  cb.setComputePipeline(m_pipeline);
  cb.setShaderResources(m_srb);
  cb.dispatch((out_size.width() + 15) / 16, (out_size.height() + 15) / 16, 1);
  cb.endComputePass();

  outputs.resolution.value = {float(in_size.width()), float(in_size.height())};
  if(!input)
    outputs.resolution.value = {0.f, 0.f};
}

void Node::release(score::gfx::RenderList& r)
{
  for(QRhiResource* res : std::initializer_list<QRhiResource*>{
          m_pipeline, m_srb, m_params, m_out, m_sampler})
    if(res)
      res->deleteLater();
  m_pipeline = nullptr;
  m_srb = nullptr;
  m_params = nullptr;
  m_out = nullptr;
  m_sampler = nullptr;
  m_boundInput = m_boundReference = nullptr;
  outputs.texture.texture.handle = nullptr;
  outputs.texture.texture.width = 0;
  outputs.texture.texture.height = 0;
}

Layer::Layer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent)
    : Gfx::OutletPreviewLayer{process, doc, parent}
{
  setAcceptedMouseButtons({});
  setAcceptHoverEvents(true);
  Ui::Monitor::showInlet(process, 0, doc, this, this);
  Ui::Monitor::showInlet(process, 1, doc, this, this, 15.);
  Ui::Monitor::ModeSwitch::make(process, doc, this, this);
  if(process.outlets().size() > 1)
    m_resolution = qobject_cast<const Process::ControlOutlet*>(process.outlets()[1]);
  placeholder = QStringLiteral("Texture view\nno image");
}

std::array<float, 2> Layer::resolution() const
{
  if(!m_resolution)
    return {};
  const auto v = ossia::convert<ossia::vec2f>(m_resolution->value());
  return {v[0], v[1]};
}

void Layer::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
  m_hover = ev->pos();
}

void Layer::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
  m_hover.reset();
  update();
}

void Layer::paintOverlay(QPainter* p, const QRectF& r) const
{
  using namespace Ui::Monitor;
  const auto res = resolution();
  QStringList info;
  if(res[0] > 0)
    info << QStringLiteral("rendered at %1 x %2").arg(res[0]).arg(res[1]);
  if(m_hover && r.contains(*m_hover) && res[0] > 0)
  {
    const double u = (m_hover->x() - r.left()) / r.width();
    const double v = (m_hover->y() - r.top()) / r.height();
    const QImage img = preview().image();
    QString value;
    if(!img.isNull())
    {
      const QColor c = img.pixelColor(
          std::clamp(int(u * img.width()), 0, img.width() - 1),
          std::clamp(int(v * img.height()), 0, img.height() - 1));
      value = QStringLiteral("  shown %1 %2 %3").arg(c.red()).arg(c.green()).arg(c.blue());
    }
    info << QStringLiteral("%1, %2").arg(int(u * res[0])).arg(int(v * res[1])) + value;
  }
  if(!info.isEmpty() && isUnderMouse())
    // Under the mode switch, right of the inlets
    drawReadout(*p, QPointF{std::max(r.left() + 2, 14.), std::max(r.top() + 2, 16.)}, info);
}
}
