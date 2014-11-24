#include "AddProcessToIntervalCommand.hpp"
#include <Document/Interval/IntervalModel.hpp>

using namespace iscore;


void AddProcessToIntervalCommand::undo()
{
}

void AddProcessToIntervalCommand::redo()
{
	/*
	qDebug(Q_FUNC_INFO);
	
	// Proper way should be : 
	// auto scenarios = qApp->findChildren<ScenarioModel*>("ScenarioModel");
	// But for now dirty hack because everything is in DocumentView -> MainWindow
	auto app = qApp->findChild<iscore::Application*>("Application");
	auto intervals = app->view()->findChildren<IntervalModel*>("IntervalModel");
	
	
	qDebug() << intervals.size();
	auto model_p = std::find_if(
						std::begin(intervals),
						std::end(intervals),
						[&] (IntervalModel* model)
	{qDebug() << model->id();  return model->id() == m_modelId; });
	
	if(model_p != std::end(intervals))
	{
		qDebug("1");
		(*model_p)->addTimeEvent(m_timeEventId, m_position);  // TODO what about serialization ??
		// @todo Is it useful to do error checking here ?
	}
	*/
}

int AddProcessToIntervalCommand::id() const
{
}

bool AddProcessToIntervalCommand::mergeWith(const QUndoCommand* other)
{
}

void AddProcessToIntervalCommand::serializeImpl(QDataStream&)
{
}

void AddProcessToIntervalCommand::deserializeImpl(QDataStream&)
{
}
