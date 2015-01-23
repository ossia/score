#include "BaseElementPresenter.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/BaseElement/Widgets/AddressBar.hpp"

// TODO put this somewhere else
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

#include <QGraphicsScene>
using namespace iscore;


BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* delegate_model,
										   DocumentDelegateViewInterface* delegate_view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", delegate_model, delegate_view}
{
	connect(view()->addressBar(), &AddressBar::objectSelected,
			model(),			  &BaseElementModel::setDisplayedObject);

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
	TemporalConstraintViewModel* constraintViewModel{};
	if(model()->constraintModel() == model()->displayedConstraint())
	{
		constraintViewModel = model()->constraintViewModel();
	}
	else
	{
		auto viewmodels = model()->displayedConstraint()->viewModels();

		for(auto& viewmodel : viewmodels)
		{
			if((constraintViewModel = dynamic_cast<TemporalConstraintViewModel*>(viewmodel)))
				break;
		}

		if(!constraintViewModel)
			qFatal("Error : cannot display a constraint without view model");
	}

	auto cstrView = new TemporalConstraintView{constraintViewModel,
											   this->view()->baseObject()};
	cstrView->setFlag(QGraphicsItem::ItemIsSelectable, false);
	cstrView->setWidth(1000);
	cstrView->setMinWidth(1000);
	cstrView->setMaxWidth(1000);

	delete m_baseConstraintPresenter;
	m_baseConstraintPresenter = new TemporalConstraintPresenter
									{
										constraintViewModel,
										cstrView,
										this
										};
	on_askUpdate();

	connect(m_baseConstraintPresenter,	&TemporalConstraintPresenter::submitCommand,
			this,						&BaseElementPresenter::submitCommand);

	connect(m_baseConstraintPresenter,	&TemporalConstraintPresenter::elementSelected,
			this,						&BaseElementPresenter::elementSelected);

	connect(m_baseConstraintPresenter,	&TemporalConstraintPresenter::askUpdate,
			this,						&BaseElementPresenter::on_askUpdate);

	// Update the address bar
	view()
		->addressBar()
		->setTargetObject(ObjectPath::pathFromObject(model()->displayedConstraint()));
}

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
