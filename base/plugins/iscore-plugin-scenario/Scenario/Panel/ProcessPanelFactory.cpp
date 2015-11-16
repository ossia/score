#include "ProcessPanelFactory.hpp"
#include "ProcessPanelModel.hpp"
#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include "ProcessPanelId.hpp"
#include <Process/ProcessFactory.hpp>

#include <core/view/View.hpp>
#include <core/document/DocumentModel.hpp>

int ProcessPanelFactory::panelId() const
{
    return PROCESS_PANEL_ID;
}

QString ProcessPanelFactory::panelName() const
{
    return "ProcessPanelModel";
}

iscore::PanelView*ProcessPanelFactory::makeView(iscore::View* parent)
{
    return new ProcessPanelView{parent};
}

iscore::PanelPresenter*ProcessPanelFactory::makePresenter(
        iscore::Presenter* parent_presenter,
        iscore::PanelView* view)
{
    auto fact = parent_presenter->applicationComponents().factory<DynamicProcessList>();
    ISCORE_ASSERT(fact);
    return new ProcessPanelPresenter{*fact, parent_presenter, view};
}

iscore::PanelModel*ProcessPanelFactory::makeModel(iscore::DocumentModel* parent)
{
    return new ProcessPanelModel{parent};
}
