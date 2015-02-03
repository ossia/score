#include "FullViewConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Document/ZoomHelper.hpp"

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

	updateHeight();
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
