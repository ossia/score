#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <JS/Qml/EditContext.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>

#include <QObject>
#include <QPointF>

#include <score_plugin_js_export.h>

#include <nano_signal_slot.hpp>
#include <verdigris>

namespace Scenario
{
class IntervalModel;
class ScenarioDocumentModel;
}
namespace Nodal
{
class Model;
}

namespace JS
{

class SCORE_PLUGIN_JS_EXPORT PatcherContext
    : public QObject
    , public Nano::Observer
{
  W_OBJECT(PatcherContext)

public:
  PatcherContext(
      QObject* container, const score::DocumentContext& ctx, QObject* parent = nullptr);
  ~PatcherContext();

  QObject* container() const noexcept { return m_container; }
  void setContainer(QObject* c);

  QObjectList nodes() const;
  QObjectList cables() const;

  QPointF panOffset() const noexcept { return m_panOffset; }
  void setPanOffset(QPointF p);

  double zoom() const noexcept { return m_zoom; }
  void setZoom(double z);

  EditJsContext* editContext() const noexcept { return m_editCtx; }
  const score::DocumentContext& documentContext() const noexcept { return m_ctx; }

  // Signals
  void nodesChanged() W_SIGNAL(nodesChanged);
  void cablesChanged() W_SIGNAL(cablesChanged);
  void containerChanged() W_SIGNAL(containerChanged);
  void panOffsetChanged(QPointF p) W_SIGNAL(panOffsetChanged, p);
  void zoomChanged(double z) W_SIGNAL(zoomChanged, z);

  PROPERTY(QObject*, container W_READ container W_WRITE setContainer W_NOTIFY containerChanged)
  PROPERTY(QObjectList, nodes W_READ nodes W_NOTIFY nodesChanged)
  PROPERTY(QObjectList, cables W_READ cables W_NOTIFY cablesChanged)
  PROPERTY(
      QPointF,
      panOffset W_READ panOffset W_WRITE setPanOffset W_NOTIFY panOffsetChanged)
  PROPERTY(double, zoom W_READ zoom W_WRITE setZoom W_NOTIFY zoomChanged)
  W_PROPERTY(EditJsContext*, editContext W_READ editContext W_CONSTANT)

private:
  void connectToContainer();
  void disconnectFromContainer();

  // Nano signal handlers
  void on_nodeAdded(Process::ProcessModel&);
  void on_nodeRemoving(const Process::ProcessModel&);
  void on_cableAdded(Process::Cable&);
  void on_cableRemoving(const Process::Cable&);

  QObject* m_container{};
  const score::DocumentContext& m_ctx;
  EditJsContext* m_editCtx{};
  QPointF m_panOffset{};
  double m_zoom{1.0};
};

}
