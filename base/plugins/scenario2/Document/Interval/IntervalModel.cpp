#include "IntervalModel.hpp"
using namespace std;
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
	
}

void IntervalModel::deleteProcess(int processId)
{
	emit processDeleted(processId);
	removeFromVectorIf(m_processes, 
					   [&processId] (ProcessSharedModel* model)  // TODO faire une macro pour recherche par id.
						  { 
							  bool to_delete = model->id() == processId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
}


void IntervalModel::createView()
{
	
}

void IntervalModel::deleteView(int viewId)
{
	emit viewDeleted(processId);
	removeFromVectorIf(m_contents, 
					   [&viewId] (IntervalContentModel* model) 
						  { 
							  bool to_delete = model->id() == viewId;
							  if(to_delete) delete model;
							  return to_delete; 
						  });
}

void IntervalModel::duplicateView(int viewId)
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
