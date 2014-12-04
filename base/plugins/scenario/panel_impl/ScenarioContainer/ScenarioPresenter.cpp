#include "ScenarioPresenter.hpp"
#include "ScenarioModel.hpp"
#include "ScenarioView.hpp"

#include <QDebug>
#include <QApplication>

//#include "commands/CreateEventCommand.hpp"
#include <core/presenter/Presenter.hpp>
ScenarioPresenter::ScenarioPresenter (ScenarioModel* model, 
									  ScenarioView* view, 
									  QObject* parent) :
	QObject {nullptr},
	m_model {model},
	m_view {view}
{
	setObjectName ("ScenarioPresenter");
	setParent (parent);
	connect (m_view,	&ScenarioView::viewAskForTimeEvent,
			 this,		&ScenarioPresenter::on_askTimeEvent);
	
	connect(m_model,	&ScenarioModel::timeEventAdded,
			this,		&ScenarioPresenter::on_timeEventAdded);
	connect(m_model,	&ScenarioModel::timeEventRemoved,
			this,		&ScenarioPresenter::on_timeEventRemoved);
	/*
	auto parent_ptr = (QObject*) this;
	while(parent_ptr != nullptr)
	{
		qDebug() << parent_ptr->objectName();
		parent_ptr = parent_ptr->parent();
	}
	*/
}

ScenarioPresenter::~ScenarioPresenter()
{

}

void ScenarioPresenter::on_askTimeEvent (QPointF pos)
{
	qDebug (Q_FUNC_INFO);
/*	
	// On instancie la commande
	auto cmd = new CreatEventCommand (m_model->id(), pos);

	// Puis on la fait remonter au présenteur
	auto pres = qApp->findChild<iscore::Presenter*> ("Presenter");
	pres->applyCommand (cmd);
	*/
}
/* Not yet :-(
#include <functional>
std::function<bool (int)> index_is(int index)
{
	return [&index] (auto elt)
	{
		return elt->index() == index;
	};
}
*/
#include "panel_impl/TimeEvent/TimeEventModel.hpp"
#include "panel_impl/TimeEvent/TimeEventView.hpp"
#include "panel_impl/TimeEvent/TimeEventPresenter.hpp"

void ScenarioPresenter::on_timeEventAdded(TimeEventModel* model)
{
	qDebug(Q_FUNC_INFO);
	auto view = new TimeEventView (model->index(), this);
	auto presenter = new TimeEventPresenter (model, view, this);
	view->setPresenter(presenter);
	
	m_view->addTimeEventView(view);
	m_timeEvents.push_back(presenter);
}

void ScenarioPresenter::on_timeEventRemoved(TimeEventModel* model)
{
	qDebug(Q_FUNC_INFO);
	m_view->removeTimeEvent(model->index());
	
	auto it = std::find_if(std::begin(m_timeEvents),
						   std::end(m_timeEvents),
						   [&model] (TimeEventPresenter* p)
							{
								return p->index() == model->index();
							});
	
	m_timeEvents.erase(it);
}