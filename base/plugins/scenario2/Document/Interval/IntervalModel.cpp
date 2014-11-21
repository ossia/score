#include "IntervalModel.hpp"
#include <interface/process/ProcessSharedModelInterface.hpp>
#include "IntervalContent/IntervalContentModel.hpp"

#include <QApplication>
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>
using namespace std;

iscore::ProcessFactoryInterface* getProcessFactory(QString processName)
{
	return qApp
			->findChild<iscore::ProcessList*>("ProcessList")
			->getProcess(processName);
}

IntervalModel::IntervalModel(QObject* parent):
	QNamedObject{parent, "IntervalModel"}
{
	
}

template <typename Vector, typename Functor>
void removeFromVectorIf(Vector& v, Functor&& f )
{
	v.erase(remove_if(begin(v), end(v), f), end(v));
}

//// Complex commands
void IntervalModel::createProcess(QString processName)
{
	auto processFactory = getProcessFactory(processName);
	
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
	removeFromVectorIf(m_processes, 
					   [&processId] (iscore::ProcessSharedModelInterface* model)  // TODO faire une macro pour recherche par id.
						  { 
							  bool to_delete = model->id() == processId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
}


void IntervalModel::createContentModel()
{
	auto content = new IntervalContentModel{this};
	emit viewCreated(content->id());
}

void IntervalModel::deleteContentModel(int viewId)
{
	emit viewDeleted(viewId);
	removeFromVectorIf(m_contentModel, 
					   [&viewId] (IntervalContentModel* model) 
						  { 
							  bool to_delete = model->id() == viewId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
}

void IntervalModel::duplicateContentModel(int viewId)
{
	
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
