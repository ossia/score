#pragma once
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Process/LayerView.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Process/CurveProcessModel.hpp>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/widgets/GraphicsItem.hpp>

#include <Curve/Segment/CurveSegmentList.hpp>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <score_plugin_curve_export.h>

class CurvePresenter;
class LayerView;
class CurveProcessView;

namespace Curve
{
template <typename Model_T, typename LayerView_T>
class CurveProcessPresenter : public Process::LayerPresenter
{
public:
  CurveProcessPresenter(
      const Curve::Style& style,
      const Model_T& lm,
      LayerView_T* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent)
      : LayerPresenter{ctx, parent}
      , m_layer{lm}
      , m_view{static_cast<LayerView_T*>(view)}
      , m_curve{ctx, style, m_layer.curve(), new View{m_view}, this}
      , m_commandDispatcher{ctx.commandStack}
      , m_sm{m_context, m_curve}
  {
    con(m_layer, &CurveProcessModel::curveChanged, this,
        &CurveProcessPresenter::parentGeometryChanged);

    connect(m_view.impl, &Process::LayerView::pressed, this, [&]() {
      m_context.context.focusDispatcher.focus(this);
    });

    con(
        m_curve, &Presenter::contextMenuRequested, this,
        &LayerPresenter::contextMenuRequested);

    connect(&m_curve.view(), &View::doubleClick,
            this, [this] (QPointF pt) { m_sm.createPoint(pt); });

    parentGeometryChanged();
  }

  virtual ~CurveProcessPresenter()
  {
  }

  void on_focusChanged() final override
  {
    bool b = focused();
    if (b)
      m_view->setFocus();

    // TODO Same for Scenario please.
    m_curve.enableActions(b);

    // TODO if playing() ?
    m_curve.editionSettings().setTool(Curve::Tool::Select);
  }

  void setWidth(qreal width) final override
  {
    m_view->setWidth(width);
    m_curve.view().setRect(m_view->boundingRect());
  }

  void setHeight(qreal height) final override
  {
    m_view->setHeight(height);
    m_curve.view().setRect(m_view->boundingRect());
  }

  void putToFront() final override
  {
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
    m_curve.enable();
  }

  void putBehind() final override
  {
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    m_curve.disable();
  }

  void on_zoomRatioChanged(ZoomRatio val) final override
  {
    m_zoomRatio = val;
    parentGeometryChanged();
  }

  void parentGeometryChanged() final override
  {
    // Compute the rect with the duration of the process.
    QRectF rect = m_view->boundingRect(); // for the height
    m_curve.view().setRect(rect);

    rect.setWidth(m_layer.duration().toPixels(m_zoomRatio));
    m_curve.setRect(rect);
  }

  const Model_T& model() const final override
  {
    return m_layer;
  }

  const Id<Process::ProcessModel>& modelId() const final override
  {
    return m_layer.id();
  }

  void fillContextMenu(
      QMenu& menu,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&) final override
  {
    m_curve.fillContextMenu(menu, pos, scenepos);
  }


protected:
  const Model_T& m_layer;
  graphics_item_ptr<LayerView_T> m_view;

  Presenter m_curve;
  CommandDispatcher<> m_commandDispatcher;

  Curve::ToolPalette_T<Process::LayerContext> m_sm;
  ZoomRatio m_zoomRatio{};
};
}
