
#if defined(SCORE_HAS_GPU_JS)
#include "TextureSource.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Execution/Settings/ExecutorModel.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/PreviewNode.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QPainter>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QtGui/private/qrhi_p.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::TextureSource)

namespace JS
{
class TextureSourceRenderer : public QQuickRhiItemRenderer
{
public:
  void initialize(QRhiCommandBuffer* cb) override;
  void synchronize(QQuickRhiItem* item) override;
  void render(QRhiCommandBuffer* cb) override;
  void rebuild();
  void clear()
  {
    if(m_nodeId < 0)
      return;
    if(m_screenId < 0)
      return;
    if(!item)
      return;
    if(!item->m_gfxPlugin)
      return;

    auto& graph = item->m_gfxPlugin->context;
    // fixme clear m_extractionNode
    graph.disconnect_preview_node(Gfx::EdgeSpec{{m_nodeId, 0}, {m_screenId, 0}});
    graph.unregister_preview_node(m_screenId);
    m_nodeId = -1;
    m_screenId = -1;
    m_extractionNode_p = nullptr;
    m_extractionNode.reset();
  }

private:
  std::unique_ptr<score::gfx::OutputNode> m_extractionNode;
  score::gfx::OutputNode* m_extractionNode_p{};

  QRhi* m_rhi = nullptr;
  int m_sampleCount = 1;
  int m_nodeId = -1;
  int m_screenId = -1;
  TextureSource* item{};
  bool m_needsRebuild{};
};

QQuickRhiItemRenderer* TextureSource::createRenderer()
{
  return new TextureSourceRenderer;
}

void TextureSourceRenderer::synchronize(QQuickRhiItem* rhiItem)
{
  auto new_item = static_cast<TextureSource*>(rhiItem);
  m_needsRebuild |= (new_item != item);
  item = new_item;
  m_needsRebuild |= (item->m_nodeId != m_nodeId);
}

void TextureSourceRenderer::rebuild()
{
  bool changed = m_needsRebuild;
  if(m_rhi != rhi())
  {
    m_rhi = rhi();
    changed = true;
  }

  if(m_sampleCount != renderTarget()->sampleCount())
  {
    m_sampleCount = renderTarget()->sampleCount();
    changed = true;
  }

  if(!item)
  {
    clear();
    return;
  }
  if(item->m_nodeId == -1)
  {
    clear();
    return;
  }
  if(item->m_nodeId != m_nodeId)
    changed = true;

  QRhiTexture* finalTex = m_sampleCount > 1 ? resolveTexture() : colorTexture();

  // Find the node in the graph
  auto& graph = item->m_gfxPlugin->context;

  if(changed)
  {
    clear();
  }

  if(!m_extractionNode_p)
  {
    Gfx::SharedOutputSettings set;
    set.width = finalTex->pixelSize().width();
    set.height = finalTex->pixelSize().height();
    set.rate = 60;
    m_extractionNode = std::make_unique<score::gfx::PreviewNode>(
        set, m_rhi, this->renderTarget(), finalTex);
    m_extractionNode_p = m_extractionNode.get();
    m_screenId = graph.register_preview_node(std::move(m_extractionNode));
    if(m_screenId != -1)
    {
      m_nodeId = item->m_nodeId;
      graph.connect_preview_node(Gfx::EdgeSpec{{m_nodeId, 0}, {m_screenId, 0}});
      m_needsRebuild = false;
    }
  }
}

void TextureSourceRenderer::initialize(QRhiCommandBuffer* cb)
{
  m_needsRebuild = true;
  rebuild();
}

void TextureSourceRenderer::render(QRhiCommandBuffer* cb)
{
  rebuild();
  if(m_extractionNode_p)
  {
    auto r = m_extractionNode_p->renderer();
    if(r)
    {
      r->render(*cb);
    }
  }
  update();
}

TextureSource::TextureSource(QQuickItem* parent)
    : QQuickRhiItem{parent}
{
  // Enable rendering
  setFlag(ItemHasContents, true);

  // Set up update timer for continuous rendering
  // Connect property changes
  connect(this, &TextureSource::processChanged, this, &TextureSource::rebuild);
  connect(this, &TextureSource::portChanged, this, &TextureSource::rebuild);

  // Handle window changes
  connect(this, &QQuickItem::windowChanged, this, &TextureSource::handleWindowChanged);
}

TextureSource::~TextureSource()
{
  disconnectFromOutlet();
}

void TextureSource::componentComplete()
{
  QQuickItem::componentComplete();
  rebuild();
}

void TextureSource::handleWindowChanged(QQuickWindow* window)
{
  rebuild();
}

void TextureSource::releaseResources() { }

void TextureSource::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
  QQuickItem::geometryChange(newGeometry, oldGeometry);

