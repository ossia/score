#include "IntervalModel.hpp"
#include <interface/process/ProcessSharedModelInterface.hpp>
#include "IntervalContent/IntervalContentModel.hpp"
#include <Document/Event/EventModel.hpp>

#include <QApplication>
#include <core/processes/ProcessList.hpp>
#include <interface/process/ProcessFactoryInterface.hpp>

#include <tools/utilsCPP11.hpp>
#include <API/Headers/Editor/TimeBox.h>
#include <QDebug>

QDataStream& operator <<(QDataStream& s, const IntervalModel& i)
{
	// Metadata
	s	<< i.name()
		<< i.comment()
		<< i.color();

	// Processes
	s	<< (int) i.m_processes.size();
	for(auto& process : i.m_processes)
	{
		s << process->processName();
		s << *process;
	}

	// Contents
	s	<<  (int) i.m_contentModels.size();
	for(auto& content : i.m_contentModels)
	{
		s << *content;
	}

	// Events
	s	<< i.m_startEvent;
	s	<< i.m_endEvent;

	// API Object
	// s << i.apiObject()->save();
	return s;
}


QDataStream& operator >>(QDataStream& s, IntervalModel& interval)
{
	// Metadata
	QString name;
	QString comment;
	QColor color;
	s >> name >> comment >> color;
	interval.setName(name);
	interval.setComment(comment);
	interval.setColor(color);

	// Processes
	int process_size;
	s >> process_size;
	for(int i = 0; i < process_size; i++)
	{
		QString name;
		s >> name;
		interval.createProcess(name, s);
	}

	// Contents
	int content_models_size;
	s >> content_models_size;
	for(int i = 0; i < content_models_size; i++)
	{
		interval.createContentModel(s);
	}

	// Events
	s >> interval.m_startEvent;
	s >> interval.m_endEvent;

	return s;
}

IntervalModel::IntervalModel(QDataStream& s, QObject* parent):
	IdentifiedObject{s, "IntervalModel", parent} // Id has to be set afterwards
{
	s >> *this;
}



int IntervalModel::width() const
{
	return m_width;
}

void IntervalModel::setWidth(int width)
{
	m_width = width;
}

int IntervalModel::height() const
{
	return m_height;
}

void IntervalModel::setHeight(int height)
{
	m_height = height;
}

IntervalModel::IntervalModel(int id,
							 QObject* parent):
	IdentifiedObject{id, "IntervalModel", parent},
	m_timeBox{new OSSIA::TimeBox}
{
	createContentModel();
}

IntervalModel::IntervalModel(int id, double yPos, QObject *parent):
	IntervalModel(id, parent)
{
	setHeightPercentage(yPos);
}

//// Complex commands
int IntervalModel::createProcess(QString processName)
{
	auto model = iscore::ProcessList::getFactory(processName)->makeModel(getNextId(m_processes), this);
	return createProcess_impl(model);
}

int IntervalModel::createProcess(QString processName, QDataStream& data)
{
	auto model = iscore::ProcessList::getFactory(processName)->makeModel(data, this);
	return createProcess_impl(model);
}

int IntervalModel::createProcess_impl(iscore::ProcessSharedModelInterface* model)
{
	m_processes.push_back(model);
	emit processCreated(model->processName(), model->id());

	return model->id();
}


void IntervalModel::deleteProcess(int processId)
{
	emit processDeleted(processId);
	removeById(m_processes,
			   processId);
}


void IntervalModel::createContentModel()
{
	auto content = new IntervalContentModel{getNextId(m_contentModels), this};
	createContentModel_impl(content);
}

void IntervalModel::createContentModel(QDataStream& s)
{
	auto content = new IntervalContentModel{s, this};
	createContentModel_impl(content);
}

void IntervalModel::createContentModel_impl(IntervalContentModel* content)
{
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
}

void IntervalModel::duplicateContentModel(int viewId)
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

int IntervalModel::startEvent()
{
	return m_startEvent;
}

int IntervalModel::endEvent()
{
	return m_endEvent;
}

void IntervalModel::setStartEvent(int e)
{
	m_startEvent = e;
}

void IntervalModel::setEndEvent(int e)
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
	return m_name + QString::number(id());
}

QString IntervalModel::comment() const
{
	return m_comment;
}

QColor IntervalModel::color() const
{
	return m_color;
}

double IntervalModel::heightPercentage() const
{
	return m_heightPercentage;
}

int IntervalModel::startDate() const
{
	return m_x;
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

void IntervalModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}
