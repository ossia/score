#include "TemporalConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

TemporalConstraintPresenter::TemporalConstraintPresenter(
		TemporalConstraintViewModel* cstr_model,
		TemporalConstraintView* cstr_view,
		QObject* parent):
	AbstractConstraintPresenter{"TemporalConstraintPresenter", cstr_model, cstr_view, parent}
{

	if(viewModel(this)->isBoxShown())
	{
		on_boxShown(viewModel(this)->shownBox());
	}

	connect(viewModel(this), &TemporalConstraintViewModel::boxShown,
			this,			 &AbstractConstraintPresenter::on_boxShown);
	connect(viewModel(this), &TemporalConstraintViewModel::boxHidden,
			this,			 &AbstractConstraintPresenter::on_boxHidden);
	connect(viewModel(this), &TemporalConstraintViewModel::boxRemoved,
			this,			 &AbstractConstraintPresenter::on_boxRemoved);

	connect(viewModel(this)->model(),   &ConstraintModel::minDurationChanged,
			this,						&TemporalConstraintPresenter::on_minDurationChanged);
	connect(viewModel(this)->model(),   &ConstraintModel::maxDurationChanged,
			this,						&TemporalConstraintPresenter::on_maxDurationChanged);

	// Le contentView est child de TemporalConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	connect(view(this), &TemporalConstraintView::constraintPressed,
			this,		&TemporalConstraintPresenter::on_constraintPressed);

	connect(view(this), &TemporalConstraintView::constraintReleased,
			[&] (QPointF p)
	{
		ConstraintData data{};
		data.id = viewModel(this)->model()->id();
		data.y = p.y();
		data.x = p.x();
		emit constraintReleased(data);
	});

	updateView();
}

TemporalConstraintPresenter::~TemporalConstraintPresenter()
{
	if(view(this))
	{
		auto sc = view(this)->scene();
		if(sc && sc->items().contains(view(this))) sc->removeItem(view(this));
		view(this)->deleteLater();
	}
}

void TemporalConstraintPresenter::on_constraintPressed(QPointF click)
{
	emit elementSelected(viewModel(this));
}

void TemporalConstraintPresenter::on_minDurationChanged(int min)
{
    //todo passer par scenariopresenter pour le zoom
	view(this)->setMinWidth(viewModel(this)->model()->minDuration());
	updateView();
}

void TemporalConstraintPresenter::on_maxDurationChanged(int max)
{
	view(this)->setMaxWidth(viewModel(this)->model()->maxDuration());
	updateView();
}

void TemporalConstraintPresenter::updateView()
{
	if(viewModel(this)->isBoxShown())
	{
		view(this)->setHeight(m_box->height() + 60);
	}
	else
	{
		// faire vue appropriée plus tard
		view(this)->setHeight(25);
	}

	emit askUpdate();
	view(this)->update();
}
