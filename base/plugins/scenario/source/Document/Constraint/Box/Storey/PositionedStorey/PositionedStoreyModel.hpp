#pragma once
#include "Document/Constraint/Box/Storey/StoreyModel.hpp"

class BoxModel;
class PositionedStoreyModel : public StoreyModel
{
		Q_OBJECT
		Q_PROPERTY(int position
				   READ position
				   WRITE setPosition
				   NOTIFY positionChanged)

		friend QDataStream& operator << (QDataStream& s, const PositionedStoreyModel& c);
		friend QDataStream& operator >> (QDataStream& s, PositionedStoreyModel& c);

	public:
		PositionedStoreyModel(QDataStream& s, BoxModel* parent);
		PositionedStoreyModel(int position, int id, BoxModel* parent);

		int position() const
		{
			return m_position;
		}

	signals:
		void positionChanged(int arg);

	public slots:
		void setPosition(int arg)
		{
			if (m_position == arg)
				return;

			m_position = arg;
			emit positionChanged(arg);
		}

	private:
		int m_position{};
};
