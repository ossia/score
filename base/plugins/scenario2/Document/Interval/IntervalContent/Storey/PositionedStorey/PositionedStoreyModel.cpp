#include "PositionedStoreyModel.hpp"
#include <QDebug>

QDataStream& operator << (QDataStream& s, const PositionedStoreyModel& storey)
{
	qDebug() << "PositionedStoreyModel";
	s << static_cast<const StoreyModel&>(storey);
	
	s << storey.position();
}