#include <core/view/View.hpp>

#include <Process/ProcessList.hpp>
#include "ProcessPanelFactory.hpp"
#include "ProcessPanelModel.hpp"
#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"

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
        QObject* parent)
{
    return new ProcessPanelView{parent};
}

iscore::PanelPresenter*ProcessPanelFactory::makePresenter(
        const iscore::ApplicationContext& ctx,
        iscore::PanelView* view,
        QObject* parent)
{
    auto& fact = ctx.components.factory<Process::ProcessList>();
    return new ProcessPanelPresenter{fact, view, parent};
}

iscore::PanelModel*ProcessPanelFactory::makeModel(
        const iscore::DocumentContext&,
        QObject* parent)
{
    return new ProcessPanelModel{parent};
}
