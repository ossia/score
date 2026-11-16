#include "TextureCapture.hpp"

#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/Graph/PreviewTap.hpp>

#include <QObject>

#include <rhi/qrhi.h>

#include <cstring>

namespace Gfx
{
std::optional<port_index> textureProducer(const ossia::net::parameter_base& p)
{
  if(auto* t = dynamic_cast<const simple_texture_input_parameter*>(&p))
    if(t->node_id != score::gfx::invalid_node_index)
      return port_index{t->node_id, 0};
  if(auto* t = dynamic_cast<const video_texture_input_parameter*>(&p))
    if(t->node_id != score::gfx::invalid_node_index)
      return port_index{t->node_id, 0};
  return std::nullopt;
}

TextureCapture::TextureCapture(GfxContext& ctx, port_index producer)
{
  m_preview.attachNative(&ctx, producer);
}

TextureCapture::~TextureCapture() = default;

void TextureCapture::setProducer(port_index producer)
{
  m_preview.setProducer(producer);
}

TextureCapture::State TextureCapture::state() const noexcept
{
  using P = score::gfx::CaptureProblem;
  if(!m_preview.attached())
    return State::Unavailable;
  switch(m_preview.context()->capture_problem(m_preview.id()))
  {
    case P::None:
      break;
    case P::NoOwnTexture:
      return State::Waiting;
    default:
      return State::Unavailable;
  }
  return m_preview.tapped() || m_preview.onHub() ? State::Capturing : State::Waiting;
}

QString TextureCapture::reason() const
{
  using P = score::gfx::CaptureProblem;
  if(!m_preview.attached())
    return QObject::tr("The graphics context is gone");
  switch(m_preview.context()->capture_problem(m_preview.id()))
  {
    case P::None:
      return {};
    case P::NoProducer:
      return QObject::tr("No node renders this texture");
    case P::NoFreeInput:
      return QObject::tr("Too many captures at once");
    case P::NoOwnTexture:
      return QObject::tr("The producer has published no texture of its own yet");
    case P::NotTexture2D:
      return QObject::tr("The texture is not a single 2D image");
    case P::FloatUnsupported:
      return QObject::tr("The graphics backend cannot render to float textures");
  }
  return {};
}

void TextureCapture::request()
{
  m_preview.request();
}

int64_t TextureCapture::frames() const noexcept
{
  return m_preview.frames();
}

bool TextureCapture::take(std::vector<float>& rgba, int& width, int& height)
{
  const int64_t done = frames();
  if(done == m_taken)
    return false;
  m_taken = done;

  const auto* rb = m_preview.readback();
  if(!rb || rb->format != QRhiTexture::RGBA32F)
    return false;
  const int w = rb->pixelSize.width(), h = rb->pixelSize.height();
  if(w <= 0 || h <= 0)
    return false;
  const std::size_t n = std::size_t(w) * h * 4;
  if(rb->data.size() < qsizetype(n * sizeof(float)))
    return false;
  rgba.resize(n);
  std::memcpy(rgba.data(), rb->data.constData(), n * sizeof(float));
  width = w;
  height = h;
  return true;
}
}
