#include "AbstractConstraintPresenter.hpp"
#include "AbstractConstraintView.hpp"
#include "AbstractConstraintViewModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

/**
 * TODO Mettre dans la doc :
 * L'abstract constraint presenter a deux interfaces :
 *  - une qui est relative à la gestion de la vue (setScaleFactor)
 *  - une qui est là pour interagir avec le modèle (on_defaul/min/maxDurationChanged)
 */

AbstractConstraintPresenter::AbstractConstraintPresenter(
		QString name,
		AbstractConstraintViewModel* model,
		AbstractConstraintView* view,
		QObject* parent):
	NamedObject{name, parent},
	m_viewModel{model},
	m_view{view}
{
	connect(m_viewModel->model(),   &ConstraintModel::minDurationChanged,
			this,					&AbstractConstraintPresenter::on_minDurationChanged);
	connect(m_viewModel->model(),   &ConstraintModel::defaultDurationChanged,
			this,					&AbstractConstraintPresenter::on_defaultDurationChanged);
	connect(m_viewModel->model(),   &ConstraintModel::maxDurationChanged,
			this,					&AbstractConstraintPresenter::on_maxDurationChanged);

	connect(m_viewModel, &AbstractConstraintViewModel::boxShown,
			this,		 &AbstractConstraintPresenter::on_boxShown);
	connect(m_viewModel, &AbstractConstraintViewModel::boxHidden,
			this,		 &AbstractConstraintPresenter::on_boxHidden);
	connect(m_viewModel, &AbstractConstraintViewModel::boxRemoved,
			this,		 &AbstractConstraintPresenter::on_boxRemoved);

	connect(m_view, &AbstractConstraintView::constraintPressed,
			this,	&AbstractConstraintPresenter::on_constraintPressed);
}

int AbstractConstraintPresenter::zoomSlider() const
{
	return m_horizontalZoomSliderVal;
}

void AbstractConstraintPresenter::updateScaling(double secPerPixel)
{
	// prendre en compte la distance du clic à chaque côté
	m_view->setDefaultWidth(m_viewModel->model()->defaultDuration().msec() / secPerPixel);
	m_view->setMinWidth(m_viewModel->model()->minDuration().msec() / secPerPixel);
	m_view->setMaxWidth(m_viewModel->model()->maxDuration().msec() / secPerPixel);

	if(box())
	{
		box()->setWidth(m_view->defaultWidth() - 20);
	}

	updateHeight();
}

void AbstractConstraintPresenter::on_horizontalZoomChanged(int val)
{
	m_horizontalZoomSliderVal = val;
	updateScaling(millisecondsPerPixel(m_horizontalZoomSliderVal));

	if(box())
	{
		box()->on_horizontalZoomChanged(val);
	}
}

void AbstractConstraintPresenter::on_defaultDurationChanged(TimeValue val)
{
	double secPerPixel = millisecondsPerPixel(m_horizontalZoomSliderVal);
	m_view->setDefaultWidth(val.msec() / secPerPixel);

	emit askUpdate();
	m_view->update();
}

void AbstractConstraintPresenter::on_minDurationChanged(TimeValue min)
{
	double secPerPixel = millisecondsPerPixel(m_horizontalZoomSliderVal);
	m_view->setMinWidth(min.msec() / secPerPixel);

	emit askUpdate();
	m_view->update();
}

void AbstractConstraintPresenter::on_maxDurationChanged(TimeValue max)
{
	double secPerPixel = millisecondsPerPixel(m_horizontalZoomSliderVal);
	m_view->setMaxWidth(max .msec()/ secPerPixel);

	emit askUpdate();
	m_view->update();
}

void AbstractConstraintPresenter::updateHeight()
{
	if(m_viewModel->isBoxShown())
	{
		m_view->setHeight(box()->height() + 60);
	}
	else
	{
		// faire vue appropriée plus tard
		m_view->setHeight(25);
	}

	emit askUpdate();
	m_view->update();
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

	updateHeight();
}

void AbstractConstraintPresenter::on_boxHidden()
{
	clearBoxPresenter();

	updateHeight();
}

void AbstractConstraintPresenter::on_boxRemoved()
{
	clearBoxPresenter();

	updateHeight();
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
	auto boxView = new BoxView{m_view};
	boxView->setPos(0, 5);

	// Cas par défaut
	m_box = new BoxPresenter{boxModel,
							 boxView,
							 this};

	m_box->on_horizontalZoomChanged(m_horizontalZoomSliderVal);

	connect(m_box, &BoxPresenter::submitCommand,
			this,  &AbstractConstraintPresenter::submitCommand);
	connect(m_box, &BoxPresenter::elementSelected,
			this,  &AbstractConstraintPresenter::elementSelected);

	connect(m_box, &BoxPresenter::askUpdate,
			this,  &AbstractConstraintPresenter::updateHeight);
}
