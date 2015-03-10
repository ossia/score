#include "ProcessPanelFactory.hpp"
#include "ProcessPanelModel.hpp"
#include "ProcessPanelPresenter.hpp"
#include "ProcessPanelView.hpp"

#include <core/view/View.hpp>
#include <core/document/DocumentModel.hpp>
iscore::PanelViewInterface*ProcessPanelFactory::makeView(iscore::View* parent)
{
    return new ProcessPanelView{parent};
}

iscore::PanelPresenterInterface*ProcessPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
                                                                   iscore::PanelViewInterface* view)
{
    return new ProcessPanelPresenter{parent_presenter, view};
}

iscore::PanelModelInterface*ProcessPanelFactory::makeModel(iscore::DocumentModel* parent)
{
    return new ProcessPanelModel{parent};
}
