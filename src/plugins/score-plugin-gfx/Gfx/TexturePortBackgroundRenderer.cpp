#include "TexturePortBackgroundRenderer.hpp"

#include <Process/Process.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Settings/Model.hpp>

namespace Gfx
{

class TextureOutletBackgroundRenderer : public score::BackgroundRenderer
{
public:
  TextureOutletBackgroundRenderer(Gfx::TextureOutlet& target, QObject* parent)
      : score::BackgroundRenderer{parent}
  {
    auto& ctx = score::IDocument::documentContext(target);
    plug = ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      GfxExecutionAction& exec = plug->exec;
      auto node = new score::gfx::BackgroundNode2{};
      this->shared_readback = std::make_shared<QRhiReadbackResult>();
      node->shared_readback = this->shared_readback;

      screenId = exec.ui->register_node(std::unique_ptr<score::gfx::Node>{node});
      if(screenId != -1)
      {
        if(target.nodeId != -1)
        {
          nodeId = target.nodeId;
          e = {{nodeId, 0}, {screenId, 0}};
          plug->context.connect_preview_node(*e);
        }
      }
    }

    connect(&target, &Gfx::TextureOutlet::destroyed, this, &QObject::deleteLater);
  }

  ~TextureOutletBackgroundRenderer() override
  {
    if(plug)
    {
      if(e)
      {
        plug->context.disconnect_preview_node(*e);
      }
      plug->context.unregister_node(screenId);
    }
  }

  bool render(QPainter* painter, const QRectF& rect) override
  {
    auto& m_readback = *shared_readback;
    const auto w = m_readback.pixelSize.width();
    const auto h = m_readback.pixelSize.height();
    int sz = w * h * 4;
    int bytes = m_readback.data.size();
    if(bytes > 0 && bytes >= sz)
    {
      QImage img{
          (const unsigned char*)m_readback.data.data(), w, h, w * 4,
          QImage::Format_RGBA8888};
      painter->drawImage(rect, img);
      return true;
    }
    return false;
  }

private:
  QPointer<Gfx::DocumentPlugin> plug;
  std::optional<Gfx::EdgeSpec> e;
  std::shared_ptr<QRhiReadbackResult> shared_readback;
  int32_t screenId{-1};
  int32_t nodeId{-1};
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
