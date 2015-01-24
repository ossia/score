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
		TemporalConstraintViewModel* model,
		TemporalConstraintView* view,
		QObject* parent):
	NamedObject{"TemporalConstraintPresenter", parent},
	m_viewModel{model},
	m_view{view}
{

	if(m_viewModel->isBoxShown())
	{
		on_boxShown(m_viewModel->shownBox());
	}

	connect(m_viewModel, &TemporalConstraintViewModel::boxShown,
			this,		 &TemporalConstraintPresenter::on_boxShown);
	connect(m_viewModel, &TemporalConstraintViewModel::boxHidden,
			this,		 &TemporalConstraintPresenter::on_boxHidden);
	connect(m_viewModel, &TemporalConstraintViewModel::boxRemoved,
			this,		 &TemporalConstraintPresenter::on_boxRemoved);

	connect(m_viewModel->model(), &ConstraintModel::minDurationChanged,
            this,				  &TemporalConstraintPresenter::on_minDurationChanged);
	connect(m_viewModel->model(), &ConstraintModel::maxDurationChanged,
            this,				  &TemporalConstraintPresenter::on_maxDurationChanged);

	// Le contentView est child de TemporalConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	connect(m_view, &TemporalConstraintView::constraintPressed,
			this,	&TemporalConstraintPresenter::on_constraintPressed);

	connect(m_view, &TemporalConstraintView::constraintReleased,
			[&] (QPointF p)
	{
		ConstraintData data{};
		data.id = viewModel()->model()->id();
		data.y = p.y();
		data.x = p.x();
		emit constraintReleased(data);
	});

	updateView();
}

TemporalConstraintPresenter::~TemporalConstraintPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc && sc->items().contains(m_view)) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

TemporalConstraintView *TemporalConstraintPresenter::view()
{
	return m_view;
}

TemporalConstraintViewModel *TemporalConstraintPresenter::viewModel()
{
	return m_viewModel;
}

bool TemporalConstraintPresenter::isSelected() const
{
	return m_view->isSelected();
}

void TemporalConstraintPresenter::deselect()
{
	m_view->setSelected(false);
}

void TemporalConstraintPresenter::on_constraintPressed(QPointF click)
{
	emit elementSelected(m_viewModel);
}

void TemporalConstraintPresenter::on_boxShown(id_type<BoxModel> boxId)
{
	clearBoxPresenter();
	createBoxPresenter(m_viewModel->model()->box(boxId));

	updateView();
}

void TemporalConstraintPresenter::on_boxHidden()
{
	clearBoxPresenter();

	updateView();
}

void TemporalConstraintPresenter::on_boxRemoved()
{
	clearBoxPresenter();

	updateView();
}

void TemporalConstraintPresenter::on_minDurationChanged(int min)
{
    //todo passer par scenariopresenter pour le zoom
    m_view->setMinWidth(m_viewModel->model()->minDuration());
	updateView();
}

void TemporalConstraintPresenter::on_maxDurationChanged(int max)
{
    m_view->setMaxWidth(m_viewModel->model()->maxDuration());
	updateView();
}

void TemporalConstraintPresenter::clearBoxPresenter()
{
	if(m_box)
	{
		m_box->deleteLater();
		m_box = nullptr;
	}
}

void TemporalConstraintPresenter::updateView()
{
	if(m_viewModel->isBoxShown())
	{
		m_view->setHeight(m_box->height() + 60);
	}
	else
	{
		// faire vue appropriée plus tard
		m_view->setHeight(25);
	}

	emit askUpdate();
	m_view->update();
}

void TemporalConstraintPresenter::createBoxPresenter(BoxModel* boxModel)
{
	auto contentView = new BoxView{m_view};
	contentView->setPos(5, 50);

	// Cas par défaut
	m_box = new BoxPresenter{boxModel,
							 contentView,
							 this};

	connect(m_box, &BoxPresenter::submitCommand,
			this,  &TemporalConstraintPresenter::submitCommand);
	connect(m_box, &BoxPresenter::elementSelected,
			this,  &TemporalConstraintPresenter::elementSelected);

	connect(m_box, &BoxPresenter::askUpdate,
			this,  &TemporalConstraintPresenter::updateView);
}
