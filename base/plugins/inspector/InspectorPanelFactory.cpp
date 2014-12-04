#include "InspectorPanelFactory.hpp"
#include <Inspector/InspectorPanel.hpp>
#include <Interval/objectinterval.hpp>
#include <core/model/Model.hpp>
#include <core/view/View.hpp>
using namespace iscore;

InspectorPanelView::InspectorPanelView (View* parent) :
	iscore::PanelViewInterface {parent}
{

}

QWidget* InspectorPanelView::getWidget()
{
	auto ptr = new InspectorPanel;
	ObjectInterval* test1 = new ObjectInterval ("MonNom", "remarques diverses", Qt::red );

	ptr->newItemInspected (test1);

	return ptr;
}


iscore::PanelViewInterface* InspectorPanelFactory::makeView (iscore::View* parent)
{
	return new InspectorPanelView {parent};
}

iscore::PanelPresenterInterface* InspectorPanelFactory::makePresenter (
    iscore::Presenter* parent_presenter,
    iscore::PanelModelInterface* model,
    iscore::PanelViewInterface* view)
{
	return new InspectorPanelPresenter {parent_presenter, model, view};
}

iscore::PanelModelInterface* InspectorPanelFactory::makeModel (iscore::Model* parent)
{
	return new InspectorPanelModel {parent};
}

InspectorPanelModel::InspectorPanelModel (Model* parent) :
	iscore::PanelModelInterface {parent, "InspectorPanelModel"}
{
}


InspectorPanelPresenter::InspectorPanelPresenter (iscore::Presenter* parent, iscore::PanelModelInterface* model, iscore::PanelViewInterface* view) :
	iscore::PanelPresenterInterface {parent, model, view}
{

}
