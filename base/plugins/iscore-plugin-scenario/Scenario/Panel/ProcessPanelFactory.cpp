#include <core/document/DocumentModel.hpp>
#include <core/view/View.hpp>

#include <Process/ProcessList.hpp>
#include "ProcessPanelFactory.hpp"
#include "ProcessPanelModel.hpp"
#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <core/presenter/Presenter.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "ProcessPanelId.hpp"

int ProcessPanelFactory::panelId() const
{
    return PROCESS_PANEL_ID;
}

QString ProcessPanelFactory::panelName() const
{
    return "ProcessPanelModel";
}

iscore::PanelView*ProcessPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::View* parent)
{
    return new ProcessPanelView{parent};
}

iscore::PanelPresenter*ProcessPanelFactory::makePresenter(
        iscore::Presenter* parent_presenter,
        iscore::PanelView* view)
{
    auto& fact = parent_presenter->applicationComponents().factory<DynamicProcessList>();
    return new ProcessPanelPresenter{fact, parent_presenter, view};
}

iscore::PanelModel*ProcessPanelFactory::makeModel(iscore::DocumentModel* parent)
{
    return new ProcessPanelModel{parent};
}
