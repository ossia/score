#include "ConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintView.hpp"
#include "Document/Constraint/ConstraintContent/ConstraintContentPresenter.hpp"
#include "Document/Constraint/ConstraintContent/ConstraintContentView.hpp"
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
	view->m_rect.setWidth(model->width()/m_millisecPerPixel);
	view->m_rect.setHeight(model->height());

	// Le contentView est child de ConstraintView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	// TODO mettre ça dans la doc des classes

	auto contentView = new ConstraintContentView{view};

	// Cas par défaut
	auto constraint_presenter =
			new ConstraintContentPresenter{
							  model->contentModels().front(),
							  contentView,
							  this};

	connect(constraint_presenter, &ConstraintContentPresenter::submitCommand,
			this,				&ConstraintPresenter::submitCommand);
	connect(constraint_presenter, &ConstraintContentPresenter::elementSelected,
			this,				&ConstraintPresenter::elementSelected);

	m_contentPresenters.push_back(constraint_presenter);

	connect(m_view, &ConstraintView::constraintPressed,
			this,	&ConstraintPresenter::on_constraintPressed);

	connect(m_view, &ConstraintView::constraintReleased,
			[&] (QPointF p)
	{
        emit constraintReleased(id(), p.y() - clickedPoint.y());
	});

	connect(m_view, &ConstraintView::addScenarioProcessClicked,
			[&] ()
	{
		auto path = ObjectPath::pathFromObject("BaseConstraintModel", m_model);
		auto cmd = new AddProcessToConstraintCommand(std::move(path), "Scenario");
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
    clickedPoint = click;
	emit elementSelected(m_model);
}
