#include "PositionedStoreyModel.hpp"
#include <QDebug>

QDataStream& operator << (QDataStream& s, const PositionedStoreyModel& storey)
{
	s << static_cast<const StoreyModel&>(storey);
	s << storey.position();

	return s;
}

QDataStream& operator >> (QDataStream& s, PositionedStoreyModel& storey)
{
	int pos;
	s >> pos;
	storey.setPosition(pos);
}

PositionedStoreyModel::PositionedStoreyModel(QDataStream& s, IntervalContentModel* parent):
	StoreyModel{s, parent}
{
	s >> *this;
}

PositionedStoreyModel::PositionedStoreyModel(int position, int id, IntervalContentModel* parent):
	StoreyModel{id, parent},
	m_position{position}
{

}
