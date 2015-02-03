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
	connect(viewModel(this)->model(),   &ConstraintModel::minDurationChanged,
			this,						&TemporalConstraintPresenter::on_minDurationChanged);
	connect(viewModel(this)->model(),   &ConstraintModel::maxDurationChanged,
			this,						&TemporalConstraintPresenter::on_maxDurationChanged);

	// Le contentView est child de TemporalConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes


	connect(view(this), &TemporalConstraintView::constraintReleased,
			[&] (QPointF p)
	{
		ConstraintData data{};
		data.id = viewModel(this)->model()->id();
		data.y = p.y();
		data.x = p.x();
		emit constraintReleased(data);
	});


	if(viewModel(this)->isBoxShown())
	{
		on_boxShown(viewModel(this)->shownBox());
	}

	updateHeight();
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
