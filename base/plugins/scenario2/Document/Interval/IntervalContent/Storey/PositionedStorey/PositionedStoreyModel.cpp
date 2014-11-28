#include "PositionedStoreyModel.hpp"
#include <QDebug>

QDataStream& operator << (QDataStream& s, const PositionedStoreyModel& storey)
{
	qDebug() << "PositionedStoreyModel";
	s << static_cast<const StoreyModel&>(storey);

	s << storey.position();
}

PositionedStoreyModel::PositionedStoreyModel(QDataStream& s, IntervalContentModel* parent):
	StoreyModel{s, parent}
{
	int pos;
	s >> pos;
	setPosition(pos);
}

PositionedStoreyModel::PositionedStoreyModel(int position, int id, IntervalContentModel* parent):
	StoreyModel{id, parent},
	m_position{position}
{

}
