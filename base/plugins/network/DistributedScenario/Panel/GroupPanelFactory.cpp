#include "GroupPanelFactory.hpp"

#include "GroupPanelModel.hpp"
#include "GroupPanelPresenter.hpp"
#include "GroupPanelView.hpp"

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
