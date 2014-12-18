#include "ConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/Process/AddProcessToConstraintCommand.hpp"

#include <core/presenter/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

ConstraintPresenter::ConstraintPresenter(ConstraintModel* model,
										 ConstraintView* view,
										 QObject* parent):
	NamedObject{"ConstraintPresenter", parent},
	m_model{model},
	m_view{view}
{
	view->setWidth(model->width()/m_millisecPerPixel);
	view->setHeight(model->height());

	for(auto& box : m_model->boxes())
	{
		on_boxCreated_impl(box);
	}

	connect(m_model, &ConstraintModel::boxCreated,
			this,	 &ConstraintPresenter::on_boxCreated);

	// Le contentView est child de ConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	connect(m_view, &ConstraintView::constraintPressed,
			this,	&ConstraintPresenter::on_constraintPressed);

	connect(m_view, &ConstraintView::constraintReleased,
			[&] (QPointF p)
	{
		ConstraintData data{};
		data.id = id();
		data.y = p.y();
		data.x = p.x();
		emit constraintReleased(data);
	});

	connect(m_view, &ConstraintView::addScenarioProcessClicked,
			[&] ()
	{
		auto path = ObjectPath::pathFromObject("BaseConstraintModel", m_model);
		auto cmd = new AddProcessToConstraintCommand(std::move(path), "CurvePlugin");
		emit submitCommand(cmd);
	});
}

ConstraintPresenter::~ConstraintPresenter()
{
	auto sc = m_view->scene();
	if(sc) sc->removeItem(m_view);
	m_view->deleteLater();
}

int ConstraintPresenter::id() const
{
	return m_model->id();
}

ConstraintView *ConstraintPresenter::view()
{
	return m_view;
}

ConstraintModel *ConstraintPresenter::model()
{
	return m_model;
}

void ConstraintPresenter::on_constraintPressed(QPointF click)
{
	emit elementSelected(m_model);
}

void ConstraintPresenter::on_boxCreated(int boxId)
{
	on_boxCreated_impl(m_model->box(boxId));
}

void ConstraintPresenter::on_askUpdate()
{
	int contentPresenterVerticalSize = 0;
	if(m_contentPresenters.size() > 0)
		contentPresenterVerticalSize = m_contentPresenters.front()->height();

	m_view->setHeight(contentPresenterVerticalSize + 60);

	emit askUpdate();
	m_view->update();
}

void ConstraintPresenter::on_boxCreated_impl(BoxModel* boxModel)
{
	auto contentView = new BoxView{m_view};
	contentView->setPos(5, 50);

	// Cas par défaut
	auto box_presenter =
			new BoxPresenter{boxModel,
							 contentView,
							 this};

	connect(box_presenter, &BoxPresenter::submitCommand,
			this,		   &ConstraintPresenter::submitCommand);
	connect(box_presenter, &BoxPresenter::elementSelected,
			this,		   &ConstraintPresenter::elementSelected);

	connect(box_presenter, &BoxPresenter::askUpdate,
			this,		   &ConstraintPresenter::on_askUpdate);

	m_contentPresenters.push_back(box_presenter);

	on_askUpdate();
}
