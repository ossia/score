#include "InspectorPanelFactory.hpp"
#include "InspectorPanelView.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelPresenter.hpp"
#include "InspectorPanelId.hpp"
using namespace iscore;


int InspectorPanelFactory::panelId() const
{
    return INSPECTOR_PANEL_ID;
}

QString InspectorPanelFactory::panelName() const
{
    return "Inspector";
}

iscore::PanelView* InspectorPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::View* parent)
{
    return new InspectorPanelView {parent};
}

iscore::PanelPresenter* InspectorPanelFactory::makePresenter(
    iscore::Presenter* parent_presenter,
    iscore::PanelView* view)
{
    return new InspectorPanelPresenter {parent_presenter, view};
}

iscore::PanelModel* InspectorPanelFactory::makeModel(iscore::DocumentModel* parent)
{
    return new InspectorPanelModel {parent};
}

