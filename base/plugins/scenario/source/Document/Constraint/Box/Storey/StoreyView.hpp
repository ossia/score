#pragma once
#include <QGraphicsObject>

class StoreyView : public QGraphicsObject
{
	Q_OBJECT

	public:
		StoreyView(QGraphicsObject* parent);
		virtual ~StoreyView() = default;

		virtual QRectF boundingRect() const override;
		virtual void paint(QPainter *painter,
						   const QStyleOptionGraphicsItem *option,
						   QWidget *widget) override;

		void setHeight(int height);
		int height() const;

	signals:
		void bottomHandleSelected();
		void bottomHandleChanged(int newHeight);
		void bottomHandleReleased();

	protected:
		virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
		virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
		virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

	private:
		int m_height{};
};

