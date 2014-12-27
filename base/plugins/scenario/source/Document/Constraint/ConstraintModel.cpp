#include "ConstraintModel.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Event/EventModel.hpp"

#include "Control/ProcessList.hpp"
#include <tools/utilsCPP11.hpp>
#include "ProcessInterface/ProcessFactoryInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include <API/Headers/Editor/TimeBox.h>

#include <QDebug>
#include <QApplication>


/*
ConstraintModel::ConstraintModel(QDataStream& s, QObject* parent):
	IdentifiedObject{s, parent}
{
	s >> *this;
}
*/
void ConstraintModel::makeViewModel_impl(AbstractConstraintViewModel* viewmodel) const
{
	connect(this,		&ConstraintModel::boxRemoved,
			viewmodel,	&AbstractConstraintViewModel::on_boxRemoved);
}





int ConstraintModel::width() const
{
	return m_width;
}

void ConstraintModel::setWidth(int width)
{
	m_width = width;
}

ConstraintModel::ConstraintModel(int id,
								 QObject* parent):
	IdentifiedObject{id, "ConstraintModel", parent},
	m_timeBox{new OSSIA::TimeBox}
{
	metadata.setName(QString("Constraint.%1").arg(this->id()));
}

ConstraintModel::ConstraintModel(int id, double yPos, QObject *parent):
	ConstraintModel{id, parent}
{
	setHeightPercentage(yPos);
}

//// Complex commands
void ConstraintModel::createProcess(QString processName, int processId)
{
	auto model = ProcessList::getFactory(processName)->makeModel(processId, this);
	addProcess(model);
}



void ConstraintModel::addProcess(ProcessSharedModelInterface* model)
{
	m_processes.push_back(model);
	emit processCreated(model->processName(), model->id());
}


// TODO use this pattern everywhere to prevent problems.
void ConstraintModel::removeProcess(int processId)
{
	auto proc = process(processId);
	vec_erase_remove_if(m_processes,
						[&processId] (ProcessSharedModelInterface* model)
						{ return model->id() == processId; });

	emit processRemoved(processId);
	delete proc;
}


void ConstraintModel::createBox(int boxId)
{
	auto box = new BoxModel{boxId, this};
	addBox(box);
}

void ConstraintModel::createBox(QDataStream& s)
{
	auto box = new BoxModel{s, this};
	addBox(box);
}

void ConstraintModel::addBox(BoxModel* box)
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

int ConstraintModel::startEvent() const
{
	return m_startEvent;
}

int ConstraintModel::endEvent() const
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

BoxModel*ConstraintModel::box(int contentId) const
{
	return findById(m_boxes, contentId);
}

ProcessSharedModelInterface* ConstraintModel::process(int processId) const
{
	return findById(m_processes, processId);
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


double ConstraintModel::heightPercentage() const
{
	return m_heightPercentage;
}



void ConstraintModel::setHeightPercentage(double arg)
{
	if (m_heightPercentage != arg) {
		m_heightPercentage = arg;
		emit heightPercentageChanged(arg);
	}
}
