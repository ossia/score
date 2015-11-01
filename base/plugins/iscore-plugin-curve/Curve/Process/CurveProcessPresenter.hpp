#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include "CurveProcessModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/GraphicsItem.hpp>
#include <core/document/Document.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveView.hpp"
#include "Curve/StateMachine/CurveStateMachine.hpp"


class CurvePresenter;
class QCPGraph;
class LayerView;
class CurveProcessView;


template<typename LayerModel_T, typename LayerView_T>
class CurveProcessPresenter : public LayerPresenter
{
    public:
        CurveProcessPresenter(
                const CurveStyle& style,
                const LayerModel_T& lm,
                LayerView_T* view,
                QObject* parent) :
            LayerPresenter {"CurveProcessPresenter", parent},
            m_layer{lm},
            m_view{static_cast<LayerView_T*>(view)},
            m_commandDispatcher{iscore::IDocument::commandStack(m_layer.processModel())},
            m_focusDispatcher{*iscore::IDocument::documentFromObject(m_layer.processModel())}
        {
            con(m_layer.model(), &CurveProcessModel::curveChanged,
                this, &CurveProcessPresenter::parentGeometryChanged);

            auto cv = new CurveView{m_view};
            m_curvepresenter = new CurvePresenter{style, m_layer.model().curve(), cv, this};

            connect(cv, &CurveView::pressed,
                    this, [&] (const QPointF&)
            {
                m_focusDispatcher.focus(this);
            });
            connect(m_curvepresenter, &CurvePresenter::contextMenuRequested,
                    this, &LayerPresenter::contextMenuRequested);

            con(m_layer.model(), &Process::execution,
                this, [&] (bool b) {
                setCurveStateMachineStatus(!b);
            });


            parentGeometryChanged();
        }

        ~CurveProcessPresenter()
        {
            deleteGraphicsObject(m_view);
        }

        void on_focusChanged() override
        {
            bool b = focused();
            m_curvepresenter->enableActions(b);
            setCurveStateMachineStatus(b);
        }


        void setWidth(int width) override
        {
            m_view->setWidth(width);
        }

        void setHeight(int height) override
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

        const Id<Process>& modelId() const override
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

        void setCurveStateMachineStatus(bool run)
        {
            auto& sm = m_curvepresenter->stateMachine();
            if(run)
            {
                if(!sm.isRunning())
                    sm.start();
            }
            else
            {
                if(sm.isRunning())
                {
                    sm.stop();
                    sm.setMoveState();
                }
            }
        }

    protected:
        const LayerModel_T& m_layer;
        LayerView_T* m_view{};

        CurvePresenter* m_curvepresenter{};

        CommandDispatcher<> m_commandDispatcher;
        FocusDispatcher m_focusDispatcher;

        ZoomRatio m_zoomRatio {};
};

