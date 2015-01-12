#include "BaseElementPresenter.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementView.hpp"

#include <QGraphicsScene>
using namespace iscore;


BaseElementPresenter::BaseElementPresenter(DocumentPresenter* parent_presenter,
										   DocumentDelegateModelInterface* model,
										   DocumentDelegateViewInterface* view):
	DocumentDelegatePresenterInterface{parent_presenter, "BaseElementPresenter", model, view}
{

	auto cstrView = new TemporalConstraintView{this->model()->constraintViewModel(),
											   this->view()->baseObject()};
	cstrView->setFlag(QGraphicsItem::ItemIsSelectable, false);

	m_baseConstraintPresenter = new TemporalConstraintPresenter
									{
										this->model()->constraintViewModel(),
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
}

void BaseElementPresenter::on_reset()
{
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

BaseElementModel* BaseElementPresenter::model()
{
	return static_cast<BaseElementModel*>(m_model);
}

BaseElementView* BaseElementPresenter::view()
{
	return static_cast<BaseElementView*>(m_view);
}
