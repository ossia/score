#include "CreateEventCommand.hpp"
#include "panel_impl/ScenarioContainer/ScenarioModel.hpp"

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>
using namespace iscore;


CreatEventCommand::CreatEventCommand(int modelId, QPointF position):
	SerializableCommand{"ScenarioControl", "CreateEventCommand", QObject::tr("Event creation")},
	m_modelId{modelId},
	m_position{position}
{
	
}
#include <QDebug>
void CreatEventCommand::undo()
{
	// Proper way should be : 
	// auto scenarios = qApp->findChildren<ScenarioModel*>("ScenarioModel");
	// But for now dirty hack because everything is in DocumentView -> MainWindow
	auto app = qApp->findChild<iscore::Application*>("Application");
	auto scenarios = app->view()->findChildren<ScenarioModel*>("ScenarioModel");
	
	auto scenar_p = std::find_if(
						std::begin(scenarios),
						std::end(scenarios),
						[&] (ScenarioModel* model)
						{ return model->id() == m_modelId; });
	
	if(scenar_p != std::end(scenarios))
	{
		// @todo removeTimeEvent required
		(*scenar_p)->removeTimeEvent(m_position);
	}
}

void CreatEventCommand::redo()
{
	qDebug(Q_FUNC_INFO);
	
	// Proper way should be : 
	// auto scenarios = qApp->findChildren<ScenarioModel*>("ScenarioModel");
	// But for now dirty hack because everything is in DocumentView -> MainWindow
	auto app = qApp->findChild<iscore::Application*>("Application");
	auto scenarios = app->view()->findChildren<ScenarioModel*>("ScenarioModel");
	
	
	qDebug() << scenarios.size();
	auto scenar_p = std::find_if(
						std::begin(scenarios),
						std::end(scenarios),
						[&] (ScenarioModel* model)
						{qDebug() << model->id();  return model->id() == m_modelId; });
	
	if(scenar_p != std::end(scenarios))
	{
		qDebug("1");
		(*scenar_p)->addTimeEvent(m_position);
		// @todo Is it useful to do error checking here ?
	}
}

int CreatEventCommand::id() const
{
	return 1;
}

bool CreatEventCommand::mergeWith(const QUndoCommand* other)
{
}

void CreatEventCommand::serializeImpl(QDataStream& s)
{
	s << m_modelId << m_position;
}

void CreatEventCommand::deserializeImpl(QDataStream& s)
{
	s >> m_modelId >> m_position;
}
