#include "ConstraintModel.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Event/EventModel.hpp"

#include "Control/ProcessList.hpp"
#include <tools/utilsCPP11.hpp>
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <API/Headers/Editor/TimeBox.h>

#include <QDebug>
#include <QApplication>

QDataStream& operator <<(QDataStream& s, const ConstraintModel& constraint)
{
	// Metadata
	s	<< constraint.name()
		<< constraint.comment()
		<< constraint.color()
		<< constraint.heightPercentage();

	// Processes
	s	<< (int) constraint.m_processes.size();
	for(auto& process : constraint.m_processes)
	{
		s << process->processName();
		s << *process;
	}

	// Boxes
	s	<<  (int) constraint.m_boxes.size();
	for(auto& content : constraint.m_boxes)
	{
		s << *content;
	}

	// Events
	s	<< constraint.m_startEvent;
	s	<< constraint.m_endEvent;

	// API Object
	// s << i.apiObject()->save();
	// Things that should be queried from the API :
	s << constraint.m_width
	  << constraint.m_height
	  << constraint.m_x;

	return s;
}


QDataStream& operator >>(QDataStream& s, ConstraintModel& constraint)
{
	// Metadata
	QString name;
	QString comment;
	QColor color;
	double heightPercentage;
	s >> name >> comment >> color >> heightPercentage;
	constraint.setName(name);
	constraint.setComment(comment);
	constraint.setColor(color);
	constraint.setHeightPercentage(heightPercentage);

	// Processes
	int process_size;
	s >> process_size;
	for(int i = 0; i < process_size; i++)
	{
		constraint.createProcess(s);
	}

	// Boxes
	int content_models_size;
	s >> content_models_size;
	for(int i = 0; i < content_models_size; i++)
	{
		constraint.createBox(s);
	}

	// Events
	s >> constraint.m_startEvent;
	s >> constraint.m_endEvent;

	// Things that should be queried from the API :
	s >> constraint.m_width
	  >> constraint.m_height
	  >> constraint.m_x;

	return s;
}

ConstraintModel::ConstraintModel(QDataStream& s, QObject* parent):
	IdentifiedObject{s, "ConstraintModel", parent} // Id has to be set afterwards
{
	s >> *this;
}



int ConstraintModel::width() const
{
	return m_width;
}

void ConstraintModel::setWidth(int width)
{
	m_width = width;
}

int ConstraintModel::height() const
{
	return m_height;
}

void ConstraintModel::setHeight(int height)
{
	m_height = height;
}

ConstraintModel::ConstraintModel(int id,
							 QObject* parent):
	IdentifiedObject{id, "ConstraintModel", parent},
	m_timeBox{new OSSIA::TimeBox}
{
}

ConstraintModel::ConstraintModel(int id, double yPos, QObject *parent):
	ConstraintModel(id, parent)
{
	setHeightPercentage(yPos);
}

//// Complex commands
int ConstraintModel::createProcess(QString processName, int processId)
{
	auto model = ProcessList::getFactory(processName)->makeModel(processId, this);
	return createProcess_impl(model);
}

int ConstraintModel::createProcess(QDataStream& data)
{
	QString processName;
	data >> processName;
	auto model = ProcessList::getFactory(processName)->makeModel(data, this);
	return createProcess_impl(model);
}

int ConstraintModel::createProcess_impl(ProcessSharedModelInterface* model)
{
	m_processes.push_back(model);
	emit processCreated(model->processName(), model->id());

	return model->id();
}


void ConstraintModel::removeProcess(int processId)
{
	removeById(m_processes,
			   processId);

	emit processRemoved(processId);
}


void ConstraintModel::createBox(int boxId)
{
	auto box = new BoxModel{boxId, this};
	createContentModel_impl(box);
}

void ConstraintModel::createBox(QDataStream& s)
{
	auto box = new BoxModel{s, this};
	createContentModel_impl(box);
}

void ConstraintModel::createContentModel_impl(BoxModel* box)
{
	connect(this,	&ConstraintModel::processRemoved,
			box,	&BoxModel::on_deleteSharedProcessModel);

	m_boxes.push_back(box);
	emit boxCreated(box->id());
}


void ConstraintModel::removeBox(int viewId)
{
	removeById(m_boxes,
			   viewId);

	emit boxRemoved(viewId);
}

void ConstraintModel::duplicateBox(int viewId)
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

int ConstraintModel::startEvent()
{
	return m_startEvent;
}

int ConstraintModel::endEvent()
{
	return m_endEvent;
}

void ConstraintModel::setStartEvent(int e)
{
	m_startEvent = e;
}

void ConstraintModel::setEndEvent(int e)
{
	m_endEvent = e;
}

BoxModel*ConstraintModel::box(int contentId)
{
	return findById(m_boxes, contentId);
}

ProcessSharedModelInterface* ConstraintModel::process(int processId)
{
	return findById(m_processes, processId);
}



//// Simple properties
QString ConstraintModel::name() const
{
	return m_name + QString::number(id());
}

QString ConstraintModel::comment() const
{
	return m_comment;
}

QColor ConstraintModel::color() const
{
	return m_color;
}

double ConstraintModel::heightPercentage() const
{
	return m_heightPercentage;
}

int ConstraintModel::startDate() const
{
	return m_x;
}

void ConstraintModel::setStartDate(int start)
{
	m_x = start;
}

void ConstraintModel::translate(int deltaTime)
{
	m_x += deltaTime;
}

void ConstraintModel::setName(QString arg)
{
	if (m_name == arg)
		return;

	m_name = arg;
	emit nameChanged(arg);
}

void ConstraintModel::setComment(QString arg)
{
	if (m_comment == arg)
		return;

	m_comment = arg;
	emit commentChanged(arg);
}

void ConstraintModel::setColor(QColor arg)
{
	if (m_color == arg)
		return;

	m_color = arg;
	emit colorChanged(arg);
}

void ConstraintModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}