  if(newGeometry.size() != oldGeometry.size())
  {
    // Size changed, we might need to update our rendering
    update();
  }
}

void TextureSource::rebuild()
{
  // Debounce
  m_needsRebuild = true;
  QTimer::singleShot(1, this, [this] {
    if(!m_needsRebuild)
      return;
    m_needsRebuild = false;
    do_rebuild();
  });
}

void TextureSource::do_rebuild()
{
  disconnectFromOutlet();
  update();
  m_outlet = nullptr;

  if(!window())
    return;

  auto doc = score::GUIAppContext().documents.currentDocument();
  if(!doc)
    return;

  // Find the plugins we need
  m_gfxPlugin = doc->context().findPlugin<Gfx::DocumentPlugin>();
  if(!m_gfxPlugin)
    return;
  m_execPlugin = doc->context().findPlugin<Execution::DocumentPlugin>();
  if(!m_execPlugin)
    return;

  constexpr auto rebuild_connect_flags
      = (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection);
  connect(
      &m_execPlugin->executionController().transport(),
      &Execution::TransportInterface::play, this, &TextureSource::rebuild,
      rebuild_connect_flags);
  connect(
      &m_execPlugin->executionController().transport(),
      &Execution::TransportInterface::stop, this, &TextureSource::rebuild,
      rebuild_connect_flags);

  if(!m_execPlugin->isPlaying())
    return;

  // Find the process
  auto& model = doc->model().modelDelegate();
  const auto processes = model.findChildren<Process::ProcessModel*>(
      QString{}, Qt::FindChildrenRecursively);

  for(const auto& proc : processes)
  {
    if(proc->metadata().getLabel() == m_process
       || proc->metadata().getName() == m_process)
    {
      m_processPtr = proc;
      break;
    }
  }

  if(!m_processPtr)
  {
    return;
  }
  connect(
      m_processPtr, &Process::ProcessModel::startExecution, this,
      &TextureSource::rebuild, rebuild_connect_flags);
  connect(
      m_processPtr, &Process::ProcessModel::stopExecution, this, &TextureSource::rebuild,
      rebuild_connect_flags);
  connect(
      m_processPtr, &Process::ProcessModel::resetExecution, this,
      &TextureSource::rebuild, rebuild_connect_flags);

  if(!m_processPtr->executing())
    return;

  // Find the texture outlet
  if(m_port.typeId() == QMetaType::Type::QString)
  {
    auto port_name = m_port.value<QString>();
    m_outlet = m_processPtr->findChild<Gfx::TextureOutlet*>(port_name);
    if(!m_outlet)
    {
      auto& outlets = m_processPtr->outlets();
      for(auto& outl : outlets)
      {
        if(outl->name() == port_name)
        {
          m_outlet = qobject_cast<Gfx::TextureOutlet*>(outl);
          break;
        }
      }
    }
  }
  else if(m_port.typeId() == QMetaType::Type::Int)
  {
    int i = m_port.toInt();
    auto& outlets = m_processPtr->outlets();
    if(i >= 0 && i < outlets.size())
    {
      m_outlet = qobject_cast<Gfx::TextureOutlet*>(outlets[i]);
    }
  }

  if(m_outlet)
  {
    auto res = connectToOutlet();
    if(!res)
      QTimer::singleShot(16, this, &TextureSource::rebuild);
    else
      update();
  }
}

bool TextureSource::connectToOutlet()
{
  m_nodeId = -1;
  if(!m_outlet || !m_gfxPlugin)
  {
    return false;
  }

  // Get the node ID from the outlet
  m_nodeId = m_outlet->nodeId;
  if(m_nodeId < 0)
  {
    return false;
  }
  return true;
}

void TextureSource::disconnectFromOutlet()
{
  if(m_processPtr)
  {
    disconnect(
        m_processPtr, &Process::ProcessModel::startExecution, this,
        &TextureSource::rebuild);
    disconnect(
        m_processPtr, &Process::ProcessModel::stopExecution, this,
        &TextureSource::rebuild);
    disconnect(
        m_processPtr, &Process::ProcessModel::resetExecution, this,
        &TextureSource::rebuild);
    m_processPtr = nullptr;
  }
  m_nodeId = -1;
}
}
#endif
