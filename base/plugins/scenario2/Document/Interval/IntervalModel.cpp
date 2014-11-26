#include "IntervalModel.hpp"
#include <interface/process/ProcessSharedModelInterface.hpp>
#include "IntervalContent/IntervalContentModel.hpp"

#include <QApplication>
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>

#include <utilsCPP11.hpp>
#include <API/Headers/Editor/TimeBox.h>

IntervalModel::IntervalModel(int id, 
							 QObject* parent):
	QIdentifiedObject{parent, "IntervalModel", id},
	m_timeBox{new OSSIA::TimeBox}
{
	
}

//// Complex commands
int IntervalModel::createProcess(QString processName)
{
	auto processFactory = iscore::ProcessList::getFactory(processName);
	
	if(processFactory)
	{
		auto model = processFactory->makeModel(m_nextProcessId++, this);
		m_processes.push_back(model);
		emit processCreated(processName, model->id());
		
		return model->id();
	}
	
	return -1;
}
int IntervalModel::createProcess(QString processName, QByteArray data)
{
	auto processFactory = iscore::ProcessList::getFactory(processName);
	
	if(processFactory)
	{
		auto model = processFactory->makeModel(m_nextProcessId++, data, this);
		m_processes.push_back(model);
		emit processCreated(processName, model->id());
		
		return model->id();
	}
	
	return -1;
}

void IntervalModel::deleteProcess(int processId)
{
	emit processDeleted(processId);
	removeById(m_processes, 
			   processId);
	
	m_nextProcessId--;
}


void IntervalModel::createContentModel()
{
	auto content = new IntervalContentModel{m_nextContentId++, this};
	m_contentModels.push_back(content);
	emit viewCreated(content->id());
}

void IntervalModel::deleteContentModel(int viewId)
{
	emit viewDeleted(viewId);
	vec_erase_remove_if(m_contentModels, 
					   [&viewId] (IntervalContentModel* model) 
						  { 
							  bool to_delete = model->id() == viewId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
}

void IntervalModel::duplicateContentModel(int viewId)
{
	// TODO
}

EventModel* IntervalModel::startEvent()
{
	return m_startEvent;
}

EventModel* IntervalModel::endEvent()
{
	return m_endEvent;
}

IntervalContentModel*IntervalModel::contentModel(int contentId)
{
	return findById(m_contentModels, contentId);
}

iscore::ProcessSharedModelInterface* IntervalModel::process(int processId)
{
	return findById(m_processes, processId);
}



//// Simple properties
QString IntervalModel::name() const
{
	return m_name;
}

QString IntervalModel::comment() const
{
	return m_comment;
}

QColor IntervalModel::color() const
{
	return m_color;
}

void IntervalModel::setName(QString arg)
{
	if (m_name == arg)
		return;
	
	m_name = arg;
	emit nameChanged(arg);
}

void IntervalModel::setComment(QString arg)
{
	if (m_comment == arg)
		return;
	
	m_comment = arg;
	emit commentChanged(arg);
}

void IntervalModel::setColor(QColor arg)
{
	if (m_color == arg)
		return;
	
	m_color = arg;
	emit colorChanged(arg);
}
