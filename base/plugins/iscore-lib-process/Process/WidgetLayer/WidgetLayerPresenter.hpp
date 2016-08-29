#pragma once
#include <Process/LayerPresenter.hpp>
#include <QPoint>

#include <Process/ZoomHelper.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_process_export.h>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>

#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>
#include <Process/LayerPresenter.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/document/DocumentContext.hpp>

namespace WidgetLayer
{
template<typename Process_T, typename Widget_T>
class Presenter final :
        public Process::LayerPresenter
{
    public:

        explicit Presenter(
                const Process::LayerModel& model,
                View* view,
                const Process::ProcessPresenterContext& ctx,
                QObject* parent):
            LayerPresenter{ctx, parent},
            m_layer{model},
            m_view{view}
        {
            putToFront();
            connect(view, &View::pressed,
                    this, [&] () {
                m_context.context.focusDispatcher.focus(this);
            });

            m_view->setWidget(new Widget_T{static_cast<const Process_T&>(m_layer.processModel()), ctx, nullptr});
        }

        void setWidth(qreal val) override
        {
            m_view->setWidth(val);
        }
        void setHeight(qreal val) override
        {
            m_view->setHeight(val);
        }

        void putToFront() override
        {
            m_view->setVisible(true);
        }

        void putBehind() override
        {
            m_view->setVisible(false);
        }

        void on_zoomRatioChanged(ZoomRatio) override
        {
        }

        void parentGeometryChanged() override
        {
        }

        const Process::LayerModel& layerModel() const override
        {
            return m_layer;
        }
        const Id<Process::ProcessModel>& modelId() const override
        {
            return m_layer.processModel().id();
        }

    private:
        const Process::LayerModel& m_layer;
        View* m_view{};
};
}
