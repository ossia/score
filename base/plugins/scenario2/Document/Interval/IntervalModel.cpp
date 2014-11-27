#include "IntervalModel.hpp"
#include <interface/process/ProcessSharedModelInterface.hpp>
#include "IntervalContent/IntervalContentModel.hpp"

#include <QApplication>
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>

#include <utilsCPP11.hpp>
#include <API/Headers/Editor/TimeBox.h>
#include <QDebug>
QDataStream& operator <<(QDataStream& s, const IntervalModel& i) 
{
	qDebug() << Q_FUNC_INFO;
	// Metadata
	s	<< i.id() 
		<< i.name()
		<< i.comment()
		<< i.color();
	
	// Content Views
	s	<<  (int) i.m_contentModels.size();
	for(auto& content : i.m_contentModels)
	{
		s << *content;
	}
	s	<< i.m_nextContentId;
	
	// Processes
	s	<< (int) i.m_processes.size();
	for(auto& process : i.m_processes)
	{
		s << *process;
	}
	s	<< i.m_nextProcessId;
	
	// API Object
	// s << i.apiObject()->save();
}

IntervalModel::IntervalModel(int id, 
							 QObject* parent):
	QIdentifiedObject{parent, "IntervalModel", id},
	m_timeBox{new OSSIA::TimeBox}
{
	createContentModel();
}

//// Complex commands
int IntervalModel::createProcess(QString processName)
{
	qDebug() << m_nextProcessId;
	auto processFactory = iscore::ProcessList::getFactory(processName);
	
	if(processFactory)
	{
		auto model = processFactory->makeModel(m_nextProcessId, this);
		m_nextProcessId++;
		
		m_processes.push_back(model);
		emit processCreated(processName, model->id());
		
		return model->id();
	}
	qDebug() << "FAIL: " << Q_FUNC_INFO;
	return -1;
}
int IntervalModel::createProcess(QString processName, QByteArray data)
{
	auto processFactory = iscore::ProcessList::getFactory(processName);
	
	if(processFactory)
	{
		auto model = processFactory->makeModel(m_nextProcessId, data, this);
		m_nextProcessId++;
				
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
	connect(this,	 &IntervalModel::processDeleted, 
			content, &IntervalContentModel::on_deleteSharedProcessModel);
	
	m_contentModels.push_back(content);
	emit viewCreated(content->id());
}

void IntervalModel::deleteContentModel(int viewId)
{
	emit viewDeleted(viewId);
	removeById(m_contentModels, 
			   viewId);
	
	m_nextContentId--;
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

void IntervalModel::setStartEvent(EventModel* e)
{
	m_startEvent = e;
}

void IntervalModel::setEndEvent(EventModel* e)
{
	m_endEvent = e;
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
