#pragma once
#include <QGraphicsObject>

class BoxView : public QGraphicsObject
{
	Q_OBJECT

	public:
		BoxView(QGraphicsObject* parent);
		virtual ~BoxView() = default;


		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

		void setHeight(int height)
		{
			m_height = height;
		}

	private:
		int m_height{};
};
