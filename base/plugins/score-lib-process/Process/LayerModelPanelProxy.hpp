#pragma once
#include <QObject>
#include <score_lib_process_export.h>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <QGraphicsScene>
#include <QGraphicsView>
class ProcessGraphicsView;
namespace score
{
class DoubleSlider;
}
class ProcessPanelGraphicsProxy;
namespace Process
{
class LayerPresenter;
class LayerView;
}
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT LayerPanelProxy : public QObject
{
public:
  using QObject::QObject;
  virtual ~LayerPanelProxy();

  // Can return the same view model, or a new one.
  virtual const Process::ProcessModel& layer() = 0;
  virtual QWidget* widget() const = 0;
};

class SCORE_LIB_PROCESS_EXPORT GraphicsViewLayerPanelProxy
    : public LayerPanelProxy
{
public:
  GraphicsViewLayerPanelProxy(const Process::ProcessModel& model, QObject* parent);

  virtual ~GraphicsViewLayerPanelProxy();

  // Can return the same view model, or a new one.
  const Process::ProcessModel& layer() final override;
  QWidget* widget() const final override;

  void on_sizeChanged(const QSize& size);
  void on_zoomChanged(ZoomRatio newzoom);

private:
  void recompute();

  double m_height{}, m_width{};

  const Process::ProcessModel& m_layer;
  QWidget* m_widget{};

  QGraphicsScene* m_scene{};
  ProcessGraphicsView* m_view{};
  ProcessPanelGraphicsProxy* m_obj{};
  score::DoubleSlider* m_zoomSlider{};

  Process::LayerPresenter* m_processPresenter{};
  Process::LayerView* m_layerView{};

  FocusDispatcher m_dispatcher;
  ProcessPresenterContext m_context;
  ZoomRatio m_zoomRatio{};
};
}
