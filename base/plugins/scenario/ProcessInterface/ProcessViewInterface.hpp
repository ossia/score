#pragma once
#include <QGraphicsObject>

class ProcessViewInterface : public QGraphicsObject
{
	public:
		using QGraphicsObject::QGraphicsObject;

		virtual QRectF boundingRect() const override
		{ return {0, 0, m_width, m_height}; }

		void setHeight(int height)
		{
			prepareGeometryChange();
			m_height = qreal(height);
		}

		int height() const
		{ return int(m_height); }

		void setWidth(int width)
		{
			prepareGeometryChange();
			m_width = qreal(width);
		}

		int width() const
		{ return int(m_width); }

	private:
		qreal m_height{};
		qreal m_width{};
};
