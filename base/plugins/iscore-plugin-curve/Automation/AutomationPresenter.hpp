#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include "AutomationModel.hpp"
#include "AutomationLayerModel.hpp"
#include "AutomationView.hpp"
class AutomationPresenter :
        public CurveProcessPresenter<
            AutomationLayerModel,
            AutomationView>
{
    public:
AutomationPresenter(
        const AutomationLayerModel& layer,
        AutomationView* view,
        QObject* parent):
    CurveProcessPresenter{layer, view, parent}
{
    con(m_layer.model(), &AutomationModel::addressChanged,
        this, [&] (const auto&)
    {
        m_view->setDisplayedName(m_layer.model().userFriendlyDescription());
    });

    m_view->setDisplayedName(m_layer.model().userFriendlyDescription());
    m_view->showName(true);
}
};
