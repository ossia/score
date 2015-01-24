#include "FullViewConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/FullView/FullViewConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

FullViewConstraintPresenter::FullViewConstraintPresenter(
		FullViewConstraintViewModel* model,
		FullViewConstraintView* view,
		QObject* parent):
	NamedObject{"FullViewConstraintPresenter", parent},
	m_viewModel{model},
	m_view{view}
{

	if(m_viewModel->isBoxShown())
	{
		on_boxShown(m_viewModel->shownBox());
	}

	connect(m_viewModel, &FullViewConstraintViewModel::boxShown,
			this,		 &FullViewConstraintPresenter::on_boxShown);
	connect(m_viewModel, &FullViewConstraintViewModel::boxHidden,
			this,		 &FullViewConstraintPresenter::on_boxHidden);
	connect(m_viewModel, &FullViewConstraintViewModel::boxRemoved,
			this,		 &FullViewConstraintPresenter::on_boxRemoved);

	connect(m_viewModel->model(), &ConstraintModel::minDurationChanged,
            this,				  &FullViewConstraintPresenter::on_minDurationChanged);
	connect(m_viewModel->model(), &ConstraintModel::maxDurationChanged,
            this,				  &FullViewConstraintPresenter::on_maxDurationChanged);

	// Le contentView est child de FullViewConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	connect(m_view, &FullViewConstraintView::constraintPressed,
			this,	&FullViewConstraintPresenter::on_constraintPressed);

	connect(m_view, &FullViewConstraintView::constraintReleased,
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

FullViewConstraintPresenter::~FullViewConstraintPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc && sc->items().contains(m_view)) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

FullViewConstraintView *FullViewConstraintPresenter::view()
{
	return m_view;
}

FullViewConstraintViewModel *FullViewConstraintPresenter::viewModel()
{
	return m_viewModel;
}

bool FullViewConstraintPresenter::isSelected() const
{
	return m_view->isSelected();
}

void FullViewConstraintPresenter::deselect()
{
	m_view->setSelected(false);
}

void FullViewConstraintPresenter::on_constraintPressed(QPointF click)
{
	emit elementSelected(m_viewModel);
}

void FullViewConstraintPresenter::on_boxShown(id_type<BoxModel> boxId)
{
	clearBoxPresenter();
	createBoxPresenter(m_viewModel->model()->box(boxId));

	updateView();
}

void FullViewConstraintPresenter::on_boxHidden()
{
	clearBoxPresenter();

	updateView();
}

void FullViewConstraintPresenter::on_boxRemoved()
{
	clearBoxPresenter();

	updateView();
}

void FullViewConstraintPresenter::on_minDurationChanged(int min)
{
    //todo passer par scenariopresenter pour le zoom
    m_view->setMinWidth(m_viewModel->model()->minDuration());
	updateView();
}

void FullViewConstraintPresenter::on_maxDurationChanged(int max)
{
    m_view->setMaxWidth(m_viewModel->model()->maxDuration());
	updateView();
}

void FullViewConstraintPresenter::clearBoxPresenter()
{
	if(m_box)
	{
		m_box->deleteLater();
		m_box = nullptr;
	}
}

void FullViewConstraintPresenter::updateView()
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

void FullViewConstraintPresenter::createBoxPresenter(BoxModel* boxModel)
{
	auto contentView = new BoxView{m_view};
	contentView->setPos(5, 50);

	// Cas par défaut
	m_box = new BoxPresenter{boxModel,
							 contentView,
							 this};

	connect(m_box, &BoxPresenter::submitCommand,
			this,  &FullViewConstraintPresenter::submitCommand);
	connect(m_box, &BoxPresenter::elementSelected,
			this,  &FullViewConstraintPresenter::elementSelected);

	connect(m_box, &BoxPresenter::askUpdate,
			this,  &FullViewConstraintPresenter::updateView);
}
