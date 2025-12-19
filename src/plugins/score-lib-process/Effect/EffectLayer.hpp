#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

#include <verdigris>

class QAction;
namespace score
{
class QGraphicsDraggablePixmap;
}
namespace Process
{

class SCORE_LIB_PROCESS_EXPORT EffectLayerView : public Process::LayerView
{
public:
  explicit EffectLayerView(QGraphicsItem* parent);
  ~EffectLayerView() override;

  void setWidth(qreal width, qreal defaultWidth);

protected:
  qreal m_defaultWidth{};

private:
  void paint_impl(QPainter*) const override;
};

class SCORE_LIB_PROCESS_EXPORT EffectLayerPresenter
    : public Process::LayerPresenter
{
  W_OBJECT(EffectLayerPresenter)
public:
  EffectLayerPresenter(
      const Process::ProcessModel& model, Process::EffectLayerView* view,
      const Process::Context& ctx, QObject* parent);
  ~EffectLayerPresenter() override;

  void setWidth(qreal width, qreal defaultWidth) override;
  void setHeight(qreal val) override;
  void putToFront() override;
  void putBehind() override;
  void on_zoomRatioChanged(ZoomRatio) override;
  void parentGeometryChanged() override;
  void fillContextMenu(
      QMenu& menu, QPoint pos, QPointF scenepos,
      const Process::LayerContextMenuManager&) override;

protected:
  Process::EffectLayerView* m_view{};
};

SCORE_LIB_PROCESS_EXPORT
QGraphicsItem* makeScriptButton(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, QObject* self,
    QGraphicsItem* parent);

SCORE_LIB_PROCESS_EXPORT
    void setupScriptUI(
        Process::ProcessModel& proc, const Process::LayerFactory& factory,
        const score::DocumentContext& ctx, bool show);

SCORE_LIB_PROCESS_EXPORT
void setupExternalUI(
    Process::ProcessModel& proc, const Process::LayerFactory& factory,
    const score::DocumentContext& ctx, bool show);

SCORE_LIB_PROCESS_EXPORT
void setupExternalUI(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, bool show);

SCORE_LIB_PROCESS_EXPORT
QGraphicsItem* makeExternalUIButton(
    Process::ProcessModel& proc, const score::DocumentContext& ctx, QObject* self,
    QGraphicsItem* parent);

SCORE_LIB_PROCESS_EXPORT
score::QGraphicsDraggablePixmap* makePresetButton(
    const Process::ProcessModel& proc, const score::DocumentContext& ctx, QObject* self,
    QGraphicsItem* parent);

SCORE_LIB_PROCESS_EXPORT
void copyProcess(JSONReader& r, const Process::ProcessModel& proc);
}
