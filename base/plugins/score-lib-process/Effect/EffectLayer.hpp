#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

class QAction;
namespace Process
{

class SCORE_LIB_PROCESS_EXPORT EffectLayerView final : public Process::LayerView
{
public:
  EffectLayerView(QGraphicsItem* parent);
  ~EffectLayerView() override;

private:
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;
};

class SCORE_LIB_PROCESS_EXPORT EffectLayerPresenter final : public Process::LayerPresenter
{
public:
  EffectLayerPresenter(
      const Process::ProcessModel& model,
      EffectLayerView* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent);
  ~EffectLayerPresenter() override;

  void setWidth(qreal val) override;
  void setHeight(qreal val) override;
  void putToFront() override;
  void putBehind() override;
  void on_zoomRatioChanged(ZoomRatio) override;
  void parentGeometryChanged() override;
  const Process::ProcessModel& model() const override;
  const Id<Process::ProcessModel>& modelId() const override;
  void fillContextMenu(
      QMenu& menu,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&) final override;

private:
  const Process::ProcessModel& m_layer;
  EffectLayerView* m_view{};
  QAction* m_showUI{};
};


}
