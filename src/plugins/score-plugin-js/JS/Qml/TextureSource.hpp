#pragma once
#if defined(SCORE_HAS_GPU_JS)
#include <Execution/DocumentPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/TexturePort.hpp>

#include <QPointer>
#include <QQuickRhiItem>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QTimer>

#include <private/qrhi_p.h>

#include <memory>
#include <verdigris>

namespace Gfx
{
class DocumentPlugin;
}

namespace score::gfx
{
class Node;
class PreviewNode;
}

namespace JS
{

class TextureSource : public QQuickRhiItem
{
  W_OBJECT(TextureSource)
  QML_ELEMENT

public:
  explicit TextureSource(QQuickItem* parent = nullptr);
  ~TextureSource();

  // QML Properties
  INLINE_PROPERTY_CREF(QString, process, = "", process, setProcess, processChanged)
  INLINE_PROPERTY_CREF(QVariant, port, = "", port, setPort, portChanged)

  QQuickRhiItemRenderer* createRenderer() override;

protected:
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void componentComplete() override;
  void releaseResources() override;

  void handleWindowChanged(QQuickWindow* window);
  void onRenderFrame();

private:
  friend class TextureSourceRenderer;
  void rebuild();
  void do_rebuild();
  bool connectToOutlet();
  void disconnectFromOutlet();
  QRhiTexture* extractTextureFromNode();

  QPointer<Gfx::TextureOutlet> m_outlet{};
  QPointer<Gfx::DocumentPlugin> m_gfxPlugin{};
  QPointer<Execution::DocumentPlugin> m_execPlugin{};
  QPointer<Process::ProcessModel> m_processPtr;

  // For texture extraction from the graph
  bool m_isConnected = false;
  bool m_needsRebuild = false;
  int m_nodeId{-1};
};

}
#endif
