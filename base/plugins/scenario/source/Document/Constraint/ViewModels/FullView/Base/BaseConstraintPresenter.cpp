#include "BaseConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/Base/BaseConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsView>

BaseConstraintPresenter::BaseConstraintPresenter(
		FullViewConstraintViewModel* model,
		BaseConstraintView* view,
		BaseElementPresenter* parent):
	NamedObject{"BaseConstraintPresenter", parent},
	m_viewModel{model},
	m_view{view}
{
	m_viewportStartTime = 0;
	m_viewportEndTime = model->model()->defaultDuration() * 0.8;

	// This will be source of shame and regrets
	// QGraphicsView* view = m_view->scene()->views().front();

	if(m_viewModel->isBoxShown())
	{
		on_boxShown(m_viewModel->shownBox());
	}

	connect(m_viewModel, &FullViewConstraintViewModel::boxShown,
			this,		 &BaseConstraintPresenter::on_boxShown);
	connect(m_viewModel, &FullViewConstraintViewModel::boxHidden,
			this,		 &BaseConstraintPresenter::on_boxHidden);
	connect(m_viewModel, &FullViewConstraintViewModel::boxRemoved,
			this,		 &BaseConstraintPresenter::on_boxRemoved);

	// Le contentView est child de BaseConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	connect(m_view, &BaseConstraintView::constraintPressed,
			this,	&BaseConstraintPresenter::on_constraintPressed);

	updateView();
}

BaseConstraintPresenter::~BaseConstraintPresenter()
{
	if(m_view)
	{
		auto sc = m_view->scene();
		if(sc && sc->items().contains(m_view)) sc->removeItem(m_view);
		m_view->deleteLater();
	}
}

BaseConstraintView *BaseConstraintPresenter::view()
{
	return m_view;
}

FullViewConstraintViewModel *BaseConstraintPresenter::viewModel()
{
	return m_viewModel;
}

bool BaseConstraintPresenter::isSelected() const
{
	return m_view->isSelected();
}

void BaseConstraintPresenter::deselect()
{
	m_view->setSelected(false);
}

void BaseConstraintPresenter::on_constraintPressed(QPointF click)
{
	emit elementSelected(m_viewModel);
}

void BaseConstraintPresenter::on_boxShown(id_type<BoxModel> boxId)
{
	clearBoxPresenter();
	createBoxPresenter(m_viewModel->model()->box(boxId));

	updateView();
}

void BaseConstraintPresenter::on_boxHidden()
{
	clearBoxPresenter();

	updateView();
}

void BaseConstraintPresenter::on_boxRemoved()
{
	clearBoxPresenter();

	updateView();
}

void BaseConstraintPresenter::on_horizontalZoomChanged(int val)
{
	m_horizontalZoomSliderVal = val;
	recomputeViewport();
}

void BaseConstraintPresenter::recomputeViewport()
{

	// Set width of constraint
	double secPerPixel = 46.73630963 * std::exp(-0.07707388206 * m_horizontalZoomSliderVal);
	// sliderval = 20 => 1 pixel = 10s
	// sliderval = 50 => 1 pixel = 1s
	// sliderval = 80 => 1 pixel = 0.01s

	// Formule : y = 46.73630963 e^(x * -7.707388206 * 10^-2 )
	double centralTime = (m_viewportStartTime + m_viewportEndTime) / 2.0; // Replace with cursor in the future

	// prendre en compte la distance du clic à chaque côté
	view()->setWidth(m_viewModel->model()->defaultDuration() / secPerPixel);

	updateView();
	// translate viewport to accomodate


	// Fetch zoom cursor ?
	emit viewportChanged();
}

void BaseConstraintPresenter::clearBoxPresenter()
{
	if(m_box)
	{
		m_box->deleteLater();
		m_box = nullptr;
	}
}

void BaseConstraintPresenter::updateView()
{
	if(m_viewModel->isBoxShown())
	{
		m_view->setHeight(m_box->height() + 60);
	}
	else
	{
		// todo faire vue appropriée plus tard
		m_view->setHeight(25);
	}

	emit askUpdate();
	m_view->update();
}

void BaseConstraintPresenter::createBoxPresenter(BoxModel* boxModel)
{
	auto contentView = new BoxView{m_view};
	contentView->setPos(5, 50);

	// Default case
	m_box = new BoxPresenter{boxModel,
							 contentView,
							 this};

	connect(m_box, &BoxPresenter::submitCommand,
			this,  &BaseConstraintPresenter::submitCommand);
	connect(m_box, &BoxPresenter::elementSelected,
			this,  &BaseConstraintPresenter::elementSelected);

	connect(m_box, &BoxPresenter::askUpdate,
			this,  &BaseConstraintPresenter::updateView);
}
