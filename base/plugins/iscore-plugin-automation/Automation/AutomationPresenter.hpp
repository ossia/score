#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationView.hpp>

#include <Process/ProcessContext.hpp>

class AutomationPresenter final :
        public CurveProcessPresenter<
            AutomationLayerModel,
            AutomationView>
{
    public:
        AutomationPresenter(
                iscore::DocumentContext& context,
                Curve::EditionSettings& set,
                const Curve::Style& style,
                const AutomationLayerModel& layer,
                AutomationView* view,
                QObject* parent):
            CurveProcessPresenter{context, set, style, layer, view, parent}
        {
            con(m_layer.model(), &AutomationModel::addressChanged,
                this, [&] (const auto&)
            {
                m_view->setDisplayedName(m_layer.model().prettyName());
            });

            m_view->setDisplayedName(m_layer.model().prettyName());
            m_view->showName(true);
        }
};
