#pragma once
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Process/CurveProcessModel.hpp>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

#include <Curve/Segment/CurveSegmentList.hpp>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <iscore_plugin_curve_export.h>

class CurvePresenter;
class LayerView;
class CurveProcessView;

namespace Curve
{
template <typename LayerModel_T, typename LayerView_T>
class CurveProcessPresenter : public Process::LayerPresenter
{
public:
  CurveProcessPresenter(
      const Curve::Style& style,
      const LayerModel_T& lm,
      LayerView_T* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent)
      : LayerPresenter{ctx, parent}
      , m_layer{lm}
      , m_view{static_cast<LayerView_T*>(view)}
      , m_curvepresenter{new Presenter{ctx, style,
                                       m_layer.processModel().curve(),
                                       new View{m_view}, this}}
      , m_commandDispatcher{ctx.commandStack}
      , m_sm{m_context, *m_curvepresenter}
  {
    con(m_layer.processModel(), &CurveProcessModel::curveChanged, this,
        &CurveProcessPresenter::parentGeometryChanged);

    connect(
        m_curvepresenter, &Presenter::contextMenuRequested, this,
        &LayerPresenter::contextMenuRequested);

    con(m_layer.processModel(), &Process::ProcessModel::execution, this,
        [&](bool b) {
          m_curvepresenter->editionSettings().setTool(
              b ? Curve::Tool::Playing
                : focused() ? Curve::Tool::Select : Curve::Tool::Disabled);
        });

    connect(&m_curvepresenter->view(), &View::doubleClick,
            this, [this] (QPointF pt) { m_sm.createPoint(pt); });

    parentGeometryChanged();
  }

  virtual ~CurveProcessPresenter()
  {
    delete m_curvepresenter;
  }

  void on_focusChanged() override
  {
    bool b = focused();
    if (b)
      m_view->setFocus();

    // TODO Same for Scenario please.
    m_curvepresenter->enableActions(b);

    // TODO if playing() ?
    m_curvepresenter->editionSettings().setTool(Curve::Tool::Select);
  }

  void setWidth(qreal width) override
  {
    m_view->setWidth(width);
  }

  void setHeight(qreal height) override
  {
    m_view->setHeight(height);
  }

  void putToFront() override
  {
    m_view->setFlag(QQuickPaintedItem::ItemStacksBehindParent, false);
    m_curvepresenter->enable();
    m_view->showName(true);
  }

  void putBehind() override
  {
    m_view->setFlag(QQuickPaintedItem::ItemStacksBehindParent, true);
    m_curvepresenter->disable();
    m_view->showName(false);
  }

  void on_zoomRatioChanged(ZoomRatio val) override
  {
    m_zoomRatio = val;
    // TODO REDRAW??? or parentGeometryChanged called afterwards?
  }

  void parentGeometryChanged() override
  {
    // Compute the rect with the duration of the process.
    QRectF rect = m_view->boundingRect(); // for the height

    rect.setWidth(m_layer.processModel().duration().toPixels(m_zoomRatio));

    m_curvepresenter->view().setRect(rect);
    m_curvepresenter->setRect(rect);
  }

  const LayerModel_T& layerModel() const override
  {
    return m_layer;
  }

  const Id<Process::ProcessModel>& modelId() const override
  {
    return m_layer.processModel().id();
  }

  void fillContextMenu(
      QMenu& menu,
      QPoint pos,
      QPointF scenepos,
      const Process::LayerContextMenuManager&) const override
  {
    m_curvepresenter->fillContextMenu(menu, pos, scenepos);
  }


protected:
  const LayerModel_T& m_layer;
  graphics_item_ptr<LayerView_T> m_view;

  Presenter* m_curvepresenter{};

  CommandDispatcher<> m_commandDispatcher;

  ZoomRatio m_zoomRatio{};

  Curve::ToolPalette_T<Process::LayerContext> m_sm;
};
}
