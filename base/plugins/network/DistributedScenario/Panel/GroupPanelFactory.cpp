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

iscore::PanelViewInterface*GroupPanelFactory::makeView(iscore::View* v)
{
    return new GroupPanelView{v};
}

iscore::PanelPresenterInterface*GroupPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
                                                                 iscore::PanelViewInterface* view)
{
    return new GroupPanelPresenter{parent_presenter, view};
}

iscore::PanelModelInterface*GroupPanelFactory::makeModel(iscore::DocumentModel* m)
{
    return new GroupPanelModel{m};
}
