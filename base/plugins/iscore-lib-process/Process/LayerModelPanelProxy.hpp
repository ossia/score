#pragma once
#include <QObject>
#include <iscore_lib_process_export.h>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <QGraphicsScene>
#include <QQuickWidget>
class ProcessGraphicsView;
namespace iscore
{
class DoubleSlider;
}
class ProcessPanelGraphicsProxy;
namespace Process
{
class LayerModel;
class LayerPresenter;
class LayerView;
}
namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT LayerModelPanelProxy : public QObject
{
public:
  using QObject::QObject;
  virtual ~LayerModelPanelProxy();

  // Can return the same view model, or a new one.
  virtual const LayerModel& layer() = 0;
  virtual QWidget* widget() const = 0;
};

class ISCORE_LIB_PROCESS_EXPORT GraphicsViewLayerModelPanelProxy
    : public LayerModelPanelProxy
{
public:
  GraphicsViewLayerModelPanelProxy(const LayerModel& model, QObject* parent);

  virtual ~GraphicsViewLayerModelPanelProxy();

  // Can return the same view model, or a new one.
  const LayerModel& layer() final override;
  QWidget* widget() const final override;

  void on_sizeChanged(const QSize& size);
  void on_zoomChanged(ZoomRatio newzoom);

private:
  void recompute();

  double m_height{}, m_width{};

  const LayerModel& m_layer;
  QWidget* m_widget{};

  QGraphicsScene* m_scene{};
  ProcessGraphicsView* m_view{};
  ProcessPanelGraphicsProxy* m_obj{};
  iscore::DoubleSlider* m_zoomSlider{};

  Process::LayerPresenter* m_processPresenter{};
  Process::LayerView* m_layerView{};

  FocusDispatcher m_dispatcher;
  ProcessPresenterContext m_context;
  ZoomRatio m_zoomRatio{};
};
}
