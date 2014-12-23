#include "TemporalConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraintCommand.hpp"

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
	view->setWidth(m_viewModel->model()->width()/m_millisecPerPixel);
	view->setHeight(m_viewModel->model()->height());

	for(auto& box : m_viewModel->model()->boxes())
	{
		on_boxCreated_impl(box);
	}

	connect(m_viewModel, &TemporalConstraintViewModel::boxCreated,
			this,		 &TemporalConstraintPresenter::on_boxCreated);
	connect(m_viewModel, &TemporalConstraintViewModel::boxRemoved,
			this,		 &TemporalConstraintPresenter::on_boxRemoved);

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
}

TemporalConstraintPresenter::~TemporalConstraintPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

int TemporalConstraintPresenter::viewModelId() const
{
	return m_viewModel->id();
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

void TemporalConstraintPresenter::on_boxCreated(int boxId)
{
	on_boxCreated_impl(m_viewModel->model()->box(boxId));
}

void TemporalConstraintPresenter::on_boxRemoved(int boxId)
{
	removeFromVectorWithId(m_boxes, boxId);
	on_askUpdate();
}

void TemporalConstraintPresenter::on_askUpdate()
{
	int contentPresenterVerticalSize = 0;
	if(m_boxes.size() > 0)
		contentPresenterVerticalSize = m_boxes.front()->height();

	m_view->setHeight(contentPresenterVerticalSize + 60);

	emit askUpdate();
	m_view->update();
}

void TemporalConstraintPresenter::on_boxCreated_impl(BoxModel* boxModel)
{
	auto contentView = new BoxView{m_view};
	contentView->setPos(5, 50);

	// Cas par défaut
	auto box_presenter =
			new BoxPresenter{boxModel,
							 contentView,
							 this};

	connect(box_presenter, &BoxPresenter::submitCommand,
			this,		   &TemporalConstraintPresenter::submitCommand);
	connect(box_presenter, &BoxPresenter::elementSelected,
			this,		   &TemporalConstraintPresenter::elementSelected);

	connect(box_presenter, &BoxPresenter::askUpdate,
			this,		   &TemporalConstraintPresenter::on_askUpdate);

	m_boxes.push_back(box_presenter);

	on_askUpdate();
}
