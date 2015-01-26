#include "BaseElementPresenter.hpp"

#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include <QGraphicsView>
#include "Document/BaseElement/Widgets/AddressBar.hpp"

// TODO put this somewhere else
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

#include <QGraphicsScene>
using namespace iscore;


BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* delegate_model,
										   DocumentDelegateViewInterface* delegate_view):
	DocumentDelegatePresenterInterface{parent_presenter,
									   "BaseElementPresenter",
									   delegate_model,
									   delegate_view}
{
	connect(view()->addressBar(), &AddressBar::objectSelected,
			model(),			  &BaseElementModel::setDisplayedObject);
	connect(view(), &BaseElementView::horizontalZoomChanged,
			this,	&BaseElementPresenter::on_horizontalZoomChanged);


	connect(model(), &BaseElementModel::displayedConstraintChanged,
			this,	 &BaseElementPresenter::on_displayedConstraintChanged);

	on_displayedConstraintChanged();
}

void BaseElementPresenter::on_askUpdate()
{
	view()->update();
}

void BaseElementPresenter::selectAll()
{
	for(auto item : view()->scene()->items())
	{
		item->setSelected(true);
	}
}

void BaseElementPresenter::deselectAll()
{
	for(auto item : view()->scene()->items())
	{
		item->setSelected(false);
	}
}

void BaseElementPresenter::deleteSelection()
{
}

void BaseElementPresenter::on_displayedConstraintChanged()
{
	auto constraintViewModel = model()->displayedConstraint()->fullView();

	auto cstrView = new FullViewConstraintView{this->view()->baseObject()};
	cstrView->setFlag(QGraphicsItem::ItemIsSelectable, false);
	cstrView->setDefaultWidth(1000);
	cstrView->setMinWidth(1000);
	cstrView->setMaxWidth(1000);

	delete m_baseConstraintPresenter;
	m_baseConstraintPresenter = new FullViewConstraintPresenter{constraintViewModel,
																cstrView,
																this};
	on_askUpdate();

	connect(m_baseConstraintPresenter,	&FullViewConstraintPresenter::submitCommand,
			this,						&BaseElementPresenter::submitCommand);

	connect(m_baseConstraintPresenter,	&FullViewConstraintPresenter::elementSelected,
			this,						&BaseElementPresenter::elementSelected);

	connect(m_baseConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
			this,						&BaseElementPresenter::on_askUpdate);

	// Update the address bar
	view()
		->addressBar()
			->setTargetObject(ObjectPath::pathFromObject(model()->displayedConstraint()));
}

void BaseElementPresenter::on_horizontalZoomChanged(int newzoom)
{
	// 0 : least zoom (min = 20)
	// 100 : most zoom (max = 80)


	// Recalculer le viewport
	//view()->view()->translate();
	// Mettre à jour la contrainte
	m_baseConstraintPresenter->on_horizontalZoomChanged(newzoom);
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
