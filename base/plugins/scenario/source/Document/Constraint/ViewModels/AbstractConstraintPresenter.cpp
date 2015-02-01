#include "AbstractConstraintPresenter.hpp"
#include "AbstractConstraintView.hpp"
#include "AbstractConstraintViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

AbstractConstraintPresenter::AbstractConstraintPresenter(
		QString name,
		AbstractConstraintViewModel* model,
		AbstractConstraintView* view,
		QObject* parent):
	NamedObject{name, parent},
	m_viewModel{model},
	m_view{view}
{
	connect(m_viewModel, &AbstractConstraintViewModel::boxShown,
			this,		 &AbstractConstraintPresenter::on_boxShown);
	connect(m_viewModel, &AbstractConstraintViewModel::boxHidden,
			this,		 &AbstractConstraintPresenter::on_boxHidden);
	connect(m_viewModel, &AbstractConstraintViewModel::boxRemoved,
			this,		 &AbstractConstraintPresenter::on_boxRemoved);

	connect(m_view, &AbstractConstraintView::constraintPressed,
			this,	&AbstractConstraintPresenter::on_constraintPressed);
}

bool AbstractConstraintPresenter::isSelected() const
{
	return m_view->isSelected();
}

void AbstractConstraintPresenter::deselect()
{
	m_view->setSelected(false);
}

void AbstractConstraintPresenter::on_boxShown(id_type<BoxModel> boxId)
{
	clearBoxPresenter();
	createBoxPresenter(m_viewModel->model()->box(boxId));

	updateView();
}

void AbstractConstraintPresenter::on_boxHidden()
{
	clearBoxPresenter();

	updateView();
}

void AbstractConstraintPresenter::on_boxRemoved()
{
	clearBoxPresenter();

	updateView();
}

void AbstractConstraintPresenter::clearBoxPresenter()
{
	if(m_box)
	{
		m_box->deleteLater();
		m_box = nullptr;
	}
}

void AbstractConstraintPresenter::createBoxPresenter(BoxModel* boxModel)
{
	auto contentView = new BoxView{m_view};
	contentView->setPos(5, 50);

	// Cas par défaut
	m_box = new BoxPresenter{boxModel,
							 contentView,
							 this};

	connect(m_box, &BoxPresenter::submitCommand,
			this,  &AbstractConstraintPresenter::submitCommand);
	connect(m_box, &BoxPresenter::elementSelected,
			this,  &AbstractConstraintPresenter::elementSelected);

	connect(m_box, &BoxPresenter::askUpdate,
			this,  &AbstractConstraintPresenter::updateView);

	box()->setWidth(m_view->defaultWidth() - 20);
}
