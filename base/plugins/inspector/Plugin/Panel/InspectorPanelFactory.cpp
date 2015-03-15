#include "InspectorPanelFactory.hpp"
#include "InspectorPanelView.hpp"
#include "InspectorPanelModel.hpp"
#include "InspectorPanelPresenter.hpp"
//#include <Interval/objectinterval.hpp>
using namespace iscore;


iscore::PanelViewInterface* InspectorPanelFactory::makeView(iscore::View* parent)
{
    return new InspectorPanelView {parent};
}

iscore::PanelPresenterInterface* InspectorPanelFactory::makePresenter(
    iscore::Presenter* parent_presenter,
    iscore::PanelViewInterface* view)
{
    return new InspectorPanelPresenter {parent_presenter, view};
}

iscore::PanelModelInterface* InspectorPanelFactory::makeModel(iscore::DocumentModel* parent)
{
    return new InspectorPanelModel {parent};
}

