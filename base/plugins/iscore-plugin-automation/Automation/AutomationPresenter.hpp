#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationView.hpp>

#include <Process/ProcessContext.hpp>

namespace Automation
{
class LayerPresenter final :
        public Curve::CurveProcessPresenter<
            LayerModel,
            LayerView>
{
    public:
        LayerPresenter(
                const iscore::DocumentContext& context,
                const Curve::Style& style,
                const LayerModel& layer,
                LayerView* view,
                QObject* parent):
            CurveProcessPresenter{context, style, layer, view, parent}
        {
            con(m_layer.model(), &ProcessModel::addressChanged,
                this, [&] (const auto&)
            {
                m_view->setDisplayedName(m_layer.model().prettyName());
            });
            con(m_layer.model().metadata, &ModelMetadata::nameChanged,
                this, [&] (QString s)
            {
                m_view->setDisplayedName(s);
            });

            m_view->setDisplayedName(m_layer.model().prettyName());
            m_view->showName(true);
        }
};
}
