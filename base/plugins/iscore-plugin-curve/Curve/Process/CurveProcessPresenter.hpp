#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurvePalette.hpp>

#include <Process/LayerPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

#include <Curve/Segment/CurveSegmentList.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <iscore_plugin_curve_export.h>

class CurvePresenter;
class LayerView;
class CurveProcessView;


template<typename LayerModel_T, typename LayerView_T>
class ISCORE_PLUGIN_CURVE_EXPORT CurveProcessPresenter :
        public Process::LayerPresenter
{
    public:
        CurveProcessPresenter(
                const iscore::DocumentContext& context,
                const Curve::Style& style,
                const LayerModel_T& lm,
                LayerView_T* view,
                QObject* parent) :
            LayerPresenter {"CurveProcessPresenter", parent},
            m_layer{lm},
            m_view{static_cast<LayerView_T*>(view)},
            m_curvepresenter{new CurvePresenter{context, style, m_layer.model().curve(), new CurveView{m_view}, this}},
            m_commandDispatcher{context.commandStack},
            m_focusDispatcher{context.document},
            m_context{context, *this, m_focusDispatcher},
            m_sm{m_context, *m_curvepresenter}
        {
            con(m_layer.model(), &CurveProcessModel::curveChanged,
                this, &CurveProcessPresenter::parentGeometryChanged);

            connect(m_curvepresenter, &CurvePresenter::contextMenuRequested,
                    this, &LayerPresenter::contextMenuRequested);

            con(m_layer.model(), &Process::ProcessModel::execution,
                this, [&] (bool b) {
                m_curvepresenter->editionSettings().setTool(
                            b ? Curve::Tool::Playing
                              : focused() ? Curve::Tool::Select
                                          : Curve::Tool::Disabled);
            });

            parentGeometryChanged();
        }

        virtual ~CurveProcessPresenter()
        {
            deleteGraphicsObject(m_view);
        }

        void on_focusChanged() override
        {
            bool b = focused();
            // TODO Same for Scenario please.
            m_curvepresenter->enableActions(b);
            // TODO if playing() ?
            m_curvepresenter->editionSettings().setTool(b ? Curve::Tool::Select
                                                          : Curve::Tool::Disabled);
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
            m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
            m_curvepresenter->enable();
            m_view->showName(true);
        }

        void putBehind() override
        {
            m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
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

            rect.setWidth(m_layer.model().duration().toPixels(m_zoomRatio));

            m_curvepresenter->setRect(rect);
        }

        const LayerModel_T& layerModel() const override
        {
            return m_layer;
        }

        const Id<Process::ProcessModel>& modelId() const override
        {
            return m_layer.model().id();
        }

        void fillContextMenu(
                QMenu* menu,
                const QPoint& pos,
                const QPointF& scenepos) const override
        {
            m_curvepresenter->fillContextMenu(menu, pos, scenepos);
        }

    protected:
        const LayerModel_T& m_layer;
        LayerView_T* m_view{};

        CurvePresenter* m_curvepresenter{};

        CommandDispatcher<> m_commandDispatcher;
        FocusDispatcher m_focusDispatcher;

        ZoomRatio m_zoomRatio {};

        LayerContext m_context;
        Curve::ToolPalette m_sm;
};

