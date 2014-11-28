#include "PositionedStoreyModel.hpp"
#include <QDebug>

QDataStream& operator << (QDataStream& s, const PositionedStoreyModel& storey)
{
	s << static_cast<const StoreyModel&>(storey);
	qDebug(Q_FUNC_INFO);

	s << storey.position();
}

PositionedStoreyModel::PositionedStoreyModel(QDataStream& s, IntervalContentModel* parent):
	StoreyModel{s, parent}
{
	qDebug(Q_FUNC_INFO);
	int pos;
	s >> pos;
	setPosition(pos);
}

PositionedStoreyModel::PositionedStoreyModel(int position, int id, IntervalContentModel* parent):
	StoreyModel{id, parent},
	m_position{position}
{

}
