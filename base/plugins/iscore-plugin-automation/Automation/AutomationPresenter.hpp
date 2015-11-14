#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include "AutomationModel.hpp"
#include "AutomationLayerModel.hpp"
#include "AutomationView.hpp"
class AutomationPresenter final :
        public CurveProcessPresenter<
            AutomationLayerModel,
            AutomationView>
{
    public:
        AutomationPresenter(
                Curve::EditionSettings& set,
                const CurveStyle& style,
                const AutomationLayerModel& layer,
                AutomationView* view,
                QObject* parent):
            CurveProcessPresenter{set, style, layer, view, parent}
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
