#include "IntervalModel.hpp"
#include <interface/process/ProcessSharedModelInterface.hpp>
#include "IntervalContent/IntervalContentModel.hpp"

#include <QApplication>
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>

#include <utilsCPP11.hpp>

IntervalModel::IntervalModel(EventModel* beginEvent, 
							 EventModel* endEvent, 
							 int id, 
							 QObject* parent):
	QIdentifiedObject{parent, "IntervalModel", id},
	m_startEvent{beginEvent},
	m_endEvent{endEvent}
{
	
}

//// Complex commands
void IntervalModel::createProcess(QString processName)
{
	auto processFactory = iscore::ProcessList::getFactory(processName);
	
	if(processFactory)
	{
		auto model = processFactory->makeModel(m_nextProcessId++, this);
		m_processes.push_back(model);
		emit processCreated(processName, model->id());
	}
}

void IntervalModel::deleteProcess(int processId)
{
	emit processDeleted(processId);
	vec_erase_remove_if(m_processes, 
					   [&processId] (iscore::ProcessSharedModelInterface* model)  // TODO faire une macro pour recherche par id.
						  { 
							  bool to_delete = model->id() == processId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
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
	auto it = std::find_if(std::begin(m_contentModels),
						   std::end(m_contentModels),
						   [&contentId] (IntervalContentModel* model)
							{
							  return model->id() == contentId;
							});
	
	if(it != std::end(m_contentModels))
		return *it;
	
	return nullptr;
}

iscore::ProcessSharedModelInterface* IntervalModel::process(int processId)
{
	auto it = std::find_if(std::begin(m_processes),
						   std::end(m_processes),
						   [&processId] (iscore::ProcessSharedModelInterface* model)
							{
							  return model->id() == processId;
							});
	
	if(it != std::end(m_processes))
		return *it;
	
	return nullptr;
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
