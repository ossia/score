#include "GroupPanelFactory.hpp"

#include "GroupPanelModel.hpp"
#include "GroupPanelPresenter.hpp"
#include "GroupPanelView.hpp"
#include "GroupPanelId.hpp"

#include <core/view/View.hpp>
// TODO review if it is really useful to make the panel view with iscore::View

int GroupPanelFactory::panelId() const
{
    return GROUP_PANEL_ID;
}

QString GroupPanelFactory::panelName() const
{
    return "Groups";
}

iscore::PanelView* GroupPanelFactory::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::View* v)
{
    return new GroupPanelView{v};
}

iscore::PanelPresenter* GroupPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
                                                                 iscore::PanelView* view)
{
    return new GroupPanelPresenter{parent_presenter, view};
}

iscore::PanelModel* GroupPanelFactory::makeModel(iscore::DocumentModel* m)
{
    return new GroupPanelModel{m};
}
