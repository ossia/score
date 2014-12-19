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

QDataStream& operator <<(QDataStream& s, const ConstraintModel& i)
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
	s	<<  (int) i.m_boxes.size();
	for(auto& content : i.m_boxes)
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


QDataStream& operator >>(QDataStream& s, ConstraintModel& constraint)
{
	// Metadata
	QString name;
	QString comment;
	QColor color;
	s >> name >> comment >> color;
	constraint.setName(name);
	constraint.setComment(comment);
	constraint.setColor(color);


	///// TODO Box should be just this part.
	// Processes
	int process_size;
	s >> process_size;
	for(int i = 0; i < process_size; i++)
	{
		constraint.createProcess(s);
	}

	// Contents
	int content_models_size;
	s >> content_models_size;
	for(int i = 0; i < content_models_size; i++)
	{
		constraint.createBox(s);
	}

	// Events
	s >> constraint.m_startEvent;
	s >> constraint.m_endEvent;

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
	emit processRemoved(processId);
	removeById(m_processes,
			   processId);
}


void ConstraintModel::createBox(int boxId)
{
	auto box = new BoxModel{boxId, this};
	createContentModel_impl(box);
}

void ConstraintModel::createBox(QDataStream& s)
{
	qDebug(Q_FUNC_INFO);
	auto box = new BoxModel{s, this};
	createContentModel_impl(box);
}

void ConstraintModel::createContentModel_impl(BoxModel* box)
{
	connect(this,	&ConstraintModel::processRemoved,
			box,	&BoxModel::on_deleteSharedProcessModel);
	connect(box,	&BoxModel::boxBecomesEmpty,
			this,	&ConstraintModel::on_boxBecomesEmpty);

	m_boxes.push_back(box);
	emit boxCreated(box->id());
}


void ConstraintModel::removeBox(int viewId)
{
	emit boxRemoved(viewId);
	removeById(m_boxes,
			   viewId);
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

void ConstraintModel::on_boxBecomesEmpty(int id)
{
	removeBox(id);
}
