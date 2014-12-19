#include "BoxModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Storey/StoreyModel.hpp"

#include <tools/utilsCPP11.hpp>

#include <QDebug>

QDataStream& operator << (QDataStream& s, const BoxModel& c)
{
	qDebug() << Q_FUNC_INFO << c.m_storeys.size();

	s << static_cast<const IdentifiedObject&>(c);
	s << (int)c.m_storeys.size();
	for(auto& storey : c.m_storeys)
	{
		s << *storey;
	}

	return s;
}

QDataStream& operator >> (QDataStream& s, BoxModel& c)
{
	int storeys_size;
	s >> storeys_size;
	qDebug() << Q_FUNC_INFO << storeys_size;
	for(; storeys_size --> 0 ;)
	{
		qDebug() << "Creating storey";
		c.createStorey(s);
	}

	return s;
}


BoxModel::BoxModel(int id, ConstraintModel* parent):
	IdentifiedObject{id, "BoxModel", parent}
{

}

BoxModel::BoxModel(QDataStream& s, ConstraintModel* parent):
	IdentifiedObject{s, "BoxModel", parent}
{
	qDebug(Q_FUNC_INFO);
	s >> *this;
}

int BoxModel::createStorey(int newStoreyId)
{
	return createStorey_impl(
				new StoreyModel{(int) m_storeys.size(),
								newStoreyId,
								this});

}

int BoxModel::createStorey(QDataStream& s)
{
	return createStorey_impl(
				new StoreyModel{s,
								this});
}

int BoxModel::createStorey_impl(StoreyModel* storey)
{
	connect(this,	&BoxModel::on_deleteSharedProcessModel,
			storey, &StoreyModel::on_deleteSharedProcessModel);
	connect(storey, &StoreyModel::storeyBecomesEmpty,
			this,	&BoxModel::on_storeyBecomesEmpty);
	m_storeys.push_back(storey);

	emit storeyCreated(storey->id());
	return storey->id();
}


void BoxModel::deleteStorey(int storeyId)
{
	auto deletedStorey = storey(storeyId);

	// Make the remaining storeys decrease their position.
	for(StoreyModel* storey : m_storeys)
	{
		auto pos = storey->position();
		if(pos > deletedStorey->position())
			storey->setPosition(pos - 1);
	}

	// Delete
	emit storeyDeleted(storeyId);
	removeById(m_storeys, storeyId);

	if(m_storeys.empty())
	{
		emit boxBecomesEmpty(id());
	}
}

void BoxModel::changeStoreyOrder(int storeyId, int position)
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

StoreyModel* BoxModel::storey(int storeyId) const
{
	return findById(m_storeys, storeyId);
}

void BoxModel::duplicateStorey()
{
	qDebug() << Q_FUNC_INFO << "TODO";
}

void BoxModel::on_storeyBecomesEmpty(int id)
{
	deleteStorey(id);
}


