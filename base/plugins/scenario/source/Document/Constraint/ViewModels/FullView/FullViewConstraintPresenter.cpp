#include "FullViewConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

FullViewConstraintPresenter::FullViewConstraintPresenter(
		FullViewConstraintViewModel* cstr_model,
		FullViewConstraintView* cstr_view,
		QObject* parent):
	AbstractConstraintPresenter{"FullViewConstraintPresenter", cstr_model, cstr_view, parent}
{
	connect(viewModel(this)->model(), &ConstraintModel::minDurationChanged,
			this,					  &FullViewConstraintPresenter::on_minDurationChanged);
	connect(viewModel(this)->model(), &ConstraintModel::maxDurationChanged,
			this,					  &FullViewConstraintPresenter::on_maxDurationChanged);

	// Le contentView est child de FullViewConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	connect(view(this), &FullViewConstraintView::constraintPressed,
			this,		&FullViewConstraintPresenter::on_constraintPressed);


	if(viewModel(this)->isBoxShown())
	{
		on_boxShown(viewModel(this)->shownBox());
	}

	updateView();
}

FullViewConstraintPresenter::~FullViewConstraintPresenter()
{
	if(view(this))
	{
		auto sc = view(this)->scene();
		if(sc && sc->items().contains(view(this))) sc->removeItem(view(this));
		view(this)->deleteLater();
	}
}

void FullViewConstraintPresenter::on_constraintPressed(QPointF click)
{
	emit elementSelected(viewModel(this));
}


void FullViewConstraintPresenter::on_minDurationChanged(int min)
{
	//todo passer par scenariopresenter pour le zoom
	view(this)->setMinWidth(viewModel(this)->model()->minDuration());
	updateView();
}

void FullViewConstraintPresenter::on_maxDurationChanged(int max)
{
	view(this)->setMaxWidth(viewModel(this)->model()->maxDuration());
	updateView();
}


void FullViewConstraintPresenter::updateView()
{
	if(viewModel(this)->isBoxShown())
	{
		view(this)->setHeight(box()->height() + 60);
	}
	else
	{
		// faire vue appropriée plus tard
		view(this)->setHeight(25);
	}

	emit askUpdate();
	view(this)->update();
}

void FullViewConstraintPresenter::on_horizontalZoomChanged(int val)
{
	m_horizontalZoomSliderVal = val;
	recomputeViewport();
}

void FullViewConstraintPresenter::recomputeViewport()
{

	// Set width of constraint
	double secPerPixel = 46.73630963 * std::exp(-0.07707388206 * m_horizontalZoomSliderVal);
	// sliderval = 20 => 1 pixel = 10s
	// sliderval = 50 => 1 pixel = 1s
	// sliderval = 80 => 1 pixel = 0.01s

	// Formule : y = 46.73630963 e^(x * -7.707388206 * 10^-2 )
//	double centralTime = (m_viewportStartTime + m_viewportEndTime) / 2.0; // Replace with cursor in the future

	// prendre en compte la distance du clic à chaque côté
	view(this)->setDefaultWidth(viewModel(this)->model()->defaultDuration() / secPerPixel);

	updateView();
	// translate viewport to accomodate


	// Fetch zoom cursor ?
	emit viewportChanged();
}

