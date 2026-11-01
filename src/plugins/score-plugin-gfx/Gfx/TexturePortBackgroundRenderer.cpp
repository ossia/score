#include "TexturePortBackgroundRenderer.hpp"

#include <Process/Process.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Widgets/TexturePreview.hpp>

#include <QPaintDevice>
#include <QPainter>

namespace Gfx
{

//! What a selected texture outlet produces, behind the editor.
class TextureOutletBackgroundRenderer : public score::BackgroundRenderer
{
public:
  TextureOutletBackgroundRenderer(Gfx::TextureOutlet& target, QObject* parent)
      : score::BackgroundRenderer{parent}
      , m_target{&target}
  {
    auto& ctx = score::IDocument::documentContext(target);
    if(auto* plug = ctx.findPlugin<Gfx::DocumentPlugin>())
      m_preview.attach(&plug->context, target.graphicsPort(), QSize{512, 512});

    connect(&target, &Gfx::TextureOutlet::destroyed, this, &QObject::deleteLater);
  }

  bool render(QPainter* painter, const QRectF& rect) override
  {
    if(!m_target || !m_preview.attached())
      return false;
    // The outlet has a producer only while the process runs, and gets
    // another one when it is instantiated again
    m_preview.setProducer(m_target->graphicsPort());
    // In device pixels; the context bounds it to a thumbnail
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.;
    m_preview.setSize((rect.size() * dpr).toSize());

    const QImage img = m_preview.image();
    if(img.isNull())
      return false;
    painter->drawImage(TexturePreview::fit(rect, img.size()), img);
    return true;
  }

private:
  QPointer<Gfx::TextureOutlet> m_target;
  TexturePreview m_preview;
};

bool TextureOutletBackgroundRendererFactory::matches(
    const Selection& sel, QObject* parent) const noexcept
{
  ossia::small_vector<Process::ProcessModel*, 4> proc;
  for(auto& obj : sel)
  {
    if(qobject_cast<Gfx::TextureOutlet*>(obj.data()))
      return true;
    if(auto p = qobject_cast<Process::ProcessModel*>(obj.data()))
      proc.push_back(p);
  }

  for(auto p : proc)
  {
    for(auto& out : p->outlets())
    {
      if(qobject_cast<Gfx::TextureOutlet*>(out))
        return true;
    }
  }
  return false;
}

score::BackgroundRenderer*
TextureOutletBackgroundRendererFactory::make(const Selection& sel, QObject* parent) const
{
  TextureOutlet* target{};
  ossia::small_vector<Process::ProcessModel*, 4> proc;
  for(auto& obj : sel)
  {
    if((target = qobject_cast<Gfx::TextureOutlet*>(obj.data())))
    {
      break;
    }
    if(auto p = qobject_cast<Process::ProcessModel*>(obj.data()))
      proc.push_back(p);
  }

  if(!target)
  {
    for(auto p : proc)
    {
      for(auto& out : p->outlets())
      {
        if((target = qobject_cast<Gfx::TextureOutlet*>(out)))
          break;
      }
      if(target)
        break;
    }
  }

  if(!target)
    return nullptr;

  return new TextureOutletBackgroundRenderer{*target, parent};
}

TextureOutletBackgroundRendererFactory::~TextureOutletBackgroundRendererFactory()
    = default;
}
