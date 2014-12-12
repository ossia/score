#include "ConstraintContentModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Storey/PositionedStorey/PositionedStoreyModel.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>

QDataStream& operator << (QDataStream& s, const ConstraintContentModel& c)
{
	s << (int)c.m_storeys.size();
	for(auto& storey : c.m_storeys)
	{
		s << *storey;
	}

	return s;
}

QDataStream& operator >> (QDataStream& s, ConstraintContentModel& c)
{
	int storeys_size;
	s >> storeys_size;
	for(; storeys_size --> 0 ;)
	{
		c.createStorey(s);
	}

	return s;
}


ConstraintContentModel::ConstraintContentModel(int id, ConstraintModel* parent):
	IdentifiedObject{id, "ConstraintContentModel", parent}
{

}

ConstraintContentModel::ConstraintContentModel(QDataStream& s, ConstraintModel* parent):
	IdentifiedObject{s, "ConstraintContentModel", parent}
{
	s >> *this;
}

int ConstraintContentModel::createStorey(int newStoreyId)
{
	return createStorey_impl(
				new PositionedStoreyModel{(int) m_storeys.size(),
										  newStoreyId,
										  this});

}

int ConstraintContentModel::createStorey(QDataStream& s)
{
	return createStorey_impl(
				new PositionedStoreyModel{s,
										  this});
}

int ConstraintContentModel::createStorey_impl(PositionedStoreyModel* storey)
{
	connect(this,	&ConstraintContentModel::on_deleteSharedProcessModel,
			storey, &PositionedStoreyModel::on_deleteSharedProcessModel);
	m_storeys.push_back(storey);

	emit storeyCreated(storey->id());
	return storey->id();
}


void ConstraintContentModel::deleteStorey(int storeyId)
{
	emit storeyDeleted(storeyId);

	removeById(m_storeys, storeyId);
}

void ConstraintContentModel::changeStoreyOrder(int storeyId, int position)
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

PositionedStoreyModel* ConstraintContentModel::storey(int storeyId) const
{
	return findById(m_storeys, storeyId);
}

void ConstraintContentModel::duplicateStorey()
{
	qDebug() << Q_FUNC_INFO << "TODO";
}


